# FORGE7

## ASCII-only UI text policy (temporary)

FORGE7 currently keeps all visible UI strings ASCII-only to avoid UTF-8/mojibake issues across macOS, Linux, and embedded builds.

Before committing UI string changes, run:

```bash
python3 scripts/check_ascii_ui_text.py
```

