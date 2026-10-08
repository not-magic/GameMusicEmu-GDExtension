#include "audio_stream_gme.h"

#include <gme/Ay_Emu.h>
#include <gme/Data_Reader.h>
#include <gme/Gbs_Emu.h>
#include <gme/Gym_Emu.h>
#include <gme/Hes_Emu.h>
#include <gme/Kss_Emu.h>
#include <gme/Music_Emu.h>
#include <gme/Nsf_Emu.h>
#include <gme/Nsfe_Emu.h>
#include <gme/Sap_Emu.h>
#include <gme/Spc_Emu.h>
#include <gme/Vgm_Emu.h>
#include <godot_cpp/classes/audio_server.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstring>
#include <vector>

using namespace godot;

namespace {

constexpr long info_sample_rate = 44100;
constexpr long default_play_length_ms = 150 * 1000;
constexpr float sample_to_float = 1.0f / 32768.0f;

struct VgmChip {
	size_t clock_offset;
	const char *name;
};

// Chips with a VGM header clock that Vgm_Emu cannot emulate (it only supports SN76489, YM2413 and YM2612).
constexpr VgmChip unsupported_vgm_chips[] = {
	{ 0x30, "YM2151" }, { 0x38, "SegaPCM" }, { 0x40, "RF5C68" }, { 0x44, "YM2203" },
	{ 0x48, "YM2608" }, { 0x4c, "YM2610" }, { 0x50, "YM3812" }, { 0x54, "YM3526" },
	{ 0x58, "Y8950" }, { 0x5c, "YMF262" }, { 0x60, "YMF278B" }, { 0x64, "YMF271" },
	{ 0x68, "YMZ280B" }, { 0x6c, "RF5C164" }, { 0x70, "PWM" }, { 0x74, "AY8910" },
	{ 0x80, "GameBoy DMG" }, { 0x84, "NES APU" }, { 0x88, "MultiPCM" }, { 0x8c, "uPD7759" },
	{ 0x90, "OKIM6258" }, { 0x98, "OKIM6295" }, { 0x9c, "K051649" }, { 0xa0, "K054539" },
	{ 0xa4, "HuC6280" }, { 0xa8, "C140" }, { 0xac, "K053260" }, { 0xb0, "Pokey" },
	{ 0xb4, "QSound" },
};

uint32_t read_le32(const PackedByteArray &p_data, size_t p_offset) {
	return static_cast<uint32_t>(p_data[p_offset]) | (static_cast<uint32_t>(p_data[p_offset + 1]) << 8) |
			(static_cast<uint32_t>(p_data[p_offset + 2]) << 16) | (static_cast<uint32_t>(p_data[p_offset + 3]) << 24);
}

String find_unsupported_vgm_chips(const PackedByteArray &p_data) {
	constexpr size_t min_header_size = 0x40;
	if (static_cast<size_t>(p_data.size()) < min_header_size) {
		return String();
	}
	const uint32_t version = read_le32(p_data, 0x08);
	const size_t data_start = version >= 0x150 ? 0x34 + read_le32(p_data, 0x34) : min_header_size;
	String names;
	for (const VgmChip &chip : unsupported_vgm_chips) {
		const bool is_in_header = chip.clock_offset + 4 <= data_start && chip.clock_offset + 4 <= static_cast<size_t>(p_data.size());
		if (is_in_header && (read_le32(p_data, chip.clock_offset) & 0x3fffffff) != 0) {
			names += (names.is_empty() ? "" : ", ") + String(chip.name);
		}
	}
	return names;
}

bool is_valid_utf8(const char *p_text) {
	const unsigned char *byte = reinterpret_cast<const unsigned char *>(p_text);
	while (*byte) {
		int continuation_total = 0;
		if (*byte < 0x80) {
			continuation_total = 0;
		} else if (*byte >= 0xC2 && *byte <= 0xDF) {
			continuation_total = 1;
		} else if (*byte >= 0xE0 && *byte <= 0xEF) {
			continuation_total = 2;
		} else if (*byte >= 0xF0 && *byte <= 0xF4) {
			continuation_total = 3;
		} else {
			return false;
		}
		++byte;
		for (int i = 0; i < continuation_total; ++i, ++byte) {
			if ((*byte & 0xC0) != 0x80) {
				return false;
			}
		}
	}
	return true;
}

// Old music files predate UTF-8; fall back to Latin-1 when the bytes aren't valid UTF-8.
String sanitize_string(const char *p_text) {
	String text;
	if (is_valid_utf8(p_text)) {
		text = String::utf8(p_text);
	} else {
		for (const unsigned char *byte = reinterpret_cast<const unsigned char *>(p_text); *byte; ++byte) {
			text += String::chr(*byte);
		}
	}

	String result;
	for (int i = 0; i < text.length(); ++i) {
		const char32_t character = text[i];
		const bool is_control = character < 0x20 || (character >= 0x7F && character < 0xA0);
		if (!is_control) {
			result += String::chr(character);
		}
	}
	return result.strip_edges();
}

long calc_play_length_ms(const track_info_t &p_info) {
	if (p_info.length > 0) {
		return p_info.length;
	}
	const long looped_length = p_info.intro_length + 2 * p_info.loop_length;
	return looped_length > 0 ? looped_length : default_play_length_ms;
}

bool find_track_info(const AudioStreamGME &p_stream, int p_track_index, track_info_t &r_info) {
	const std::unique_ptr<Music_Emu> info_emu = p_stream.create_emu(info_sample_rate);
	return info_emu && !info_emu->track_info(&r_info, p_track_index);
}

Dictionary make_parameter(const String &p_name, Variant::Type p_type, PropertyHint p_hint, const String &p_hint_string, const Variant &p_default_value) {
	Dictionary parameter;
	parameter["name"] = p_name;
	parameter["class_name"] = StringName();
	parameter["type"] = static_cast<int>(p_type);
	parameter["hint"] = static_cast<int>(p_hint);
	parameter["hint_string"] = p_hint_string;
	parameter["usage"] = static_cast<int>(PROPERTY_USAGE_DEFAULT);
	parameter["default_value"] = p_default_value;
	return parameter;
}

} // namespace

