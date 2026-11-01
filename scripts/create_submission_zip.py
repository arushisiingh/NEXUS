#!/usr/bin/env python3
"""Create the final candidate submission archive from a verified Windows artifact.

The candidate identifier and Windows package directory are intentionally explicit
inputs so this script cannot invent a filename or silently package an unverified
build.
"""
from __future__ import annotations

import argparse
import re
import shutil
import tempfile
import zipfile
from pathlib import Path


ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
REQUIRED_WINDOWS_FILES = (
    "Launch.exe",
    "bin/nexus-api.exe",
    "frontend/dist/index.html",
    "docs/windows_packaging.md",
)


def add_tree(archive: zipfile.ZipFile, source: Path, prefix: str = "") -> None:
    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue
        relative = path.relative_to(source).as_posix()
        archive_name = f"{prefix}{relative}"
        if archive_name.startswith("__MACOSX/") or "/__MACOSX/" in archive_name:
            continue
        archive.write(path, archive_name)


def validate_windows_package(package: Path) -> None:
    missing = [name for name in REQUIRED_WINDOWS_FILES if not (package / name).is_file()]
    if missing:
        raise SystemExit(f"Windows package is missing required files: {', '.join(missing)}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate_id", help="real candidate identifier; never use the placeholder")
    parser.add_argument("--windows-package", type=Path, required=True, help="extracted verified Windows package directory")
    parser.add_argument("--output-dir", type=Path, default=Path("submission"))
    args = parser.parse_args()

    if not ID_PATTERN.fullmatch(args.candidate_id) or args.candidate_id.lower() == "candidateid":
        raise SystemExit("Provide the real candidate identifier; placeholder or unsafe identifiers are rejected")
    package = args.windows_package.resolve()
    if not package.is_dir():
        raise SystemExit(f"Windows package directory does not exist: {package}")
    validate_windows_package(package)

    root = Path(__file__).resolve().parents[1]
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f"{args.candidate_id}_Workbench.zip"
    if output.exists():
        raise SystemExit(f"Refusing to overwrite existing archive: {output}")

    with tempfile.TemporaryDirectory(prefix="nexus-submission-") as temp_name:
        staging = Path(temp_name)
        shutil.copytree(package, staging, dirs_exist_ok=True)

        # Keep the verified runtime package at the archive root.  Put source
        # code under a literal src/ directory while retaining tests, results,
        # scripts, and configuration at predictable submission-level paths.
        source_specs = (
            ("core", "src/core"),
            ("backend", "src/backend"),
            ("frontend/src", "src/frontend/src"),
            ("tests", "tests"),
            ("scripts", "scripts"),
            (".vscode", ".vscode"),
            (".github", ".github"),
            ("results", "results"),
            ("config", "config"),
            ("README.md", "README.md"),
            ("CMakeLists.txt", "CMakeLists.txt"),
            ("frontend/package.json", "frontend/package.json"),
            ("frontend/package-lock.json", "frontend/package-lock.json"),
        )
        for relative, archive_relative in source_specs:
            source = root / relative
            destination = staging / archive_relative
            if source.exists():
                if source.is_dir():
                    shutil.copytree(source, destination, dirs_exist_ok=True)
                else:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(source, destination)

        with zipfile.ZipFile(output, "x", compression=zipfile.ZIP_DEFLATED) as archive:
            add_tree(archive, staging)

    with zipfile.ZipFile(output) as archive:
        names = set(archive.namelist())
        missing = [name for name in REQUIRED_WINDOWS_FILES if name not in names]
        if missing:
            output.unlink(missing_ok=True)
            raise SystemExit(f"Created archive is missing required Windows files: {', '.join(missing)}")
        if any(name.startswith("__MACOSX/") for name in names):
            output.unlink(missing_ok=True)
            raise SystemExit("Created archive contains forbidden __MACOSX metadata")

    print(f"Created and validated: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
