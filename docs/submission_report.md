# NEXUS Unified Engineering Workbench — final report

Verification note: GitHub Actions run #5 on `windows-2022` for commit `a261a03` passed Windows x64 compilation, CTest, frontend build, CPack packaging, packaged HTTP checks, 11/11 API integration checks, and artifact uploads. Interactive GUI and physical clean-machine verification were not performed.

## 1. Problem and architecture

NEXUS is a local engineering workbench combining a React/Vite interface with a C++17 API and browser-independent core modules. The API validates requests and serializes results; numerical and processing responsibilities stay in `core/`. The same API serves bundled frontend assets in the verified Windows standalone package.

## 2. Geometry processing

Geometry Lab parses strict ASCII STL and temperature CSV data in C++17. It reports triangles, bounds, node statistics, and processing time. Three.js renders the returned mesh as solid/wireframe and the returned temperature nodes as a color-mapped point cloud. Sample provenance and unsupported binary STL/CSV quoting limitations are documented.

## 3. Thermal FEM

Thermal Studio assembles two-node linear stiffness matrices and consistent load vectors, applies both prescribed temperatures, solves the dense system, and compares midpoint interpolation with the analytical quadratic solution. The measured errors are 1.25, 0.3125, 0.078125, and 0.01953125 °C for 10, 20, 40, and 80 elements, demonstrating the expected approximately second-order reduction.

## 4. Performance

Performance Lab generates deterministic thermal cases from a fixed seed, executes serial and bounded `std::thread` worker-pool runs, warms up once, measures five repetitions, and reports medians, speedup, efficiency, hardware/compiler metadata, and numerical agreement. The stabilized 240-case run produced 40.589 ms serial median and 13.158 ms four-worker median, or 3.085× speedup and 77.1% efficiency. Results remain raw; small-workload noise is documented.

## 5. Hardware simulation

Hardware Studio models an Arduino Nano RP2040 Connect, Digilent Basys 3, and SEGGER J-Link EDU Mini with device-specific voltage, connector, interface, and debug metadata. Discovery, connection, telemetry, timeout, and recovery are stateful C++ mock-adapter operations. No physical hardware access is present.

## 6. Neural backpropagation

Neural Lab implements the exact one-sample `tanh` regression assignment with manual chain-rule derivatives, simultaneous updates, central finite differences, and 100 additional training steps. The default gradients pass the `1e-5` check by roughly ten orders of magnitude, and the loss decreases from 1.182541819 to 1.080495e-20.

## 7. Testing, limitations, conclusion

Clean macOS CMake configuration, six focused CTest targets, frontend production build, live API integration, static frontend serving, and browser navigation smoke all pass. GitHub Actions run #5 additionally verified Windows x64 MSVC compilation, CTest, frontend build, CPack packaging, ZIP contents, packaged backend startup, HTTP health/frontend delivery, 11/11 API integration checks, and artifact uploads. Remaining limitations are the lightweight HTTP server, non-streaming server cancellation, frontend bundle-size warning, no geometry derived export, and lack of interactive GUI/physical clean-machine evidence. The system is ready for evaluator review after the candidate supplies the identifier and handwritten appendix.
