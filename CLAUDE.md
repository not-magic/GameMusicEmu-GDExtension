# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## State of the repo

This is a Godot GDExtension that started from a starter template and has been renamed to `game_music_emu`. When renaming, `ADDON_NAME` in `SConstruct`, `project/addons/game_music_emu/game_music_emu.gdextension` (library paths and `entry_symbol`), and `src/register_types.cpp` (`game_music_emu_library_init` and the init/uninit function names) must all stay in sync.

## Build / test / tooling (SCons)

`godot-cpp` is a git submodule (branch 4.5); run `git submodule update --init --recursive` first.

- `scons platform=linux target=template_debug` builds the extension to `project/addons/game_music_emu/bin/`. The output filename must match the paths in the `.gdextension` file.
- `scons tests` builds and runs the native tests in `tests/*.cpp`. They are engine-independent, with no godot-cpp linking and no running Godot. Only pure logic in `src/` that doesn't include godot-cpp can be tested this way. There is no per-test runner. Each test is a plain `main()` with an `expect()` helper, and `tests/*.cpp` are all linked into one program.
- `scons format` runs clang-format in place over `src/` and `tests/`. `scons tidy` runs clang-tidy over `src/*.cpp`. Both need a recent LLVM (distro clang 14 can't parse the config files). `pip install clang-format clang-tidy` works, or set `CLANG_FORMAT` / `CLANG_TIDY`.
- `scons docs` regenerates `doc_classes/*.xml` by running `flatpak run org.godotengine.Godot --doctool` from `project/`, then regenerates the `docs/*.md` wiki pages. This needs a `template_debug` build and the flatpak Godot editor. `scons update_wiki` only does the XML to Markdown step via `tools/generate_docs.py`.
- `editor` and `template_debug` builds embed `doc_classes/*.xml` through `GodotCPPDocData` (generated `src/gen/doc_data.gen.cpp`). Release builds don't.

## Architecture

- `game-music-emu/` is a git submodule. `SConstruct` compiles its sources directly (not via its CMake) into a static lib at `game-music-emu/build/`, which is linked into the extension. All emulators are enabled with the Nuked YM2612 core. zlib is not linked, so VGZ and other compressed formats are unsupported. Add new gme source files to the explicit list in `SConstruct`. Include as `<gme/gme.h>`.

- `src/register_types.cpp` is the extension entry point. Classes are registered with `GDREGISTER_CLASS` at `MODULE_INITIALIZATION_LEVEL_SCENE`. A new class needs a `src/` .cpp/.h pair, a registration line here, and a `doc_classes/<Class>.xml`. All `src/*.cpp` files are globbed automatically.
- `project/` is a Godot test project that loads the built addon, for manual checking in the editor.
- CI (`.github/workflows/`): `build.yml` runs `scons ... target=template_debug` plus `scons tests` on Linux/Windows/macOS. `release.yml` zips the addon on `vX.X.X` tags. `wiki.yml` publishes the generated docs to the GitHub wiki (this needs "Read and write" workflow permissions and one dummy wiki page).
- Music formats: `ResourceFormatLoaderGME` imports every supported extension (nsf, nsfe, gbs, spc, vgm, gym, hes, kss, sap, ay) as a single `AudioStreamGME`, setting its `format` enum from the extension. `AudioStreamGME::create_emu()` switches on the format to build the matching gme `Music_Emu`, loading through a `Mem_File_Reader` (`load_mem()` skips track-table setup for single-track formats like SPC and crashes). The stream only holds file bytes and read-only metadata. Per-player options (`track_index`, `looping`) are Godot stream parameters: `AudioStreamGME::_get_parameter_list()` declares them (they appear as `parameters/...` on `AudioStreamPlayer`) and `AudioStreamPlaybackGME::_set_parameter()` receives them, possibly before `_start()` or live during playback. To add a format, extend `AudioStreamGME::Format`, `create_emu()`, `_get_stream_name()`, and the loader's table.
