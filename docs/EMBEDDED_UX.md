# Embedded UX Principles (FORGE7)

FORGE7 is designed around an embedded pedal UI:
- 7-inch touchscreen
- rotary encoder with press + long press
- four assignable encoders (K1-K4)
- Button 1 and Button 2 (programmable; planned hardware uses a blue LED marker on Button 1 and a yellow/amber LED on Button 2 so players can tell them apart at a distance)
- chain switching buttons

The macOS app is a development and validation harness for that UX.

## Primary principles

- Touch-first: all normal workflows must work with touch.
- Encoder-friendly: all important workflows should work with the encoder.
- Modal safety: overlays and modals must take encoder focus exclusively.
- No desktop assumptions: no required mouse/keyboard for normal operation.
- Avoid native OS dialogs for normal workflows.

## Surfaces and navigation expectations

Performance mode:
- optimized for live use
- large touch targets
- chain/scene switching is the primary action

Rack/Edit mode:
- editing surface for the active chain
- still usable on touchscreen (large cards, clear actions)

Fullscreen plugin editor:
- embedded surface (do not depend on external plugin windows)
- may require future work around scaling/panning on small displays

Settings:
- configuration and diagnostics

Jump Browser:
- global navigation overlay for Project/Scene/Chain
- opened via encoder long press
- supports touch and encoder

Simulated hardware:
- development-only validation tool for the embedded control pipeline

## Encoder rules (focus + modal behavior)

- Rotate: move focus or adjust focused item (depending on focus target).
- Press: activate focused item.
- Long press: global back/navigation behavior.

Modal focus:
- When an overlay or modal is open, it must install a modal focus chain in `EncoderNavigator`.
- Root focus chains (Performance/Rack) must not override modal focus while a modal is open.
- When the modal closes, restore the root focus chain.

Long press priority (typical):
- If Jump Browser open: close it
- Else close other overlays first (fullscreen editor, settings, plugin browser)
- Else open Jump Browser

## File dialogs

Normal embedded workflows should not depend on native OS file pickers:
- Save/load should use the in-app project library flow.
- Name entry should use an in-app modal.

Native file dialogs are acceptable for explicit development workflows:
- import/export
- diagnostics

## Text policy

User-visible UI strings are kept ASCII-only for portability across macOS, Linux, and embedded environments.
Before committing UI string changes, run:

```bash
python3 scripts/check_ascii_ui_text.py
```

