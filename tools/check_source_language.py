from __future__ import annotations

import pathlib
import re
import sys

ARABIC = re.compile(r"[\u0600-\u06FF\u0750-\u077F\u08A0-\u08FF]")
SOURCE_SUFFIXES = {
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx",
    ".cmake", ".py", ".sh", ".ps1", ".bat"
}
SOURCE_NAMES = {"CMakeLists.txt"}


def is_source(path: pathlib.Path) -> bool:
    return path.suffix.lower() in SOURCE_SUFFIXES or path.name in SOURCE_NAMES


def main() -> int:
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    violations: list[str] = []

    for path in root.rglob("*"):
        if not path.is_file() or not is_source(path):
            continue
        if any(part in {".git", "build", "out"} for part in path.parts):
            continue

        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue

        if ARABIC.search(text):
            violations.append(str(path.relative_to(root)))

    if violations:
        print("Arabic characters found in source files:")
        for item in violations:
            print(f"  {item}")
        return 1

    print("Source language check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
