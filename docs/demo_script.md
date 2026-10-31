# NEXUS 3–5 minute demo script

1. Start `nexus-api` on port 8081 and the Vite frontend on port 5173; show the dashboard's live backend status.
2. Open Geometry Lab, load the sample STL and temperature CSV, switch to Temperature Cloud, and select a node.
3. Open Thermal Studio, run the default FEM study, show the temperature chart, convergence table, and export CSV.
4. Open Performance Lab, keep the 240-case default, run the benchmark, show the worker table, scaling charts, numerical agreement, and export CSV.
5. Open Hardware Studio, discover devices, connect the Nano RP2040 simulation, trigger SIMULATE TIMEOUT, show the fault telemetry, then Recover.
6. Open Neural Lab, run the assignment, show forward values, finite-difference ALL PASS table, loss curve, and export CSV.
7. Close with the limitations: hardware is simulated and Windows packaging is prepared but not verified on this macOS host.

## Evidence checklist

- [ ] Timestamped dashboard/backend health screenshot.
- [ ] Geometry imported mesh and temperature-cloud screenshot.
- [ ] Thermal convergence table and exported CSV.
- [ ] Performance benchmark table and raw API response.
- [ ] Hardware discovery screenshot.
- [ ] Hardware connected screenshot.
- [ ] Hardware timeout and recovery screenshots.
- [ ] Neural gradient-check and loss-curve screenshot.
- [ ] Terminal/API log showing integration PASS.
- [ ] CTest and frontend build logs.
- [ ] Windows clean-machine recording, if performed by the developer.
