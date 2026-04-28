from pathlib import Path
import sys


def main() -> int:
    extensions = {".cpp", ".h", ".hpp", ".mm", ".cxx"}
    bad = []

    root = Path(__file__).resolve().parent.parent
    source = root / "Source"

    for p in source.rglob("*"):
        if p.suffix.lower() not in extensions:
            continue

        text = p.read_text(errors="replace")

        for i, line in enumerate(text.splitlines(), 1):
            bad_chars = sorted({ch for ch in line if ord(ch) > 127})
            if bad_chars:
                chars = " ".join(f"U+{ord(ch):04X}({repr(ch)[1:-1]})" for ch in bad_chars)
                bad.append((p, i, chars, line))

    if bad:
        print("Non-ASCII characters found:")
        for p, i, chars, line in bad:
            print(f"{p}:{i}: {chars}: {line}")
        return 1

    print("ASCII UI/source text check passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

