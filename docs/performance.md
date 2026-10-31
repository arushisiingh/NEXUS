# Performance Lab

Performance Lab benchmarks independent thermal FEM cases using the existing C++17 solver. The same generated workload object is passed to every serial and parallel run, preserving case IDs and input order.

## Workload generation

`generate_workload` uses `std::mt19937_64` with the requested fixed seed. Each case uses the assignment geometry and boundary temperatures, with conductivity sampled uniformly from 5–50 W/(m·K) and volumetric heat generation sampled uniformly from 1,000–20,000 W/m³. Both values are finite and positive. Reusing the same seed, case count, and mesh size produces identical generated parameters.

## Execution architecture

Serial execution iterates over the workload in case order and calls `nexus::thermal::solve`. Parallel execution creates a bounded number of `std::thread` workers, capped by the requested count and available logical CPU count. Workers claim the next case through an atomic index and write only to their own pre-sized result slot. Exceptions are captured, workers are always joined, and the first exception is rethrown after shutdown.

## Benchmark methodology

For serial execution and every requested worker count, Performance Lab runs one warm-up followed by five measured repetitions. Timing uses `std::chrono::steady_clock` around only solver execution. Workload generation, request transport, JSON serialization, browser rendering, file loading, and chart rendering are excluded. `launch_to_ready_ms` reports workload preparation separately.

For serial median `T1` and parallel median `Tp` with `p` workers:

```text
speedup = T1 / Tp
efficiency = speedup / p
```

The UI reports actual CPU model, physical/logical core counts, RAM when available, compiler, and optimization status. It does not invent missing hardware metadata.

## Numerical agreement

The final serial result is compared with a final parallel result for every configured worker count. The maximum absolute nodal-temperature difference must be no greater than `1e-8 °C`. The API returns the maximum difference, case count, agreement status, and failed case IDs.

## API

`POST /api/performance/benchmark`

Example request:

```json
{"case_count":240,"mesh_size":40,"seed":20241031,"worker_counts":[1,2,4]}
```

The response includes workload metadata, five serial timings, five timings per worker configuration, medians, speedups, efficiencies, system metadata, numerical agreement, progress, launch-to-ready time, and total execution duration. Invalid configuration returns HTTP 400.

The frontend keeps the request asynchronous and provides cancellation through `AbortController`; cancellation stops the client request, while an already-running server-side benchmark is not forcibly interrupted by the current lightweight HTTP server.

## Limitations and interpretation

The default is 240 cases at 40 elements because the earlier 24-case workload completed in only a few milliseconds and produced timing noise, including efficiencies above 100%. The larger workload takes roughly 1 second on the verified Apple M1 environment and still reports raw results. Small workloads can be dominated by thread creation and synchronization overhead, so a worker count being slower than serial is valid and is reported as-is. There is no persistent benchmark database, streaming progress channel, or OS-level process isolation yet.

## Debugging insight

The first live integration check used the rebuilt backend binary while the previous process was still listening on port 8081. The benchmark route appeared missing until the exact old NEXUS process was stopped and the rebuilt binary was started. Final verification therefore checks both the listening process and the live HTTP response rather than relying on compilation alone.
