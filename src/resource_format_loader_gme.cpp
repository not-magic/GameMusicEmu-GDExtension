#include "resource_format_loader_gme.h"

#include "audio_stream_gme.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

namespace {

struct FormatEntry {
	const char *extension;
	AudioStreamGME::Format format;
};

constexpr FormatEntry format_table[] = {
	{ "nsf", AudioStreamGME::FORMAT_NSF },
	{ "nsfe", AudioStreamGME::FORMAT_NSFE },
	{ "gbs", AudioStreamGME::FORMAT_GBS },
	{ "spc", AudioStreamGME::FORMAT_SPC },
	{ "vgm", AudioStreamGME::FORMAT_VGM },
	{ "gym", AudioStreamGME::FORMAT_GYM },
	{ "hes", AudioStreamGME::FORMAT_HES },
	{ "kss", AudioStreamGME::FORMAT_KSS },
	{ "sap", AudioStreamGME::FORMAT_SAP },
	{ "ay", AudioStreamGME::FORMAT_AY },
};

bool find_format(const String &p_path, AudioStreamGME::Format &r_format) {
	const String extension = p_path.get_extension().to_lower();
	for (const FormatEntry &entry : format_table) {
		if (extension == entry.extension) {
			r_format = entry.format;
			return true;
		}
	}
	return false;
}

} // namespace

PackedStringArray ResourceFormatLoaderGME::_get_recognized_extensions() const {
	PackedStringArray extensions;
	for (const FormatEntry &entry : format_table) {
		extensions.push_back(entry.extension);
	}
	return extensions;
}

bool ResourceFormatLoaderGME::_handles_type(const StringName &p_type) const {
	return ClassDB::is_parent_class(p_type, "AudioStream");
}

String ResourceFormatLoaderGME::_get_resource_type(const String &p_path) const {
	AudioStreamGME::Format format;
	return find_format(p_path, format) ? "AudioStreamGME" : "";
}

Variant ResourceFormatLoaderGME::_load(const String &p_path, const String &p_original_path, bool p_use_sub_threads, int32_t p_cache_mode) const {
	AudioStreamGME::Format format;
	if (!find_format(p_path, format)) {
		return Variant(static_cast<int>(ERR_FILE_UNRECOGNIZED));
	}

	const Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	if (file.is_null()) {
		return Variant(static_cast<int>(ERR_FILE_CANT_OPEN));
	}

	Ref<AudioStreamGME> stream;
	stream.instantiate();
	stream->set_format(format);
	stream->set_data(file->get_buffer(file->get_length()));
	return stream;
}
