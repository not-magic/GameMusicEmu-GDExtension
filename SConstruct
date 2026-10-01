#!/usr/bin/env python
import os
import sys

# You can find documentation for SCons and SConstruct files at:
# https://scons.org/documentation.html

ADDON_NAME = 'game_music_emu'


# This lets SCons know that we're using godot-cpp, from the godot-cpp folder.
env = SConscript("godot-cpp/SConstruct")

# Configures the 'src' directory as a source for header files.
env.Append(CPPPATH=["src/"])

# Game Music Emu (game-music-emu/ submodule), built as a static library from
# the same source list as gme/CMakeLists.txt with every emulator enabled
# (see gme/gme_types.h), the Nuked YM2612 core, and no zlib (so no VGZ).
GME_DIR = "game-music-emu/gme"
env.Append(CPPPATH=["game-music-emu/"])
# Every Godot target is little-endian; gme headers require it to be stated.
# VGM_YM2612_NUKED must be defined for both the library and its users.
env.Append(CPPDEFINES=[("BLARGG_LITTLE_ENDIAN", 1), "VGM_YM2612_NUKED"])

gme_env = env.Clone()
gme_env.Append(CPPPATH=[GME_DIR])
if env.get("is_msvc", False):
    gme_env.Append(CPPDEFINES=["_CRT_SECURE_NO_WARNINGS"])

gme_sources = [
    GME_DIR + "/" + name
    for name in [
        "Ay_Apu.cpp", "Ay_Cpu.cpp", "Ay_Emu.cpp",
        "Blip_Buffer.cpp", "Classic_Emu.cpp", "Data_Reader.cpp", "Dual_Resampler.cpp",
        "Effects_Buffer.cpp", "Fir_Resampler.cpp", "Gb_Apu.cpp", "Gb_Cpu.cpp", "Gb_Oscs.cpp",
        "Gbs_Emu.cpp", "gme.cpp", "Gme_File.cpp", "Gym_Emu.cpp",
        "Hes_Apu.cpp", "Hes_Apu_Adpcm.cpp", "Hes_Cpu.cpp", "Hes_Emu.cpp",
        "Kss_Cpu.cpp", "Kss_Emu.cpp", "Kss_Scc_Apu.cpp", "M3u_Playlist.cpp",
        "Multi_Buffer.cpp", "Music_Emu.cpp",
        "Nes_Apu.cpp", "Nes_Cpu.cpp", "Nes_Fds_Apu.cpp", "Nes_Fme7_Apu.cpp",
        "Nes_Namco_Apu.cpp", "Nes_Oscs.cpp", "Nes_Vrc6_Apu.cpp", "Nes_Vrc7_Apu.cpp",
        "Nsf_Emu.cpp", "Nsfe_Emu.cpp", "ext/emu2413.c",
        "Sap_Apu.cpp", "Sap_Cpu.cpp", "Sap_Emu.cpp", "Sms_Apu.cpp",
        "Snes_Spc.cpp", "Spc_Cpu.cpp", "Spc_Dsp.cpp", "Spc_Emu.cpp", "Spc_Filter.cpp",
        "Vgm_Emu.cpp", "Vgm_Emu_Impl.cpp", "Ym2413_Emu.cpp", "Ym2612_Nuked.cpp",
    ]
]

gme_objects = [
    gme_env.SharedObject(
        "game-music-emu/build/{}{}".format(os.path.splitext(src[len(GME_DIR) + 1:])[0], env["suffix"]),
        src,
    )
    for src in gme_sources
]
gme_library = gme_env.StaticLibrary(
    "game-music-emu/build/gme{}".format(env["suffix"]),
    source=gme_objects,
)

# Collects all .cpp files in the 'src' folder as compile targets.
sources = Glob("src/*.cpp")

if env["target"] in ["editor", "template_debug"]:
    try:
        doc_data = env.GodotCPPDocData("src/gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml"))
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference as we're targeting a pre-4.3 baseline.")

# The filename for the dynamic library for this GDExtension.
# $SHLIBPREFIX is a platform specific prefix for the dynamic library ('lib' on Unix, '' on Windows).
# $SHLIBSUFFIX is the platform specific suffix for the dynamic library (for example '.dll' on Windows).
# env["suffix"] includes the build's feature tags (e.g. '.windows.template_debug.x86_64')
# (see https://docs.godotengine.org/en/stable/tutorials/export/feature_tags.html).
# The final path should match a path in the '.gdextension' file.
lib_filename = "{}{}{}{}".format(env.subst('$SHLIBPREFIX'), ADDON_NAME, env["suffix"], env.subst('$SHLIBSUFFIX'))

