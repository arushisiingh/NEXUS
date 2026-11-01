# NEXUS final compliance audit

Audit basis: repository state after the final documentation/submission-preparation changes, local macOS checks, and the developer-supplied GitHub Actions evidence for `arushisiingh/NEXUS`, workflow **NEXUS Windows build**, run **#5**, commit `a261a03`, on `windows-2022` x64. Statuses distinguish implemented behavior from evidence that still requires a human or physical environment.

## Evidence summary

| Evidence | Result | Scope / limitation |
|---|---|---|
| macOS C++ build | PASS | Clean CMake build used for regression checks |
| macOS CTest | PASS, 6/6 | Core, API, thermal, geometry, performance, and neural tests |
| Frontend production build | PASS | Vite build; existing large-chunk warning remains non-blocking |
| macOS live API and browser smoke | PASS | Health, modules, static frontend, and browser navigation |
| Windows hosted build | PASS | MSVC Release, CTest 6/6, frontend, CPack, package validation |
| Windows packaged HTTP checks | PASS | Health 200, frontend 200, 11/11 API integration |
| Windows interactive GUI | NOT VERIFIABLE | Not run by hosted CI |
| Windows physical clean-machine/offline test | NOT VERIFIABLE | Requires a separate Windows machine or VM |

## Requirement-by-requirement audit

| Requirement | Implementation / file | Evidence or command | Status | Exact remaining action |
|---|---|---|---|---|
| Unified desktop interface | `frontend/src/App.tsx`, module pages, shared styles | `npm run build`; browser smoke | COMPLETE | None for implementation |
| C++17 backend and stable API | `backend/api/main.cpp`, `backend/api/server.*`, `CMakeLists.txt` | macOS build; Windows MSVC build and CTest | COMPLETE | None for implementation |
| Thermal FEM | `core/thermal.*`, `frontend/src/pages/ThermalStudio.tsx` | Thermal CTest and live API | COMPLETE | None for implementation |
| Geometry STL and ANSYS-compatible CSV | `core/geometry.*`, geometry API/page, `data/geometry` fixtures | Geometry CTest; import/export API checks | COMPLETE | For submission, show the representative fixture label during demo |
| Geometry interaction and export | `frontend/src/pages/GeometryLab.tsx`, geometry API | Browser smoke and API integration | COMPLETE | Capture manual screenshots and an exported file |
| Performance benchmark | `core/performance.*`, performance API/page | Performance CTest and API integration | COMPLETE | Capture the evaluator-facing run and resource readout |
| Hardware abstraction | `core/hardware.*`, `docs/hardware.md`, `docs/hardware_interface.md` | Hardware CTest/API checks | COMPLETE (simulated) | Do not claim physical USB/JTAG/GPIO operation |
| Neural backpropagation | `core/neural.*`, Neural Lab page, `docs/handwritten_backprop_guide.md` | Neural CTest, finite differences, API integration | COMPLETE | Candidate must handwrite/sign/date the derivation and attach it |
| Input validation and limits | API validation, frontend validation, 2 MiB upload guard | Negative CTest/API cases; source inspection | COMPLETE | Keep the existing 2 MiB limit visible in the demo |
| Cancellation and progress | API request/status flow and frontend progress states | Source inspection and live status checks | PARTIAL | Server-side cancellation is not streaming; document this limitation |
| Windows standalone package | `.github/workflows/windows-build.yml`, CPack config, `Launch.exe` | Run #5: package, required entries, startup, HTTP checks | COMPLETE for hosted CI | Perform interactive GUI and clean-machine/offline verification |
| Reports and traceability | `docs/submission_report.md`, `docs/requirements_traceability.md`, `docs/test_results.md` | Local and hosted evidence recorded | COMPLETE | Render the final report to confirm any institutional page limit |
| Demo evidence | `docs/demo_walkthrough.md`, `docs/demo_script.md` | Automated evidence exists | PARTIAL | Capture timestamped manual screenshots/video and attach exports |
| AI disclosure and ownership | `docs/ai_usage_log.md`, `docs/ownership_statement.md` | Files present and candidate ID recorded | PARTIAL | Candidate must review, sign, and disclose any additional tools/contributors |
| Final candidate archive | `scripts/create_submission_zip.py` | Script syntax/fixture test; no real package locally | INCOMPLETE | Download/extract run #5 Windows ZIP, then create `RA2311027010156_Workbench.zip` |

## Archive gate

The final archive must be created only from the verified Windows package and must contain the runtime package plus `src/`, `tests/`, `scripts/`, `results/`, `docs/`, `data/`, `config/`, `README.md`, `CMakeLists.txt`, and frontend package metadata. `scripts/create_submission_zip.py` rejects unsafe/placeholder IDs, refuses overwrite, validates `Launch.exe`, `bin/nexus-api.exe`, and `frontend/dist/index.html`, and excludes `__MACOSX` metadata. The exact command after downloading the artifact is:

```text
python3 scripts/create_submission_zip.py RA2311027010156 --windows-package <extracted-run-5-package> --output-dir submission
```

Expected output: `submission/RA2311027010156_Workbench.zip`. This archive is intentionally not fabricated in the repository because the Windows artifact is not present locally.

## Genuine limitations

The hosted Windows run is strong automated evidence, but it is not a double-click GUI test, a physical hardware test, or a clean-machine/offline test. The hardware module is explicitly simulated. The frontend bundle-size warning remains. The API server is intentionally lightweight, and cancellation is not a full streaming cancellation protocol. These are recorded as limitations rather than hidden by the report.
