# Windows 11 standalone packaging

## Verification status

Status: **WINDOWS CI VERIFIED; INTERACTIVE GUI NOT VERIFIED**. GitHub Actions workflow `NEXUS Windows build`, run #5 on commit `a261a03`, completed successfully on `windows-2022` x64. It verified MSVC C++17 compilation, CTest, frontend build, CPack ZIP generation, required ZIP entries, packaged backend startup, HTTP health/frontend delivery, 11/11 API integration checks, and artifact uploads. It did not perform a physical double-click GUI test or clean-machine/offline evaluation.

## Required build environment

- Windows 11 x64.
- Visual Studio 2022 with Desktop development with C++ and the Windows SDK.
- CMake 3.20 or newer.
- Node.js 20 LTS or newer and npm, only for building the frontend assets.
- No internet is required after npm dependencies are installed and the source tree is available.

## Reproducible build

From the repository root in a Visual Studio Developer PowerShell:

```powershell
cd frontend
npm ci
npm run build
cd ..
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64 -DNEXUS_BUILD_TESTS=ON
cmake --build build-win --config Release
ctest --test-dir build-win -C Release --output-on-failure
```

The `WIN32` CMake branch builds `Launch.exe` from `packaging/windows/Launch.cpp`. Install a staging tree with:

```powershell
cmake --install build-win --config Release --prefix package
Copy-Item -Recurse -Force frontend\dist package\frontend\dist
Copy-Item -Recurse -Force docs package\docs
Copy-Item -Recurse -Force data package\data
```

The verified package contains `Launch.exe`, `bin\nexus-api.exe`, `frontend\dist\index.html`, production assets, documentation, sample data, and no developer-specific absolute paths. The workflow selected the static MSVC runtime and inspected the backend dependencies; no dynamic MSVC runtime DLL was required by that configuration. System Windows libraries remain operating-system dependencies and are not claimed as bundled application files.

## GitHub Actions workflow

Run the workflow from the repository's **Actions** tab using **NEXUS Windows build → Run workflow**, or push to `main` / open a pull request. The workflow:

1. Uses the hosted `windows-2022` runner, configures Visual Studio 2022's x64 MSVC environment with `ilammy/msvc-dev-cmd`, and uses C++17, CMake, Node.js 20, npm, and Python.
2. Runs `npm ci` and `npm run build` for the frontend.
3. Configures the Release C++ build with the static MSVC runtime, builds the API, launcher, and tests, and runs CTest.
4. Installs the package into a relative staging directory and runs CPack to create the Windows ZIP.
5. Inspects executable dependencies and fails if an unstaged dynamic MSVC runtime is required.
6. Starts `package\bin\nexus-api.exe` on port 8081, sets `NEXUS_API_BASE_URL=http://127.0.0.1:8081`, polls `/api/health` for up to 30 seconds, verifies the served frontend root, and runs `scripts/api_integration_test.py` in the same process-owning step so the runner cannot clean up the backend between readiness and integration checks.
7. Prints backend exit code/stdout/stderr on failure, terminates the packaged backend in a `finally` block, and uploads both the ZIP and preserved build/test logs.

CPack is explicitly invoked as:

```powershell
New-Item -ItemType Directory -Force build-win/cpack-output
cpack -B build-win/cpack-output --config build-win/CPackConfig.cmake -C Release --verbose
```

The generated ZIP is searched only among direct files in `build-win/cpack-output` (CPack's `_CPack_Packages` subtree is not enumerated), copied to `build-win/artifacts/NEXUS-Windows.zip`, and uploaded as the artifact `NEXUS-Windows-<commit-sha>`. Build logs, including CPack discovery and ZIP-entry validation, are uploaded as `NEXUS-Windows-logs-<commit-sha>` even when a workflow step fails. If the exact-one-archive validation fails, the validation log lists every direct file found in the dedicated CPack output directory.

The workflow does not claim interactive GUI verification: it tests the packaged backend and static frontend over HTTP. It also does not claim a clean physical Windows machine; the runner is an ephemeral hosted CI environment.

## Current build and test result

Successful workflow evidence: repository `arushisiingh/NEXUS`, commit `a261a03`, workflow `NEXUS Windows build`, run `#5`, runner `windows-2022`, architecture Windows x64. Uploaded artifacts were `NEXUS-Workbench-0.1.0-win64.zip` and `NEXUS-Windows-logs-a261a032bdf562d7dd33e38238c5dc0fa564cf32.zip`. The package and backend were inspected as PE32+ x64/runtime artifacts by the developer. Interactive GUI and physical clean-machine verification remain outstanding.

## Runtime behavior

`Launch.exe` starts `bin\nexus-api.exe` without a terminal window, sets `NEXUS_STATIC_ROOT` to the package-relative `frontend\dist` directory, and calls the Windows shell to open `http://127.0.0.1:8081/` in the default browser. The C++ API serves `/` and `/assets/*` directly from the bundled frontend assets, so Node.js and Vite are not required on the evaluator machine. `NEXUS_API_PORT` or a command-line port can be used for source launches; the launcher currently uses 8081 and has no implemented port-conflict dialog.

The launcher remains associated with the backend process while the backend runs. A clean-machine test must confirm the expected shutdown behavior and should terminate the launcher/backend together before repeating a launch.

## Clean-machine verification procedure

1. Extract the ZIP into a path without spaces or developer-specific assumptions.
2. Confirm `Launch.exe` exists beside `bin`, `frontend`, `docs`, and `data`.
3. Double-click `Launch.exe`; confirm no console window appears and the browser opens.
4. Verify `/api/health`, each module, sample imports, exports, and the simulated hardware workflow.
5. Disconnect the network and repeat the launch.
6. Inspect Task Manager after closing the evaluator session for orphaned `nexus-api.exe` processes.
7. Record Windows version, compiler, CTest output, screenshots, and any port conflict.
