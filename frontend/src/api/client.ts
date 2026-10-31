export type HealthResponse = {
  application_name: string;
  application_version: string;
  backend_status: 'online' | 'offline' | string;
  cpp_standard: string;
  api_status: string;
};

export type ThermalParameters = {
  length_m: number;
  area_m2: number;
  conductivity_W_mK: number;
  heat_generation_W_m3: number;
  left_temperature_C: number;
  right_temperature_C: number;
  element_count: number;
};

export type ThermalResponse = {
  status: 'success';
  input_parameters: ThermalParameters;
  nodal_positions_m: number[];
  nodal_temperatures_C: number[];
  analytical_temperatures_C: number[];
  midpoint_positions_m: number[];
  fem_midpoint_temperatures_C: number[];
  analytical_midpoint_temperatures_C: number[];
  midpoint_absolute_errors_C: number[];
  maximum_absolute_midpoint_error_C: number;
  execution_duration_ms: number;
};

export type Vector3 = [number, number, number];
export type GeometryTriangle = { normal: Vector3; vertices: [Vector3, Vector3, Vector3] };
export type GeometryResponse = {
  status: 'success';
  filename: string;
  triangle_count: number;
  bounding_box: { minimum: Vector3; maximum: Vector3 };
  dimensions_m: Vector3;
  processing_duration_ms: number;
  triangles: GeometryTriangle[];
};
export type TemperatureNode = { node_id: number; position_m: Vector3; temperature_C: number };
export type TemperatureResponse = {
  status: 'success';
  node_count: number;
  minimum_temperature_C: number;
  maximum_temperature_C: number;
  average_temperature_C: number;
  bounding_box: { minimum: Vector3; maximum: Vector3 };
  processing_duration_ms: number;
  nodes: TemperatureNode[];
};

export type PerformanceConfig = { case_count: number; mesh_size: number; seed: number; worker_counts: number[] };
export type PerformanceWorkerTiming = { worker_count: number; median_ms: number; measured_runs_ms: number[]; speedup: number; efficiency: number };
export type PerformanceResponse = {
  status: 'success';
  execution_status: string;
  progress: number;
  workload: { seed: number; case_count: number; mesh_size: number; workload_generation_ms: number };
  serial: { median_ms: number; measured_runs_ms: number[] };
  parallel: PerformanceWorkerTiming[];
  cpu: { model: string; physical_cores: number; logical_cores: number; total_ram_bytes: number; compiler: string; optimization_flags: string };
  numerical_agreement: { maximum_absolute_temperature_difference_C: number; agreement: boolean; cases_compared: number; failed_case_ids: number[] };
  launch_to_ready_ms: number;
  execution_duration_ms: number;
};

export type NeuralParameters = { w: number; b: number; v: number; c: number };
export type NeuralResponse = {
  status: 'success';
  input: { x: number; target: number; learning_rate: number; additional_training_steps: number };
  forward_pass: { z: number; h: number; prediction: number; prediction_error: number; initial_loss: number };
  initial_parameters: NeuralParameters;
  gradients: { dJ_dw: number; dJ_db: number; dJ_dv: number; dJ_dc: number };
  updated_parameters: NeuralParameters;
  updated_loss: number;
  gradient_verification_passed: boolean;
  finite_difference: { parameter: string; analytical_gradient: number; numerical_gradient: number; absolute_difference: number; pass: boolean }[];
  training_history: { step: number; loss: number }[];
  final_parameters: NeuralParameters;
  final_loss: number;
  execution_duration_ms: number;
};
export type HardwareDevice = { device_id: string; device_type: string; manufacturer: string; model: string; simulated: boolean; status: string; supply_voltage_V: number; io_voltage_V: number; interfaces: string[]; capabilities: string[]; connector: string; jtag_compatibility: string; error: string };
export type HardwareTelemetry = { sequence: number; device_id: string; level: string; message: string; status: string; voltage_V: number; temperature_C: number; simulated: boolean };
export type HardwareDevicesResponse = { status: 'success'; simulated: boolean; devices: HardwareDevice[] };
export type HardwareTelemetryResponse = { status: 'success'; simulated: boolean; events: HardwareTelemetry[] };

const API_BASE_URL = (import.meta.env.VITE_API_BASE_URL || '/api').replace(/\/$/, '');
const REQUEST_TIMEOUT_MS = 4000;

export class ApiClientError extends Error {
  constructor(message: string, public readonly kind: 'timeout' | 'connection' | 'http' | 'malformed') {
    super(message);
    this.name = 'ApiClientError';
  }
}