// ============================== AudioStreamGME ==============================

String AudioStreamGME::find_unsupported_chips() const {
	return get_format() == FORMAT_VGM ? find_unsupported_vgm_chips(data) : String();
}

std::unique_ptr<Music_Emu> AudioStreamGME::create_emu(long p_sample_rate) const {
	if (data.is_empty()) {
		return nullptr;
	}

	std::unique_ptr<Music_Emu> emu;
	switch (get_format()) {
		case FORMAT_NSF:
			emu = std::make_unique<Nsf_Emu>();
			break;
		case FORMAT_NSFE:
			emu = std::make_unique<Nsfe_Emu>();
			break;
		case FORMAT_GBS:
			emu = std::make_unique<Gbs_Emu>();
			break;
		case FORMAT_SPC:
			emu = std::make_unique<Spc_Emu>();
			break;
		case FORMAT_VGM:
			emu = std::make_unique<Vgm_Emu>();
			break;
		case FORMAT_GYM:
			emu = std::make_unique<Gym_Emu>();
			break;
		case FORMAT_HES:
			emu = std::make_unique<Hes_Emu>();
			break;
		case FORMAT_KSS:
			emu = std::make_unique<Kss_Emu>();
			break;
		case FORMAT_SAP:
			emu = std::make_unique<Sap_Emu>();
			break;
		case FORMAT_AY:
			emu = std::make_unique<Ay_Emu>();
			break;
		default:
			UtilityFunctions::push_error("AudioStreamGME: unknown format ", format_id);
			return nullptr;
	}

	if (const char *error = emu->set_sample_rate(p_sample_rate)) {
		UtilityFunctions::push_error("AudioStreamGME: ", String(error));
		return nullptr;
	}
	Mem_File_Reader reader(data.ptr(), data.size());
	if (const char *error = emu->load(reader)) {
		UtilityFunctions::push_error("AudioStreamGME: failed to load data: ", String(error));
		return nullptr;
	}
	return emu;
}

String AudioStreamGME::_get_stream_name() const {
	switch (get_format()) {
		case FORMAT_NSF:
			return "NSF";
		case FORMAT_NSFE:
			return "NSFE";
		case FORMAT_GBS:
			return "GBS";
		case FORMAT_SPC:
			return "SPC";
		case FORMAT_VGM:
			return "VGM";
		case FORMAT_GYM:
			return "GYM";
		case FORMAT_HES:
			return "HES";
		case FORMAT_KSS:
			return "KSS";
		case FORMAT_SAP:
			return "SAP";
		case FORMAT_AY:
			return "AY";
		default:
			return "GME";
	}
}

Ref<AudioStreamPlayback> AudioStreamGME::_instantiate_playback() const {
	Ref<AudioStreamPlaybackGME> playback;
	playback.instantiate();
	playback->stream = Ref<AudioStreamGME>(const_cast<AudioStreamGME *>(this));
	return playback;
}

double AudioStreamGME::_get_length() const {
	return get_track_length(0);
}

bool AudioStreamGME::_has_loop() const {
	return true;
}

TypedArray<Dictionary> AudioStreamGME::_get_parameter_list() const {
	TypedArray<Dictionary> parameters;
	parameters.push_back(make_parameter("track_index", Variant::INT, PROPERTY_HINT_RANGE, "0,255,1", 0));
	parameters.push_back(make_parameter("looping", Variant::BOOL, PROPERTY_HINT_NONE, "", true));
	return parameters;
}

