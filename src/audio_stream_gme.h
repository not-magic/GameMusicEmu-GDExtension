#pragma once

#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/classes/audio_stream_playback_resampled.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

#include <atomic>
#include <memory>

struct Music_Emu;

namespace godot {

class AudioStreamGME : public AudioStream {
	GDCLASS(AudioStreamGME, AudioStream) // NOLINT

public:
	enum Format {
		FORMAT_NSF,
		FORMAT_NSFE,
		FORMAT_GBS,
		FORMAT_SPC,
		FORMAT_VGM,
		FORMAT_GYM,
		FORMAT_HES,
		FORMAT_KSS,
		FORMAT_SAP,
		FORMAT_AY,
	};

private:
	PackedByteArray data;
	int format_id = FORMAT_NSF;

protected:
	static void _bind_methods();

public:
	String find_unsupported_chips() const;
	std::unique_ptr<Music_Emu> create_emu(long p_sample_rate) const;

	virtual Ref<AudioStreamPlayback> _instantiate_playback() const override;
	virtual String _get_stream_name() const override;
	virtual double _get_length() const override;
	virtual bool _has_loop() const override;
	virtual TypedArray<Dictionary> _get_parameter_list() const override;

	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const { return data; }

	void set_format(Format p_format);
	Format get_format() const { return static_cast<Format>(format_id); }

	String get_game() const;
	String get_author() const;
	String get_copyright() const;
	int get_track_count() const;
	String get_track_title(int p_track_index) const;
	PackedStringArray get_track_titles() const;
	double get_track_length(int p_track_index) const;
};

class AudioStreamPlaybackGME : public AudioStreamPlaybackResampled {
	GDCLASS(AudioStreamPlaybackGME, AudioStreamPlaybackResampled) // NOLINT

	friend class AudioStreamGME;

	Ref<AudioStreamGME> stream;
	std::unique_ptr<Music_Emu> emu;
	float mix_rate = 44100.0f;
	std::atomic<int> track_index{ 0 };
	int playing_track_index = 0;
	std::atomic<bool> is_loop_on{ true };
	bool is_active = false;

protected:
	static void _bind_methods();

public:
	AudioStreamPlaybackGME();
	~AudioStreamPlaybackGME();

	virtual void _start(double p_from_pos) override;
	virtual void _stop() override;
	virtual bool _is_playing() const override;
	virtual double _get_playback_position() const override;
	virtual void _seek(double p_position) override;
	virtual int32_t _mix_resampled(AudioFrame *p_dst_buffer, int32_t p_frame_count) override;
	virtual float _get_stream_sampling_rate() const override;
	virtual void _set_parameter(const StringName &p_name, const Variant &p_value) override;
	virtual Variant _get_parameter(const StringName &p_name) const override;

	void set_track_index(int p_track_index);
	int get_track_index() const { return track_index; }

	void set_looping(bool p_is_looping);
	bool is_looping() const { return is_loop_on; }
};

} // namespace godot

VARIANT_ENUM_CAST(AudioStreamGME::Format);
