# FORGE7

FORGE7 is an embedded touchscreen guitar pedal. Projects are organized as:

**Project > Scenes > Chains**

- **Project** - artist/band/performance context. Holds a name and one or more Scenes.
- **Scene** - one song. Has a name, tempo, and an ordered list of Chains.
- **Chain** - a sound/section within a Scene (e.g. Intro, Verse, Chorus, Solo). Holds the plugin chain, plugin states, bypass states, and K1-K4 mappings.

Chain navigation uses the dedicated **Chain -** / **Chain +** controls (which wrap when reaching the first/last chain in the active scene).

> Internal class names (e.g. `ChainVariation`) still use the legacy "variation" terminology in V1; user-facing UI uses **Chain**.

## ASCII-only UI text policy (temporary)

FORGE7 currently keeps all visible UI strings ASCII-only to avoid UTF-8/mojibake issues across macOS, Linux, and embedded builds.

Before committing UI string changes, run:

```bash
python3 scripts/check_ascii_ui_text.py
```

## macOS microphone / audio input permission

FORGE7 requests microphone-style audio input access so Core Audio can capture live input (guitar, interface, etc.).

On macOS, enable input access under **System Settings > Privacy & Security > Microphone**. Depending on launch method, permission may appear under **FORGE 7**, **Terminal**, **Cursor**, or **Xcode**.

After building, confirm the usage string is embedded:

```bash
plutil -p "build/Forge7_artefacts/Release/FORGE 7.app/Contents/Info.plist" | grep Microphone
```