# Creates a SCons target for the path with our sources.
library = env.SharedLibrary(
    "project/addons/{}/bin/{}".format(ADDON_NAME, lib_filename),
    source=sources,
    LIBS=env.get("LIBS", []) + [gme_library],
)

# Selects the shared library as the default target.
Default(library)

# --- Unit tests (tests/) ---
# Builds and runs a native, engine-independent test program (no godot-cpp
# linking, no running Godot process required) for any pure logic added to
# src/. Uses its own native Environment rather than the (possibly
# cross-compiling) `env` above, since the test binary needs to run on this
# machine.
test_env = Environment()
test_env.Append(CPPPATH=["src/"])
if test_env["CXX"] == "cl":
    test_env.Append(CXXFLAGS=["/std:c++17"])
else:
    test_env.Append(CXXFLAGS=["-std=c++17"])

test_program = test_env.Program("tests/bin/tests", Glob("tests/*.cpp"))
run_tests = test_env.Alias("tests", test_program, test_program[0].abspath)
AlwaysBuild(run_tests)

# --- Formatting and linting (.clang-format, .clang-tidy) ---
# `scons format` rewrites src/ and tests/ in place with clang-format;
# `scons tidy` runs clang-tidy over every src/*.cpp (plus headers under src/) with
# the same include paths/defines the real build uses. Point CLANG_FORMAT /
# CLANG_TIDY at a specific binary to override the one found on PATH. The
# .clang-format/.clang-tidy files need a recent LLVM (distro clang 14 can't
# parse them); `pip install clang-format clang-tidy` provides one.
import subprocess

godot_env = env
lint_sources = sorted(str(f) for f in Glob("src/*.cpp") + Glob("src/*.h") + Glob("tests/*.cpp"))


def run_format(target, source, env):
    tool = os.environ.get("CLANG_FORMAT", "clang-format")
    return subprocess.call([tool, "-i", "--style=file"] + lint_sources)


def run_tidy(target, source, env):
    tool = os.environ.get("CLANG_TIDY", "clang-tidy")
    status = 0
    for path in lint_sources:
        if not path.endswith(".cpp") or path.startswith("tests"):
            continue
        compile_args = ["-I" + str(d) for d in godot_env["CPPPATH"]] + ["-std=c++17"]
        for define in godot_env["CPPDEFINES"]:
            if isinstance(define, (tuple, list)):
                compile_args.append("-D{}={}".format(*define))
            else:
                compile_args.append("-D" + str(define))
        status |= subprocess.call([tool, "--quiet", "--header-filter=.*/src/.*", path, "--"] + compile_args)
    return status


format_sources = Command("format", None, run_format)
AlwaysBuild(format_sources)

tidy_sources = Command("tidy", None, run_tidy)
AlwaysBuild(tidy_sources)

# --- Docs update (doc_classes/) ---
# Regenerates doc_classes/*.xml from the classes' _bind_methods() by loading
# the built extension into Godot's --doctool. Requires a template_debug build
# (with doc data compiled in, see the GodotCPPDocData block above) and a
# flatpak install of the Godot editor (org.godotengine.Godot). Run with
# `scons docs`. Runs from project/ since --doctool needs a Godot project
# (project.godot) to load the extension into.
update_docs = Command(
    "update_docs",
    None,
    "flatpak run org.godotengine.Godot --doctool ../ --gdextension-docs",
    chdir="project",
)
AlwaysBuild(update_docs)

# --- Wiki docs (docs/) ---
# Regenerates docs/*.md (GitHub wiki pages) from doc_classes/*.xml via
# tools/generate_docs.py. Not part of the default build -- run explicitly
# with `scons update_wiki` (against whatever doc_classes/*.xml is currently
# on disk), or via `scons docs`, which also regenerates that XML first so
# the wiki pages never drift from it. Split into two Command nodes so a
# plain build/`scons update_wiki` never pulls in the flatpak --doctool step
# above.
wiki_action = "{} tools/generate_docs.py --src doc_classes --out docs".format(sys.executable)

update_wiki = Command("update_wiki", None, wiki_action)
AlwaysBuild(update_wiki)

update_wiki_after_docs = Command("update_wiki_after_docs", None, wiki_action)
AlwaysBuild(update_wiki_after_docs)
Requires(update_wiki_after_docs, update_docs)

docs_alias = Alias("docs", [update_docs, update_wiki_after_docs])