export async function getHealth(): Promise<HealthResponse> {
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);

  try {
    let response: Response;
    try {
      response = await fetch(`${API_BASE_URL}/health`, {
        method: 'GET',
        headers: { Accept: 'application/json' },
        signal: controller.signal,
      });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        throw new ApiClientError(`Health request timed out after ${REQUEST_TIMEOUT_MS} ms`, 'timeout');
      }
      throw new ApiClientError('Unable to connect to the NEXUS C++ backend', 'connection');
    }

    if (!response.ok) {
      throw new ApiClientError(`Health request failed with HTTP ${response.status}`, 'http');
    }

    let payload: unknown;
    try {
      payload = await response.json();
    } catch {
      throw new ApiClientError('Backend returned a non-JSON health response', 'malformed');
    }

    if (!payload || typeof payload !== 'object') {
      throw new ApiClientError('Backend health response has an invalid shape', 'malformed');
    }
    const health = payload as Partial<HealthResponse>;
    if (typeof health.application_name !== 'string' ||
        typeof health.application_version !== 'string' ||
        typeof health.backend_status !== 'string' ||
        typeof health.cpp_standard !== 'string' ||
        typeof health.api_status !== 'string') {
      throw new ApiClientError('Backend health response is missing required fields', 'malformed');
    }
    return health as HealthResponse;
  } finally {
    window.clearTimeout(timeout);
  }
}

export async function solveThermal(parameters: ThermalParameters): Promise<ThermalResponse> {
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  try {
    let response: Response;
    try {
      response = await fetch(`${API_BASE_URL}/thermal/solve`, {
        method: 'POST',
        headers: { Accept: 'application/json', 'Content-Type': 'application/json' },
        body: JSON.stringify(parameters),
        signal: controller.signal,
      });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') {
        throw new ApiClientError(`Thermal request timed out after ${REQUEST_TIMEOUT_MS} ms`, 'timeout');
      }
      throw new ApiClientError('Unable to connect to the NEXUS thermal solver', 'connection');
    }
    let payload: unknown;
    try {
      payload = await response.json();
    } catch {
      throw new ApiClientError('Thermal solver returned a non-JSON response', 'malformed');
    }
    if (!response.ok) {
      const message = payload && typeof payload === 'object' && 'message' in payload
        ? String((payload as { message: unknown }).message)
        : `Thermal request failed with HTTP ${response.status}`;
      throw new ApiClientError(message, 'http');
    }
    if (!payload || typeof payload !== 'object' || (payload as Partial<ThermalResponse>).status !== 'success') {
      throw new ApiClientError('Thermal solver returned an invalid response shape', 'malformed');
    }
    return payload as ThermalResponse;
  } finally {
    window.clearTimeout(timeout);
  }
}

async function parseUploadedFile<T>(path: string, filename: string, content: string): Promise<T> {
  if (new Blob([content]).size > 2 * 1024 * 1024) throw new ApiClientError('File exceeds the 2 MiB upload limit', 'http');
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), REQUEST_TIMEOUT_MS);
  try {
    let response: Response;
    try {
      response = await fetch(API_BASE_URL + path, {
        method: 'POST',
        headers: { Accept: 'application/json', 'Content-Type': 'application/json' },
        body: JSON.stringify({ filename, content }),
        signal: controller.signal,
      });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') throw new ApiClientError('File parse timed out after ' + REQUEST_TIMEOUT_MS + ' ms', 'timeout');
      throw new ApiClientError('Unable to connect to the NEXUS geometry parser', 'connection');
    }
    let payload: unknown;
    try { payload = await response.json(); } catch { throw new ApiClientError('Geometry parser returned a non-JSON response', 'malformed'); }
    if (!response.ok) {
      const message = payload && typeof payload === 'object' && 'message' in payload ? String((payload as { message: unknown }).message) : 'File parse failed with HTTP ' + response.status;
      throw new ApiClientError(message, 'http');
    }
    if (!payload || typeof payload !== 'object' || (payload as { status?: string }).status !== 'success') throw new ApiClientError('Geometry parser returned an invalid response shape', 'malformed');
    return payload as T;
  } finally { window.clearTimeout(timeout); }
}

export function parseStl(filename: string, content: string) { return parseUploadedFile<GeometryResponse>('/geometry/stl/parse', filename, content); }
export function parseTemperatureCsv(filename: string, content: string) { return parseUploadedFile<TemperatureResponse>('/geometry/csv/parse', filename, content); }

export async function runPerformanceBenchmark(config: PerformanceConfig, externalSignal?: AbortSignal): Promise<PerformanceResponse> {
  const controller = new AbortController();
  const abortFromCaller = () => controller.abort();
  if (externalSignal?.aborted) controller.abort();
  else externalSignal?.addEventListener('abort', abortFromCaller, { once: true });
  const timeout = window.setTimeout(() => controller.abort(), 30000);
  try {
    let response: Response;
    try {
      response = await fetch(`${API_BASE_URL}/performance/benchmark`, { method: 'POST', headers: { Accept: 'application/json', 'Content-Type': 'application/json' }, body: JSON.stringify(config), signal: controller.signal });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') throw new ApiClientError('Performance benchmark timed out after 30000 ms', 'timeout');
      throw new ApiClientError('Unable to connect to the NEXUS performance benchmark', 'connection');
    }
    let payload: unknown;
    try { payload = await response.json(); } catch { throw new ApiClientError('Performance benchmark returned a non-JSON response', 'malformed'); }
    if (!response.ok) {
      const message = payload && typeof payload === 'object' && 'message' in payload ? String((payload as { message: unknown }).message) : `Performance request failed with HTTP ${response.status}`;
      throw new ApiClientError(message, 'http');
    }
    if (!payload || typeof payload !== 'object' || (payload as { status?: string }).status !== 'success') throw new ApiClientError('Performance benchmark returned an invalid response shape', 'malformed');
    return payload as PerformanceResponse;
  } finally { window.clearTimeout(timeout); externalSignal?.removeEventListener('abort', abortFromCaller); }
}

