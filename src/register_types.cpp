#include "register_types.h"

#include "audio_stream_gme.h"
#include "resource_format_loader_gme.h"

#include <gdextension_interface.h>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

using namespace godot;

namespace {

Ref<ResourceFormatLoaderGME> gme_loader;

} // namespace

void initialize_game_music_emu_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(AudioStreamGME);
	GDREGISTER_CLASS(AudioStreamPlaybackGME);
	GDREGISTER_CLASS(ResourceFormatLoaderGME);

	gme_loader.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(gme_loader);
}

void uninitialize_game_music_emu_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	ResourceLoader::get_singleton()->remove_resource_format_loader(gme_loader);
	gme_loader.unref();
}

extern "C" {
GDExtensionBool GDE_EXPORT game_music_emu_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, const GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	const godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize_game_music_emu_module);
	init_obj.register_terminator(uninitialize_game_music_emu_module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
}