void AudioStreamGME::set_data(const PackedByteArray &p_data) {
	data = p_data;
	emit_changed();
}

void AudioStreamGME::set_format(Format p_format) {
	format_id = p_format;
	emit_changed();
}

String AudioStreamGME::get_game() const {
	track_info_t info;
	return find_track_info(*this, 0, info) ? sanitize_string(info.game) : String();
}

String AudioStreamGME::get_author() const {
	track_info_t info;
	return find_track_info(*this, 0, info) ? sanitize_string(info.author) : String();
}

String AudioStreamGME::get_copyright() const {
	track_info_t info;
	return find_track_info(*this, 0, info) ? sanitize_string(info.copyright) : String();
}

int AudioStreamGME::get_track_count() const {
	track_info_t info;
	return find_track_info(*this, 0, info) ? static_cast<int>(info.track_count) : 0;
}

String AudioStreamGME::get_track_title(int p_track_index) const {
	track_info_t info;
	return find_track_info(*this, p_track_index, info) ? sanitize_string(info.song) : String();
}

PackedStringArray AudioStreamGME::get_track_titles() const {
	PackedStringArray titles;
	const std::unique_ptr<Music_Emu> info_emu = create_emu(info_sample_rate);
	if (!info_emu) {
		return titles;
	}

	const int track_total = info_emu->track_count();
	for (int i = 0; i < track_total; ++i) {
		track_info_t info;
		titles.push_back(!info_emu->track_info(&info, i) ? sanitize_string(info.song) : String());
	}
	return titles;
}

double AudioStreamGME::get_track_length(int p_track_index) const {
	track_info_t info;
	return find_track_info(*this, p_track_index, info) ? calc_play_length_ms(info) / 1000.0 : 0.0;
}

void AudioStreamGME::_bind_methods() {
	BIND_ENUM_CONSTANT(FORMAT_NSF);
	BIND_ENUM_CONSTANT(FORMAT_NSFE);
	BIND_ENUM_CONSTANT(FORMAT_GBS);
	BIND_ENUM_CONSTANT(FORMAT_SPC);
	BIND_ENUM_CONSTANT(FORMAT_VGM);
	BIND_ENUM_CONSTANT(FORMAT_GYM);
	BIND_ENUM_CONSTANT(FORMAT_HES);
	BIND_ENUM_CONSTANT(FORMAT_KSS);
	BIND_ENUM_CONSTANT(FORMAT_SAP);
	BIND_ENUM_CONSTANT(FORMAT_AY);

	ClassDB::bind_method(D_METHOD("set_format", "format"), &AudioStreamGME::set_format);
	ClassDB::bind_method(D_METHOD("get_format"), &AudioStreamGME::get_format);
	ClassDB::bind_method(D_METHOD("set_data", "data"), &AudioStreamGME::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &AudioStreamGME::get_data);

	ClassDB::bind_method(D_METHOD("get_game"), &AudioStreamGME::get_game);
	ClassDB::bind_method(D_METHOD("get_author"), &AudioStreamGME::get_author);
	ClassDB::bind_method(D_METHOD("get_copyright"), &AudioStreamGME::get_copyright);
	ClassDB::bind_method(D_METHOD("get_track_count"), &AudioStreamGME::get_track_count);
	ClassDB::bind_method(D_METHOD("get_track_title", "track_index"), &AudioStreamGME::get_track_title);
	ClassDB::bind_method(D_METHOD("get_track_titles"), &AudioStreamGME::get_track_titles);
	ClassDB::bind_method(D_METHOD("get_track_length", "track_index"), &AudioStreamGME::get_track_length);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE), "set_data", "get_data");

	ADD_PROPERTY(PropertyInfo(Variant::INT, "format", PROPERTY_HINT_ENUM, "NSF,NSFE,GBS,SPC,VGM,GYM,HES,KSS,SAP,AY", PROPERTY_USAGE_STORAGE), "set_format", "get_format");

	ADD_GROUP("Info", "");
	constexpr PropertyUsageFlags read_only_usage = static_cast<PropertyUsageFlags>(PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "game", PROPERTY_HINT_NONE, "", read_only_usage), "", "get_game");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "author", PROPERTY_HINT_NONE, "", read_only_usage), "", "get_author");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright", PROPERTY_HINT_NONE, "", read_only_usage), "", "get_copyright");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "track_count", PROPERTY_HINT_NONE, "", read_only_usage), "", "get_track_count");
}

// ========================== AudioStreamPlaybackGME ==========================

AudioStreamPlaybackGME::AudioStreamPlaybackGME() {
}

