# GameMusicEmu-GDExtension

A Godot 4.5+ GDExtension that is a wrapper for [Game Music Emu](https://github.com/libgme/game-music-emu). This lets you play chiptune music using emulated sounds of the day.

## Supported formats

Drop a file into your project and Godot imports it as an `AudioStreamGME`:

| Extension | System |
| --- | --- |
| `.nsf`, `.nsfe` | NES / Famicom |
| `.gbs` | Game Boy |
| `.spc` | SNES |
| `.vgm` | Sega and other chips (uncompressed only) |
| `.gym` | Sega Genesis / Mega Drive |
| `.hes` | PC Engine / TurboGrafx-16 |
| `.kss` | MSX and other Z80 systems |
| `.sap` | Atari |
| `.ay` | ZX Spectrum / Amstrad CPC |

## Usage

Assign the imported file as the `stream` of an `AudioStreamPlayer`. You can adjust the 

* `parameters/track_index`: zero-based track to play. Some formats are multi-track, so this lets you select what track you want.
* `parameters/looping`: restart the track when it ends, defaults to true.

```gdscript
$AudioStreamPlayer.set("parameters/track_index", 3)
$AudioStreamPlayer.play()
```

## Building

Requires [SCons](https://scons.org/) and a C++17 compiler. Clone with submodules (`godot-cpp` and `game-music-emu`):

```
git submodule update --init --recursive
scons platform=linux target=template_debug
```

The library is written to `project/addons/game_music_emu/bin/`. The `project/` folder is a Godot test project that loads it.

Other SCons targets:

* `scons tests` builds and runs the native tests in `tests/`
* `scons format` / `scons tidy` run clang-format / clang-tidy
* `scons docs` regenerates `doc_classes/*.xml` and the wiki pages in `docs/` (needs a `template_debug` build and the flatpak Godot editor)

