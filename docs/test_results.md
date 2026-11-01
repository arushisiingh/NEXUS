# NEXUS stabilization test results

Run date: 2026-11-01 local evidence plus GitHub Actions Windows run #5 for commit `a261a03`. Windows CI evidence below was supplied from the successful hosted-runner log/artifact inspection.

## GitHub Actions Windows verification

Repository: `arushisiingh/NEXUS`

Workflow: `NEXUS Windows build`

Run: `#5`
Runner: `windows-2022`, Windows x64, MSVC C++17

```text
MSVC C++ compilation: PASS
CTest: 6/6 passed
Frontend production build: PASS
CPack ZIP generation: PASS
Required ZIP entry validation: PASS
Packaged backend startup: PASS
Health endpoint: HTTP 200
Frontend delivery through packaged backend: HTTP 200
API integration: 11/11 passed
Windows ZIP artifact upload: PASS
Build/test logs artifact upload: PASS
```

Uploaded artifacts:

- `NEXUS-Workbench-0.1.0-win64.zip`
- `NEXUS-Windows-logs-a261a032bdf562d7dd33e38238c5dc0fa564cf32.zip`

The uploaded package was inspected and contains `Launch.exe`, `bin/nexus-api.exe`, `frontend/dist/index.html`, production frontend assets, sample STL/temperature CSV data, documentation, README, and CMake configuration. `Launch.exe` and `nexus-api.exe` were confirmed as PE32+ x64 executables. The packaged health response was:

```json
{"application_name":"NEXUS","application_version":"0.1.0","backend_status":"online","cpp_standard":"C++17","api_status":"ok"}
```

This verifies automated packaged HTTP behavior, not an interactive desktop launch or physical clean-machine/offline evaluation.

## Clean C++ configure/build/CTest

Command:

```text
cmake -S . -B /private/tmp/nexus-clean-build -DNEXUS_BUILD_TESTS=ON -DNEXUS_ENABLE_GTEST=OFF
cmake --build /private/tmp/nexus-clean-build --config Release
ctest --test-dir /private/tmp/nexus-clean-build --output-on-failure
```

Result: six of six tests passed.

```text
1/6 nexus-core-smoke ................. Passed
2/6 nexus-thermal-focused ............ Passed
3/6 nexus-geometry-focused ........... Passed
4/6 nexus-performance-focused ........ Passed
5/6 nexus-neural-focused ............. Passed
6/6 nexus-hardware-focused ........... Passed
100% tests passed, 0 tests failed out of 6
```

Focused tests cover thermal boundaries/convergence/zero heat, geometry STL/CSV validation, performance reproducibility/parallel agreement, neural gradients/training/invalid values, and hardware discovery/lifecycle/failure/recovery.

## Frontend production build

Command:

```text
cd frontend
npm run build
```

Result: TypeScript and Vite build passed. The final bundle reports a non-blocking chunk-size warning because Three.js and the full workbench are currently bundled together.

## Live API integration

Backend command:

```text
./build/nexus-api 8081
```

Integration command:

```text
python3 scripts/api_integration_test.py
```

Result:

```text
PASS: health endpoint
PASS: static frontend root endpoint
PASS: geometry STL endpoint
PASS: thermal endpoint and expected error
PASS: performance endpoint and numerical agreement
PASS: neural endpoint and gradient check
PASS: hardware discovery
PASS: hardware connect
PASS: hardware timeout simulation
PASS: hardware recovery
PASS: invalid hardware device ID
API integration: PASS
```

## Browser smoke and console

The live Vite page at `http://127.0.0.1:5173/` rendered the Overview dashboard with `BACKEND ONLINE`, `C++17`, and `API service READY`. Navigation labels for Geometry Lab, Thermal Studio, Performance Lab, Hardware Studio, and Neural Lab were present in the live DOM. Browser logs contained only Vite connection messages and the standard React DevTools informational notice; no error or exception entries were observed.

The rebuilt backend also served the packaged frontend root at `http://127.0.0.1:8081/` with HTTP 200, confirming the standalone static-serving path used by the packaging configuration.

## Numerical evidence

Thermal mesh convergence is reproducible: N=10 `1.25 °C`, N=20 `0.3125 °C`, N=40 `0.078125 °C`, N=80 `0.01953125 °C`. Neural default assignment finite-difference absolute differences are `1.130e-10`, `1.130e-10`, `5.254e-11`, and `1.118e-10`; final loss after 100 additional steps is `1.080495e-20`.

The performance default was increased from 24 to 240 cases at mesh 40 after a live comparison showed the smaller run completed in only a few milliseconds and produced noisy efficiencies above 100%. The larger live run took about 1.00 s end-to-end, reported serial median `40.589 ms`, four-worker median `13.158 ms`, speedup `3.085×`, efficiency `77.1%`, and maximum serial/parallel temperature difference `0 °C` over 240 cases.

## Warning classification

- Duplicate static-library linker warning: **RESOLVED** by removing redundant direct links where `nexus_performance` already propagates `nexus_thermal`.
- Vite chunk-size warning: **NON-BLOCKING**. The application builds and renders; code splitting is a future optimization.
- Initial sandboxed localhost probe: **ENVIRONMENTAL**. The restricted runner denied local socket access; the same integration command was rerun with local-process permission and passed.
- Windows CI executable and package: **VERIFIED** in hosted run #5; interactive GUI and physical clean-machine/offline behavior remain unverified.
- npm audit findings from the existing dependency tree: **OUTSTANDING** and not force-fixed; dependency upgrades could destabilize the evaluated frontend.

## Browser smoke

The in-app browser visited all five non-overview modules. Each module rendered successfully, and the final browser diagnostics contained no errors or warnings. Hardware discovery, connection, simulated timeout, recovery, and telemetry were also exercised with screenshots captured under `/private/tmp/`.
