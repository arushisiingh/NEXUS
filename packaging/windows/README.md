# Windows package layout

The Windows launcher is built only on Windows with the `WIN32` CMake branch. It starts `bin/nexus-api.exe` with a hidden console, sets `NEXUS_STATIC_ROOT` to the package's `frontend/dist` directory, and opens `http://127.0.0.1:8081/` in the default browser.

The launcher is intentionally not labelled as verified in this macOS development environment. Use `docs/windows_packaging.md` for the reproducible Windows 11 process.
