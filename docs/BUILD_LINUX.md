# FORGE 7 — Linux build and validation

Use this for **CMake compile checks** and **VM smoke tests**. Real-time latency is only meaningful on **bare-metal x86 Linux** or the **LattePanda Mu** target, not in a VM.

## Milestone 1 verification (desktop)

1. Configure and build (Release):

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   ```

2. Run the standalone binary (path matches JUCE’s `juce_add_gui_app` output):

   ```bash
   ./build/Forge7_artefacts/Release/FORGE\ 7
   ```

3. Confirm **Performance** view: scene title, meters, **audio health** line (callback delta), and **CPU** placeholder. With an audio interface, input/output meters should move; you should hear **pass-through** when the global FX bypass path matches your routing.

4. **macOS**: same steps; app bundle at `build/Forge7_artefacts/Release/FORGE 7.app` — use `open` to launch.

## Typical Debian/Ubuntu packages (JUCE GUI + ALSA)

Install build tools and dependencies (names vary slightly by distro):

- `build-essential`, `cmake`, `pkg-config`
- `libasound2-dev` (ALSA)
- `libfreetype6-dev`, `libfontconfig1-dev`
- `libx11-dev`, `libxcomposite-dev`, `libxext-dev`, `libxinerama-dev`, `libxrandr-dev`, `libxcursor-dev`, `libxrender-dev`
- `libgtk-3-dev` (some JUCE / desktop integrations)

Optional for later desktop file dialogs / WebKit (currently disabled in CMake via `JUCE_WEB_BROWSER=0`):

- `webkit2gtk-4.0` / dev package if you enable browser features.

## VST3 discovery on Linux

- Default user path is often `~/.vst3`.
- System paths may include `/usr/lib/vst3` and `/usr/local/lib/vst3`.
- `PluginHostManager::addStandardPlatformScanDirectories()` aligns with common layouts; extend scan dirs via app config / UI when wired.

## Configuration file

At runtime the app loads **`forge7_config.json`** from the OS application-data area (see `AppConfig::getDefaultConfigFile()`). First launch seeds defaults; edit or extend fields as features land.
