# FORGE7 User Workflows (Developer Reference)

This doc describes the expected in-app workflows and what "done" looks like for the current milestone.
It is written for developers validating behavior on macOS while targeting an embedded UX.

## Build a chain (Rack/Edit Mode)

1. Launch the app.
2. Switch to Rack/Edit mode (from Performance).
3. Use the Add Plugin card to open the in-app plugin browser.
4. Choose a VST3 plugin.
5. Verify the plugin appears as a slot/card in the chain.
6. Open the plugin editor (embedded fullscreen surface).
7. Toggle per-plugin bypass and verify audio changes.

Expected:
- Plugins are loaded off the audio thread (no audio callback stutters from plugin instantiation).
- Chain is audible and responds to bypass.

## Save a project (Project library)

1. Ensure the current project has at least one Scene and Chain with some plugin state.
2. Trigger Save Project.
3. Enter a project name in the in-app modal.
4. Confirm the project is written to the library folder.

Expected:
- Save stores the entire project (all scenes and chains).
- Save does not require a native OS file picker for the normal flow.
- Unsaved changes indicator clears (unless other edits remain).

Storage (macOS dev):
- `~/Documents/FORGE7/Projects/*.forgeproject`

## Load a project (Project library)

1. Trigger Load Project.
2. Choose a saved project from the in-app browser/list.
3. Confirm the active scene/chain is hydrated and audible.

Expected:
- Plugin instances hydrate best-effort; missing plugins show placeholder/missing state rather than crashing.
- Mappings restore for the loaded project.

## Switch chain (Performance Mode)

1. In Performance mode, press Chain + / Chain - (or use simulated hardware).
2. Repeat several times to confirm wrap behavior at ends.

Expected:
- Switching chains captures outgoing live state into the model, then hydrates the incoming chain.
- Switching chains should not require saving.
- Switching chains alone should not mutate mapped plugin parameters for K1-K4 (no jumps).

## Switch scene (Performance Mode)

1. Use Scene - / Scene + controls in Performance.

Expected:
- Similar to chain switching: outgoing capture, selection update, incoming hydrate (current milestone may log TODOs for preload).

## Assign K1-K4 to a plugin parameter (assignment workflow)

1. In Rack/Edit, open a plugin editor.
2. Enable Assign mode (if available in that surface).
3. Select a plugin parameter to bind.
4. Move K1 (simulated relative +/- or hardware input).

Expected:
- The mapping is stored for the active scene/chain.
- The K display updates to show the parameter name and the current loaded value.
- The parameter value should not jump just because assignment occurred.

## Use the Jump Browser (encoder long press)

1. Long press the encoder.
2. Verify the Jump Browser overlay appears full screen.
3. Rotate encoder to move through rows; press to expand/select.
4. Long press again to close.

Expected:
- Encoder focus becomes modal while the overlay is open.
- Background controls do not receive encoder focus while Jump Browser is open.

## Simulated hardware (development-only)

The simulated controls exist to validate the embedded control pipeline before physical hardware is wired.

Expected mappings:
- Encoder rotate/press/long press behaves like the physical encoder.
- Assign buttons and Chain +/- buttons behave like physical buttons.
- K1-K4 behave like **relative encoders** by default.

Dev-only absolute knob test:
- Optional debug setting exposes absolute 0...1 knob sliders for diagnostics.
- Normal validation should use relative controls to avoid parameter jumps.