AudioStreamPlaybackGME::~AudioStreamPlaybackGME() {
}

void AudioStreamPlaybackGME::_start(double p_from_pos) {
	emu.reset();
	is_active = false;

	if (stream.is_valid()) {
		mix_rate = AudioServer::get_singleton()->get_mix_rate();
		emu = stream->create_emu(static_cast<long>(mix_rate));
	}

	if (emu) {
		playing_track_index = track_index;
		const char *error = emu->start_track(playing_track_index);
		if (!error && p_from_pos > 0.0) {
			error = emu->seek(static_cast<long>(p_from_pos * 1000.0));
		}
		if (error) {
			UtilityFunctions::push_error("AudioStreamGME: failed to start track ", playing_track_index, ": ", String(error));
			emu.reset();
		} else if (const char *warning = emu->warning()) {
			UtilityFunctions::push_warning("AudioStreamGME: ", String(warning));
		}
	} else if (stream.is_valid()) {
		UtilityFunctions::push_error("AudioStreamGME: could not create emulator, playback aborted");
	}

	is_active = emu != nullptr;
	begin_resample();
}

void AudioStreamPlaybackGME::_stop() {
	emu.reset();
	is_active = false;
}

bool AudioStreamPlaybackGME::_is_playing() const {
	return is_active;
}

double AudioStreamPlaybackGME::_get_playback_position() const {
	return emu ? emu->tell() / 1000.0 : 0.0;
}

void AudioStreamPlaybackGME::_seek(double p_position) {
	if (emu) {
		emu->seek(static_cast<long>(p_position * 1000.0));
	}
}

int32_t AudioStreamPlaybackGME::_mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) {
	if (!is_active || !emu) {
		std::memset(p_dst_buffer, 0, sizeof(AudioFrame) * static_cast<size_t>(p_frame_count));
		return p_frame_count;
	}

	const int requested_track_index = track_index;
	const bool is_track_changed = requested_track_index != playing_track_index;
	if (is_track_changed || (emu->track_ended() && is_loop_on)) {
		playing_track_index = requested_track_index;
		if (emu->start_track(playing_track_index)) {
			std::memset(p_dst_buffer, 0, sizeof(AudioFrame) * static_cast<size_t>(p_frame_count));
			is_active = false;
			return p_frame_count;
		}
	}

	if (emu->track_ended()) {
		std::memset(p_dst_buffer, 0, sizeof(AudioFrame) * static_cast<size_t>(p_frame_count));
		is_active = false;
		return p_frame_count;
	}

	std::vector<Music_Emu::sample_t> samples(static_cast<size_t>(p_frame_count) * 2);
	if (emu->play(static_cast<long>(samples.size()), samples.data())) {
		std::memset(p_dst_buffer, 0, sizeof(AudioFrame) * static_cast<size_t>(p_frame_count));
		is_active = false;
		return p_frame_count;
	}

	for (int32_t i = 0; i < p_frame_count; ++i) {
		p_dst_buffer[i].left = samples[static_cast<size_t>(i) * 2] * sample_to_float;
		p_dst_buffer[i].right = samples[static_cast<size_t>(i) * 2 + 1] * sample_to_float;
	}

	return p_frame_count;
}

float AudioStreamPlaybackGME::_get_stream_sampling_rate() const {
	return mix_rate;
}

void AudioStreamPlaybackGME::_set_parameter(const StringName &p_name, const Variant &p_value) {
	if (p_name == StringName("track_index")) {
		set_track_index(p_value);
	} else if (p_name == StringName("looping")) {
		set_looping(p_value);
	}
}

Variant AudioStreamPlaybackGME::_get_parameter(const StringName &p_name) const {
	if (p_name == StringName("track_index")) {
		return get_track_index();
	}
	if (p_name == StringName("looping")) {
		return is_looping();
	}
	return Variant();
}

void AudioStreamPlaybackGME::set_track_index(int p_track_index) {
	track_index = p_track_index;
}

void AudioStreamPlaybackGME::set_looping(bool p_is_looping) {
	is_loop_on = p_is_looping;
}

void AudioStreamPlaybackGME::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_track_index", "track_index"), &AudioStreamPlaybackGME::set_track_index);
	ClassDB::bind_method(D_METHOD("get_track_index"), &AudioStreamPlaybackGME::get_track_index);
	ClassDB::bind_method(D_METHOD("set_looping", "is_looping"), &AudioStreamPlaybackGME::set_looping);
	ClassDB::bind_method(D_METHOD("is_looping"), &AudioStreamPlaybackGME::is_looping);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "track_index", PROPERTY_HINT_RANGE, "0,255,1"), "set_track_index", "get_track_index");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "looping"), "set_looping", "is_looping");
}
