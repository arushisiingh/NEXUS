# NEXUS submission status

## Candidate identifier

**TO BE SUPPLIED BY DEVELOPER**

The required archive name is:

```text
CandidateID_Workbench.zip
```

No final archive is created until the real candidate identifier is supplied. This avoids submitting a misleading filename or inventing an identity.

## Platform verification

- macOS Apple Silicon: C++ build, CTest, frontend build, live API integration, static frontend serving, and browser smoke completed.
- Windows 11 x64: **NOT VERIFIED**. The Windows launcher and CPack/install configuration are prepared, but a Windows compiler and clean-machine run were not available in this workspace.

## Developer actions before submission

1. Supply the candidate identifier and rename the final archive accordingly.
2. Review and sign `docs/ownership_statement.md`.
3. Complete the handwritten backpropagation appendix and timestamped evidence checklist in `docs/demo_script.md`.
4. Run the Windows clean-machine procedure in `docs/windows_packaging.md`.
5. Re-run the final checks after any manual changes, then create the archive excluding build caches and `node_modules`.
