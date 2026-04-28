#pragma once

// FORGE7 UI text should remain ASCII-only for now.
// This avoids UTF-8/mojibake issues across macOS, Linux, and embedded builds.
//
// Quick check for non-ASCII in Source (recommended):
//   grep -RIn "[^[:ascii:]]" Source
//
// If your grep doesn't support that character class, use Python:
//   python3 - <<'PY'
//   from pathlib import Path
//   for p in Path("Source").rglob("*"):
//       if p.suffix.lower() not in [".cpp", ".h", ".hpp"]:
//           continue
//       text = p.read_text(errors="ignore")
//       for i, line in enumerate(text.splitlines(), 1):
//           if any(ord(ch) > 127 for ch in line):
//               print(f"{p}:{i}: {line}")
//   PY

