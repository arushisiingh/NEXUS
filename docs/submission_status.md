# NEXUS submission status

## Candidate identifier

**RA2311027010156**

The required archive name is:

```text
RA2311027010156_Workbench.zip
```

No final archive is created in this workspace until the verified Windows artifact is downloaded. The candidate identifier is now known, so the final filename is deterministic: `RA2311027010156_Workbench.zip`.

## Platform verification

- macOS Apple Silicon: C++ build, CTest, frontend build, live API integration, static frontend serving, and browser smoke completed.
- Windows x64 hosted CI: **VERIFIED** in GitHub Actions run #5 for commit `a261a03`: compilation, CTest, frontend build, CPack ZIP, ZIP-entry validation, packaged backend startup, health/frontend HTTP checks, 11/11 API integration checks, and artifact uploads passed.
- Interactive Windows GUI launch: **NOT VERIFIED**. The hosted runner did not perform a double-click desktop/browser test.
- Physical clean-machine/offline evaluation: **NOT VERIFIED**.

## Developer actions before submission

1. Review the candidate identifier and use the exact final archive name above.
2. Review and sign `docs/ownership_statement.md`.
3. Complete the handwritten backpropagation appendix and timestamped evidence checklist in `docs/demo_script.md`.
4. If possible, run the Windows clean-machine procedure in `docs/windows_packaging.md`; do not represent the hosted-runner result as that evidence.
5. Obtain the verified Windows artifact from GitHub Actions and run `scripts/create_submission_zip.py` after supplying the real candidate ID.

The final archive is not currently present in this workspace because the GitHub Actions ZIP has not been downloaded into the workspace. Run `scripts/create_submission_zip.py RA2311027010156 --windows-package <extracted-run-5-package> --output-dir submission`; it creates and validates `submission/RA2311027010156_Workbench.zip` without overwriting an existing archive.
