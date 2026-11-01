# NEXUS demonstration walkthrough

This walkthrough is the evaluator-facing sequence for the final demonstration. The automated Windows evidence is recorded in `docs/windows_packaging.md`; the manual desktop steps below still require a real Windows session and screenshots.

## 1. Start and establish the baseline

1. On macOS, run `cmake --build build-macos -j2`, `ctest --test-dir build-macos --output-on-failure`, `npm run build`, and `python3 scripts/api_integration_test.py` against the running local API.
2. On Windows, extract the verified GitHub Actions package and double-click `Launch.exe`.
3. Record the candidate ID, timestamp, OS version, package SHA-256, and the URL opened by the launcher.
4. Capture the dashboard showing the live backend status and the navigation bar.

## 2. Demonstrate the engineering modules

1. Open Thermal Studio, run the default rod case, show nodal temperatures, the analytical comparison, midpoint error, convergence table, and CSV export.
2. Open Geometry Lab, import the bundled sample STL and the ANSYS-compatible CSV fixture, show the mesh/statistics result, and export the result. Keep the fixture label visible: it is a representative test input, not a claim about a measured component.
3. Open Performance Lab, run the default benchmark, and show launch-to-ready time, throughput, peak memory, and the cancellation/status behavior.
4. Open Neural Lab, show the forward pass, manual gradients, finite-difference PASS table, and the decreasing loss history. Attach the separately handwritten derivation described in `docs/handwritten_backprop_guide.md`.
5. Open Hardware Studio, show device discovery, simulated connection, telemetry, the deterministic timeout/fault transition, recovery, and the `simulated: true` indication. Do not present this as a physical hardware test.

## 3. Required evidence

For every module capture: the input panel, result panel, status indicator, and one exported artifact where applicable. Include the browser URL and a terminal/API response proving that the visible values came from the backend. Do not edit screenshots to manufacture results.

## 4. Clean-machine and offline checks

On a separate Windows machine or an isolated Windows VM, launch the package without Node.js, Vite, or repository files installed. Disconnect the network, repeat the launch and each module smoke test, close the browser, and check Task Manager for an orphaned `nexus-api.exe`. Record any port conflict or missing runtime dependency. These checks are not covered by hosted CI run #5 and remain outstanding until performed.
