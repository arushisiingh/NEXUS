#!/usr/bin/env python3
"""Live NEXUS API integration checks; requires nexus-api to be running."""
import json
import os
import sys
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError

BASE = os.environ.get(
    "NEXUS_API_BASE_URL",
    f"http://127.0.0.1:{os.environ.get('NEXUS_API_PORT', '8081')}",
).rstrip("/")
ROOT = Path(__file__).resolve().parents[1]

def request(path, method="GET", payload=None):
    data = None if payload is None else json.dumps(payload).encode()
    req = Request(BASE + path, data=data, method=method, headers={"Content-Type": "application/json", "Accept": "application/json"})
    try:
        with urlopen(req, timeout=15) as response:
            body = response.read()
            try:
                body = json.loads(body)
            except json.JSONDecodeError:
                body = body.decode(errors="replace")
            return response.status, body
    except HTTPError as error:
        body = error.read()
        try:
            body = json.loads(body)
        except json.JSONDecodeError:
            body = body.decode(errors="replace")
        return error.code, body

def check(condition, message):
    if not condition:
        raise AssertionError(message)
    print(f"PASS: {message}")

def main():
    status, health = request("/api/health")
    check(status == 200 and health["api_status"] == "ok", "health endpoint")
    status, root = request("/")
    check(status == 200 and isinstance(root, str) and 'id="root"' in root, "static frontend root endpoint")
    stl = (ROOT / "data/geometry/sample_cube.stl").read_text()
    status, geometry = request("/api/geometry/stl/parse", "POST", {"filename": "sample_cube.stl", "content": stl})
    check(status == 200 and geometry["triangle_count"] == 4, "geometry STL endpoint")
    status, thermal = request("/api/thermal/solve", "POST", {"element_count": 10})
    check(status == 200 and abs(thermal["maximum_absolute_midpoint_error_C"] - 1.25) < 1e-10, "thermal endpoint and expected error")
    status, performance = request("/api/performance/benchmark", "POST", {"case_count": 8, "mesh_size": 10, "seed": 7, "worker_counts": [1, 2]})
    check(status == 200 and performance["numerical_agreement"]["agreement"], "performance endpoint and numerical agreement")
    status, neural = request("/api/neural/backpropagation", "POST", {"additional_training_steps": 100})
    check(status == 200 and neural["gradient_verification_passed"] and neural["final_loss"] < neural["updated_loss"], "neural endpoint and gradient check")
    status, devices = request("/api/hardware/discover", "POST", {})
    check(status == 200 and len(devices["devices"]) == 3 and devices["simulated"], "hardware discovery")
    device_id = "arduino-nano-rp2040-connect"
    check(request("/api/hardware/connect", "POST", {"device_id": device_id})[1]["device"]["status"] == "connected", "hardware connect")
    check(request("/api/hardware/simulate-failure", "POST", {"device_id": device_id})[1]["device"]["status"] == "fault", "hardware timeout simulation")
    check(request("/api/hardware/recover", "POST", {"device_id": device_id})[1]["device"]["status"] == "connected", "hardware recovery")
    status, invalid = request("/api/hardware/connect", "POST", {"device_id": "missing-device"})
    check(status == 400 and invalid["error"] == "invalid_request", "invalid hardware device ID")
    print("API integration: PASS")

if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"API integration: FAIL — {error}", file=sys.stderr)
        raise SystemExit(1)
