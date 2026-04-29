# FORGE7

FORGE7 is a JUCE-based standalone VST host prototype for an embedded guitar pedal / rack-style effects processor.
It is designed for an x86 Linux target such as the LattePanda Mu, while currently being developed and validated on macOS.

The app hosts VST3 plugins, builds effect chains, stores them in a **Project > Scene > Chain** hierarchy, and supports a touch/encoder-oriented UX for a future 7-inch touchscreen pedal.

## What works today

- Live audio input/output through an audio interface
- VST3 scanning/loading and live processing
- Rack-style plugin chain editing (add/replace/remove/details)
- Plugin bypass and global bypass
- Project library save/load (in-app name entry for normal save)
- Scene and Chain switching (Chain - / Chain +)
- Performance mode (live-oriented view)
- Simulated hardware controls (development-only)
- Global Jump Browser (Project/Scene/Chain overlay) via encoder long press
- Assignable K1-K4 controls (in progress / being refined as relative encoders)

## FORGE7 Organization Model

**Project**
- Top-level workspace (band/artist/set/performance context).
- Contains multiple Scenes.

**Scene**
- Represents a song.
- Has a scene name and tempo.
- Contains one or more Chains, with an active Chain index.

**Chain**
- Represents a song section or sound state (Intro / Verse / Chorus / Solo).
- Contains plugin slots, plugin states, bypass states, and K1-K4 mappings.
- Chain - and Chain + switch between Chains in the active Scene (wraps at ends).

Example:

Project: My Band
  Scene: Song One
    Chain 01 - Intro
    Chain 02 - Verse
    Chain 03 - Chorus
  Scene: Song Two
    Chain 01 - Clean
    Chain 02 - Lead

Note: older internal code may still use **ChainVariation** in places. User-facing terminology is **Chain**.

## Current UI Surfaces

**Performance Mode**
- Primary live view.
- Shows project, scene, active chain, K1-K4 assignments/value text, BPM/CPU/status.
- Main input and output peak meters only (no per-slot meters); clip warnings match Rack metering.
- Designed for live use and fast chain/scene switching.

**Rack/Edit Mode**
- Build/edit the active Chain.
- Shows signal flow and plugin cards.
- Peak **VU-style meters** at chain input, after each plugin slot (post-slot), and at output for gain staging. Clip warnings are brief red indicators near ~0.98 FS.
- Plugin browser opens in-app.
- Supports plugin editor access, bypass, replace, remove, and rack navigation.

**Fullscreen Plugin Editor**
- Opens the selected plugin editor inside the app surface.
- Used for deeper plugin editing and the assignment workflow.

**Settings**
- Audio I/O setup, plugin scanning paths, storage info, and development diagnostics.

**Jump Browser**
- Global Project/Scene/Chain navigation overlay.
- Open via encoder long press.
- Supports touch and encoder navigation.

**Simulated Hardware**
- Development-only in-app controls (drawer/window).
- Simulates K1-K4, Button 1 / Button 2, Chain -/+, encoder rotate/press/long press.

## Embedded UX Principles

FORGE7 is intended for a hardware pedal with:
- 7-inch touchscreen
- Large clickable encoder
- Four assignable encoders (K1-K4)
- Two assignable buttons labeled **Button 1** and **Button 2** (planned hardware: blue LED on Button 1, yellow/amber LED on Button 2 for easy stage identification)
- Chain switching buttons
- No required physical keyboard
- No required mouse
- No normal desktop file dialogs for live workflows

Rules:
- Normal workflows must work with touch.
- Important workflows should work with the encoder.
- Native OS dialogs should be avoided for normal save/load/navigation.
- File pickers are acceptable for explicit dev/import/export workflows.
- Save/name entry uses in-app modal UI (and an on-screen keyboard in the embedded direction).
- Encoder long press opens the global Jump Browser or backs out of modal overlays.

## Project Saving and Loading

Current behavior:
- **Save Project** asks only for a project name.
- Project files are saved automatically to the FORGE7 library folder.
- Save writes the entire project, including all scenes/chains and mapping assignments.
- Switching chains/scenes retains edits in memory (save is not required just to switch inside the current project).
- Unsaved changes are tracked.
- When opening another project with unsaved changes, the app warns and allows Save / Discard / Cancel.

Storage (macOS development):
- `~/Documents/FORGE7/Projects` (project library)

Other library folders may exist (development / future expansion):
- `~/Documents/FORGE7/Scenes`
- `~/Documents/FORGE7/Presets`
- `~/Documents/FORGE7/PluginCache`
- `~/Documents/FORGE7/Backups`

Planned Linux/embedded target path:
- `~/.local/share/forge7/`

File extension:
- `.forgeproject`

## Audio I/O

Current working state:
- Live audio input through an audio interface works.
- Output through the selected audio device works.
- VST effects process live audio.
- Bypass and global bypass work.
- Test tone verifies output only.

macOS permission note:
- macOS treats audio interface input as microphone access.
- If input is silent, check System Settings > Privacy & Security > Microphone.
- Permission may attach to FORGE 7, Cursor, Terminal, or Xcode depending on launch method.

After building, confirm the usage string is embedded:

```bash
plutil -p "build/Forge7_artefacts/Release/FORGE 7.app/Contents/Info.plist" | grep Microphone
```

Recommended development setup:
- Guitar/interface input -> FORGE7 -> interface output/headphones

Note:
- For low-latency validation, use a real audio interface and avoid judging realtime audio behavior inside a Linux VM.

## Plugins

Current behavior:
- VST3 plugins can be scanned and added.
- macOS default VST3 path: `/Library/Audio/Plug-Ins/VST3`
- Loaded plugins appear as cards in the rack.

Non-goals (current milestone):
- Windows-only VST compatibility (future Linux strategy may involve Wine/yabridge; not part of the current stable macOS validation path).

## Assignable Controls K1-K4 (relative encoders)

Intended behavior:
- K1-K4 behave like **relative endless encoders**, not absolute potentiometers.
- On chain switch, saved plugin parameter values load intact.
- K1-K4 display rings/labels update to show the active chain's current parameter values.
- Switching chains does not change parameter values by itself.
- Turning K1-K4 applies relative deltas from the current loaded value.
- Unassigned K1-K4 controls are valid and should show Unassigned / --.

Current implementation status:
- Assignment workflow exists.
- Relative/no-jump behavior is being refined.
- Simulated hardware is expected to follow the same relative-control model.

**Button 1 / Button 2** map to discrete or toggle-like plugin parameters (same scene/chain scoping as K1-K4).
Planned hardware uses a blue LED cue on Button 1 and a yellow/amber LED cue on Button 2 so musicians can distinguish them when looking down at the pedal.

## Encoder Navigation

- Rotate encoder: move focus or adjust the focused control.
- Press encoder: activate the focused item.
- Long press: global navigation/back behavior.

Current global long-press behavior:
- Opens the Project/Scene/Chain Jump Browser from most screens.
- If Jump Browser is open, long press closes it.
- If another modal is open, long press may close/back out depending on context.
- Encoder focus becomes modal while overlays are open.

## Build (macOS)

Requirements:
- CMake
- Xcode Command Line Tools
- A C++20-capable compiler (provided by Xcode toolchain)

Build (Release):

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Launch:

```bash
open "build/Forge7_artefacts/Release/FORGE 7.app"
```

Dev checks:

```bash
python3 scripts/check_ascii_ui_text.py
```

Linux build notes are in `docs/BUILD_LINUX.md`.

## Current Milestone Status

Working:
- Standalone JUCE app
- Live audio I/O
- VST3 loading and processing
- Bypass/global bypass
- Rack editor
- Performance view
- Project library save/load
- Project > Scene > Chain model
- Safe chain/scene switching with project state retained
- Global Jump Browser
- Simulated hardware controls

In progress / needs refinement:
- K1-K4 relative encoder behavior and no-jump assignment handling
- UI/UX polish for 7-inch display
- Plugin editor scaling/panning edge cases
- Scene/chain management polish
- Better hardware abstraction for physical controls
- Linux deployment and realtime tuning

Future:
- LattePanda Mu deployment
- Linux low-latency audio stack (PipeWire/JACK/ALSA tuning)
- Hardware GPIO/MIDI/USB integration
- Robust plugin sandboxing/bridging strategy
- Factory presets/templates
- Optional setlist mode (if needed)

## Code Architecture Overview

High-level areas:
- `Source/App`: application shell, `MainComponent`, `AppContext`, `ProjectSession`
- `Source/Audio`: `AudioEngine`, audio I/O, meters/diagnostics
- `Source/PluginHost`: VST hosting, `PluginHostManager`, `PluginChain`, `PluginSlot`
- `Source/Scene`: Scene and Chain domain model (legacy internal naming may reference `ChainVariation`)
- `Source/Storage`: project serialization, storage paths, project metadata
- `Source/Controls`: hardware control events, encoder navigation, parameter mappings, simulated input
- `Source/GUI`: Rack, Performance, Settings, Jump Browser, plugin editor, simulated controls

Preferred navigation path:
- `ProjectSession` coordinates safe scene/chain switching by capturing outgoing live state, switching, hydrating the incoming chain, marking dirty, and driving UI refresh.

## Development Rules / Guardrails

- Do not break live audio input/output.
- Do not load plugins on the audio thread.
- Do not log or allocate in the audio callback.
- Do not use native OS dialogs for normal embedded workflows.
- Do not require a keyboard or mouse for normal operation.
- Keep normal UI touch/encoder friendly.
- Use `ProjectSession` for scene/chain switching.
- Run the ASCII check before committing.
- Keep user-visible UI text ASCII-only unless the policy changes.
- Keep Windows VST support isolated from the core host until the Linux strategy is ready.

## More docs

- `docs/ARCHITECTURE.md`
- `docs/USER_WORKFLOWS.md`
- `docs/EMBEDDED_UX.md`
- `docs/BUILD_LINUX.md`

## ASCII-only UI text policy (temporary)

FORGE7 currently keeps visible UI strings ASCII-only to avoid UTF-8/mojibake issues across macOS, Linux, and embedded builds.

Before committing UI string changes, run:

```bash
python3 scripts/check_ascii_ui_text.py
```