export async function runNeuralBackpropagation(config: { x: number; target: number; learning_rate: number; additional_training_steps: number; parameters: NeuralParameters }): Promise<NeuralResponse> {
  const controller = new AbortController();
  const timeout = window.setTimeout(() => controller.abort(), 30000);
  try {
    let response: Response;
    try {
      response = await fetch(`${API_BASE_URL}/neural/backpropagation`, { method: 'POST', headers: { Accept: 'application/json', 'Content-Type': 'application/json' }, body: JSON.stringify({ x: config.x, target: config.target, learning_rate: config.learning_rate, additional_training_steps: config.additional_training_steps, ...config.parameters }), signal: controller.signal });
    } catch (error) {
      if (error instanceof DOMException && error.name === 'AbortError') throw new ApiClientError('Neural backpropagation timed out after 30000 ms', 'timeout');
      throw new ApiClientError('Unable to connect to the NEXUS neural engine', 'connection');
    }
    let payload: unknown;
    try { payload = await response.json(); } catch { throw new ApiClientError('Neural engine returned a non-JSON response', 'malformed'); }
    if (!response.ok) {
      const message = payload && typeof payload === 'object' && 'message' in payload ? String((payload as { message: unknown }).message) : `Neural request failed with HTTP ${response.status}`;
      throw new ApiClientError(message, 'http');
    }
    if (!payload || typeof payload !== 'object' || (payload as { status?: string }).status !== 'success') throw new ApiClientError('Neural engine returned an invalid response shape', 'malformed');
    return payload as NeuralResponse;
  } finally { window.clearTimeout(timeout); }
}

async function hardwareRequest<T>(path: string, method: 'GET' | 'POST', device_id?: string): Promise<T> {
  const controller = new AbortController(); const timeout = window.setTimeout(() => controller.abort(), 8000);
  try {
    let response: Response;
    try { response = await fetch(`${API_BASE_URL}${path}`, { method, headers: { Accept: 'application/json', 'Content-Type': 'application/json' }, body: method === 'POST' ? JSON.stringify(device_id ? { device_id } : {}) : undefined, signal: controller.signal }); }
    catch (error) { if (error instanceof DOMException && error.name === 'AbortError') throw new ApiClientError('Hardware request timed out', 'timeout'); throw new ApiClientError('Unable to connect to the NEXUS hardware service', 'connection'); }
    let payload: unknown; try { payload = await response.json(); } catch { throw new ApiClientError('Hardware service returned a non-JSON response', 'malformed'); }
    if (!response.ok) { const message = payload && typeof payload === 'object' && 'message' in payload ? String((payload as { message: unknown }).message) : `Hardware request failed with HTTP ${response.status}`; throw new ApiClientError(message, 'http'); }
    if (!payload || typeof payload !== 'object' || (payload as { status?: string }).status !== 'success') throw new ApiClientError('Hardware service returned an invalid response shape', 'malformed');
    return payload as T;
  } finally { window.clearTimeout(timeout); }
}

export function getHardwareDevices() { return hardwareRequest<HardwareDevicesResponse>('/hardware/devices', 'GET'); }
export function discoverHardware() { return hardwareRequest<HardwareDevicesResponse>('/hardware/discover', 'POST'); }
export function connectHardware(device_id: string) { return hardwareRequest<{ status: 'success'; simulated: boolean; device: HardwareDevice }>('/hardware/connect', 'POST', device_id); }
export function disconnectHardware(device_id: string) { return hardwareRequest<{ status: 'success'; simulated: boolean; device: HardwareDevice }>('/hardware/disconnect', 'POST', device_id); }
export function simulateHardwareFailure(device_id: string) { return hardwareRequest<{ status: 'success'; simulated: boolean; device: HardwareDevice }>('/hardware/simulate-failure', 'POST', device_id); }
export function recoverHardware(device_id: string) { return hardwareRequest<{ status: 'success'; simulated: boolean; device: HardwareDevice }>('/hardware/recover', 'POST', device_id); }
export function getHardwareTelemetry() { return hardwareRequest<HardwareTelemetryResponse>('/hardware/telemetry', 'GET'); }

export { API_BASE_URL };
