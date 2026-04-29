# FORGE7 Architecture Overview

This document is a high-level map of the current FORGE7 codebase and runtime behavior.
It is meant to answer "where does feature X live?" and "what is the safe path for doing Y?"

## Goals and non-goals

FORGE7 is a JUCE-based standalone VST3 host aimed at an embedded pedal UX:
- touch-first 7-inch screen
- encoder-first navigation
- minimal reliance on desktop OS dialogs

Non-goals for the current milestone:
- Windows-only VST compatibility
- realtime tuning inside VMs
- plugin sandboxing/bridging (future)

## Core subsystems (AppContext graph)

`Source/App/AppContext.h` is the non-owning service bundle passed to UI components.
Instances are owned by `ForgeApplication` and wired into `MainComponent`.

Key nodes:
- `AudioEngine`: audio device + callback wiring (no plugin loads on the audio thread)
- `PluginHostManager`: plugin scanning, instantiation, and hosting
- `SceneManager`: domain model for Project/Scene/Chain selection
- `ProjectSerializer`: JSON save/load and host hydration
- `ProjectSession`: high-level controller for safe navigation and dirty state
- `ControlManager`: normalization boundary for hardware events (MIDI / simulated / future USB)
- `ParameterMappingManager`: K1-K4 + Button 1 / Button 2 (`HardwareControlId::AssignButton1` / `AssignButton2`) to plugin parameter bindings (scene/chain scoped)
- `EncoderNavigator`: focus ring + encoder navigation routing for touch surfaces

## Domain model: Project > Scene > Chain

User-facing terminology is:
- Project -> Scenes -> Chains

Internally, some types still use legacy naming like `ChainVariation`.
Developers should prefer user-facing "Chain" in new UI text and docs.

### Chain switching and safety

Preferred path is through `ProjectSession`:
- capture live chain into the model
- switch scene/chain selection in `SceneManager`
- hydrate the new chain into the live host (`PluginHostManager`)
- update dirty state + UI

Relevant entry points:
- `ProjectSession::nextChain()`, `previousChain()`, `switchToChain(...)`
- `ProjectSession::nextScene()`, `previousScene()`, `switchToScene(...)`

## Plugin hosting model

`PluginHostManager` owns one or more `PluginChain` instances and coordinates:
- scanning VST3 directories
- instantiating plugins on the message/host thread
- hydrating a chain snapshot into the live chain (best-effort)

`PluginChain` is an ordered set of fixed slots (see `PluginSlot`).
Slots hold:
- plugin metadata (`PluginDescription`)
- bypass state
- hosted instance pointer
- persisted state for project save/load

## Project persistence

Project files are JSON (`.forgeproject`).

Writer/reader:
- `Source/Storage/ProjectSerializer.*`

Save flow:
- optionally capture live rack into the model for the active scene/chain
- serialize scenes, chains, plugin snapshots, and global parameter mappings

Load flow:
- parse JSON into the model
- instantiate/hydrate the active chain into the host
- restore mappings

## Controls: normalized hardware event pipeline

All physical or simulated control inputs produce `HardwareControlEvent` and go through:
- `ControlManager::submitHardwareEvent(...)`

Routing:
- Mapping and listeners are delivered on the JUCE message thread.
- Knob + Button 1 / Button 2 events flow into `ParameterMappingManager`.
- Encoder events flow into `EncoderNavigator`.

### K1-K4: relative encoder direction

K1-K4 are intended to behave like relative encoders:
- events use `HardwareControlType::RelativeDelta`
- delta is applied to the current plugin parameter normalized value
- chain switching should not push any saved "knob position" into plugins

Hydration safety:
- `ProjectSerializer` temporarily suppresses K1-K4 parameter writes during hydration to avoid echo/jumps.

## Encoder navigation model

`EncoderNavigator` maintains:
- a root focus chain (Performance vs Rack)
- an optional modal focus chain (fullscreen plugin browser, Jump Browser, modals)

When a modal focus chain is active, encoder rotate/press operate only within that modal chain.

Long press behavior is routed through `AppContext::tryConsumeEncoderLongPress` (set by `MainComponent`) to implement:
- Jump Browser open/close
- modal dismiss/back behavior

## GUI surfaces

Major surfaces (message thread):
- `PerformanceViewComponent`: live-oriented view + chain/scene switching
- `RackViewComponent`: chain editing surface + plugin browser overlay
- `FullscreenPluginEditorComponent`: embedded plugin editor view
- `SettingsComponent`: audio and scanning configuration
- `ProjectSceneBrowserComponent`: Jump Browser overlay (Project/Scene/Chain navigation)
- `SimulatedControlsComponent`: development-only simulated hardware panel

## Guardrails

- Do not load plugins on the audio thread.
- Do not allocate/log in the audio callback.
- Avoid native file dialogs for normal embedded workflows.
- Prefer `ProjectSession` for scene/chain navigation.
- Keep UI strings ASCII-only (see `scripts/check_ascii_ui_text.py`).

