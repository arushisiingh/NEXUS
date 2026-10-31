# Windows 11 standalone packaging

## Verification status

Status: **NOT VERIFIED**. Development and all executed builds in this workspace are macOS builds. No Windows 11 host, Visual Studio toolchain, or clean-machine test was available during stabilization. The configuration below is prepared for a Windows build but must not be represented as a tested executable.

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

The `WIN32` CMake branch builds `nexus-launcher.exe` from `packaging/windows/Launch.cpp`. Install a staging tree with:

```powershell
cmake --install build-win --config Release --prefix package
Copy-Item -Recurse -Force frontend\dist package\frontend\dist
Copy-Item -Recurse -Force docs package\docs
Copy-Item -Recurse -Force data package\data
```

The expected package contains `nexus-launcher.exe`, `bin\nexus-api.exe`, `frontend\dist\index.html`, assets, documentation, sample data, and no developer-specific absolute paths. Rename `nexus-launcher.exe` to `Launch.exe` for the submitted package.

## Runtime behavior

`Launch.exe` starts the backend without a terminal window, sets a package-relative static asset root, and opens the browser at port 8081. The C++ API serves `/` and `/assets/*` directly from the bundled frontend assets, so Node.js and Vite are not required on the evaluator machine. `NEXUS_API_PORT` or a command-line port can be used for source launches; the launcher currently uses 8081 and should be extended with a port-conflict dialog if the clean-machine test finds a conflict.

The launcher remains associated with the backend process while the backend runs. A clean-machine test must confirm the expected shutdown behavior and should terminate the launcher/backend together before repeating a launch.

## Clean-machine verification procedure

1. Extract the ZIP into a path without spaces or developer-specific assumptions.
2. Confirm `Launch.exe` exists beside `bin`, `frontend`, `docs`, and `data`.
3. Double-click `Launch.exe`; confirm no console window appears and the browser opens.
4. Verify `/api/health`, each module, sample imports, exports, and the simulated hardware workflow.
5. Disconnect the network and repeat the launch.
6. Inspect Task Manager after closing the evaluator session for orphaned `nexus-api.exe` processes.
7. Record Windows version, compiler, CTest output, screenshots, and any port conflict.
