//========================================================================
// libansnd
// Another Sound Library for Wii and GameCube homebrew.
//------------------------------------------------------------------------
// Copyright (c) 2025-2026 James Sawyer
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ogcsys.h>
#include <gccore.h>
#include <ogc/lwp_watchdog.h>
#include <ogc/machine/processor.h>

#include "ansndlib.h"
#include "dspmixer.h"

//

#define MAX_PARAMETER_BLOCKS        ANSND_MAX_VOICES
#define PARAMETER_BLOCK_STRUCT_SIZE 128
#define DSP_DRAM_SIZE               8192
#define ANSND_SOUND_BUFFER_SIZE     960 // output 5ms stereo 16-bit sound data at 48kHz

// Values for the DSP Accelerator

#define DSP_ACCL_FMT_ADPCM          0x0000
#define DSP_ACCL_FMT_UNSIGNED_8BIT  0x0005
#define DSP_ACCL_FMT_SIGNED_8BIT    0x0019
#define DSP_ACCL_FMT_UNSIGNED_16BIT 0x0006
#define DSP_ACCL_FMT_SIGNED_16BIT   0x000A

#define DSP_ACCL_GAIN_ADPCM         0x0000
#define DSP_ACCL_GAIN_8BIT          0x0100
#define DSP_ACCL_GAIN_16BIT         0x0800

// DSP mailbox commands

#define DSP_MAIL_COMMAND            0xFACE0000 // command specifier
#define DSP_MAIL_END                0x0000DEAD // dsp terminate task
#define DSP_MAIL_NEXT               0x00001111 // dsp process next set of data
#define DSP_MAIL_PREPARE            0x00002222 // dsp prepare for next cycle
#define DSP_MAIL_MM_LOCATION        0x00003333 // cpu send main memory base locations
#define DSP_MAIL_RESTART            0x00004444 // dsp restart processing cycle

// Voice flags

#define VOICE_FLAG_PITCH_CHANGE     0x2000
#define VOICE_FLAG_CONFIGURED       0x1000
#define VOICE_FLAG_USED             0x0800
#define VOICE_FLAG_UPDATED          0x0400
#define VOICE_FLAG_INITIALIZED      0x0200
#define VOICE_FLAG_ERASED           0x0100
#define VOICE_FLAG_RUNNING          0x0080
#define VOICE_FLAG_FINISHED         0x0040
#define VOICE_FLAG_PAUSED           0x0020
#define VOICE_FLAG_DELAY            0x0010
#define VOICE_FLAG_STREAMING        0x0008
#define VOICE_FLAG_LOOPED           0x0004
#define VOICE_FLAG_ADPCM            0x0002
#define VOICE_FLAG_STEREO           0x0001

// Conversion helpers

#define HIGH(x)                     ((u16)(((x) & 0xFFFF0000) >> 16))
#define LOW(x)                      ((u16)((x) & 0x0000FFFF))
#define SAMPLES_TO_NIBBLES(x)       ((((x) / 14) * 16) + ((x) % 14) + 2)

//

typedef struct ansnd_parameter_block_t {
	s16 sample_buffer[16];                    // 0x00
	
	union {
		struct {
			s16 sample_buffer_2[16];          // 0x10
		} pcm;
		struct {
			u16 decode_coefficients[16];      // 0x10
		} adpcm;
	};
	
	s16 right_volume;                         // 0x20
	s16 left_volume;                          // 0x21
	
	u16 relative_frequency_high;              // 0x22
	u16 relative_frequency_low;               // 0x23
	u16 count_low;                            // 0x24
	
	s16 delay;                                // 0x25
	
	u16 flags;                                // 0x26
	
	u16 sample_buffer_index;                  // 0x27
	u16 sample_buffer_wrapping;               // 0x28
	
	u16 filter_step;                          // 0x29
	u16 filter_step_512;                      // 0x2A
	
	s16 correction_factor;                    // 0x2B
	
	u16 accelerator_format;                   // 0x2C
	
	u16 accelerator_start_high;               // 0x2D
	u16 accelerator_start_low;                // 0x2E
	u16 accelerator_end_high;                 // 0x2F
	u16 accelerator_end_low;                  // 0x30
	u16 accelerator_current_high;             // 0x31
	u16 accelerator_current_low;              // 0x32
	
	u16 initial_predictor_scale;              // 0x33
	s16 initial_sample_history_1;             // 0x34
	s16 initial_sample_history_2;             // 0x35
	
	u16 accelerator_gain;                     // 0x36
	
	union {
		struct {
			u16 padding[4];                   // 0x37
			
			u16 loop_start_high;              // 0x3B
			u16 loop_start_low;               // 0x3C
			
			u16 loop_predictor_scale;         // 0x3D
			s16 loop_sample_history_1;        // 0x3E
			s16 loop_sample_history_2;        // 0x3F
		} looping;
		struct {
			u16 next_buffer_start_high;       // 0x37
			u16 next_buffer_start_low;        // 0x38
			u16 next_buffer_end_high;         // 0x39
			u16 next_buffer_end_low;          // 0x3A
			u16 next_buffer_current_high;     // 0x3B
			u16 next_buffer_current_low;      // 0x3C
			
			u16 next_buffer_predictor_scale;  // 0x3D
			s16 next_buffer_sample_history_1; // 0x3E
			s16 next_buffer_sample_history_2; // 0x3F
		} streaming;
	};
} ansnd_parameter_block_t;

_Static_assert(sizeof(ansnd_parameter_block_t) == PARAMETER_BLOCK_STRUCT_SIZE, "Struct does not match expected size.");

typedef union {
	ansnd_pcm_stream_data_callback_t   pcm_callback;
	ansnd_adpcm_stream_data_callback_t adpcm_callback;
} ansnd_stream_data_callback_t;

typedef struct ansnd_voice_t {
	u32 samplerate;
	f32 pitch;
	
	u32 delay;
	
	u16 flags;
	
	f32 left_volume;
	f32 right_volume;
	
	u16 decode_coefficients[16];
	
	u16 accelerator_format;
	u16 accelerator_gain;
	
	u32 ram_buffer_start;
	u32 ram_buffer_end;
	u32 ram_buffer_first;
	
	u16 initial_predictor_scale;
	u16 initial_sample_history_1;
	u16 initial_sample_history_2;
	
	union {
		struct {
			u32 loop_start;
			u32 loop_end;
			
			u16 loop_predictor_scale;
			u16 loop_sample_history_1;
			u16 loop_sample_history_2;
		} looping;
		struct {
			u32 next_buffer_start;
			u32 next_buffer_end;
			u32 next_buffer_first;
			
			u16 next_buffer_predictor_scale;
			u16 next_buffer_sample_history_1;
			u16 next_buffer_sample_history_2;
		} streaming;
	};
	
	ansnd_parameter_block_t* parameter_block;
	
	struct ansnd_voice_t*    linked_voice;
	
	ansnd_voice_callback_t   voice_callback;
	
	ansnd_stream_data_callback_t stream_callback;
	
	void* user_pointer;
} ansnd_voice_t;

static dsptask_t              ansnd_dsp_task;
static u8                     ansnd_dsp_dram_image[DSP_DRAM_SIZE] ATTRIBUTE_ALIGN(32);

static ansnd_audio_callback_t ansnd_audio_callback           = NULL;
static void*                  ansnd_audio_callback_arguments = NULL;

static u8 ansnd_next_audio_buffer     = 0;
static u8 ansnd_audio_buffer_out[2][ANSND_SOUND_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);
static u8 ansnd_mute_buffer_out[ANSND_SOUND_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);

static u8 ansnd_output_samplerate     = ANSND_OUTPUT_SAMPLERATE_48KHZ;

static bool ansnd_library_initialized = false;

static bool ansnd_dsp_done_mixing     = false;
static bool ansnd_dsp_stalled         = false;
static bool ansnd_dsp_yielding        = false;

static u32 ansnd_active_voices        = 0;
static u64 ansnd_dsp_start_time       = 0;
static u64 ansnd_dsp_process_time     = 0;
static u64 ansnd_total_start_time     = 0;
static u64 ansnd_total_process_time   = 0;

static ansnd_voice_t ansnd_voices[ANSND_MAX_VOICES];

// forward declarations for ansnd_load_dsp_task()
static void ansnd_dsp_initialized_callback(dsptask_t* task);
static void ansnd_dsp_resume_callback(dsptask_t* task);
static void ansnd_dsp_request_callback(dsptask_t* task);

static void ansnd_load_dsp_task() {
	ansnd_dsp_task.prio       = 0;
	ansnd_dsp_task.iram_maddr = (void*)MEM_VIRTUAL_TO_PHYSICAL(dspmixer);
	ansnd_dsp_task.iram_len   = dspmixer_size;
	ansnd_dsp_task.iram_addr  = 0x0000;
	ansnd_dsp_task.dram_maddr = (void*)MEM_VIRTUAL_TO_PHYSICAL(ansnd_dsp_dram_image);
	ansnd_dsp_task.dram_len   = DSP_DRAM_SIZE;
	ansnd_dsp_task.dram_addr  = 0x0000;
	ansnd_dsp_task.init_vec   = 0x0010;
	ansnd_dsp_task.resume_vec = 0x0015;
	ansnd_dsp_task.init_cb    = ansnd_dsp_initialized_callback;
	ansnd_dsp_task.res_cb     = ansnd_dsp_resume_callback;
	ansnd_dsp_task.req_cb     = ansnd_dsp_request_callback;
	DSP_AddTask(&ansnd_dsp_task);
}

static void ansnd_erase_voice(ansnd_voice_t* voice) {
	memset(voice->parameter_block, 0, PARAMETER_BLOCK_STRUCT_SIZE);
	memset(voice, 0, sizeof(ansnd_voice_t));
}

static void ansnd_update_voice_pitch(ansnd_voice_t* voice) {
	ansnd_parameter_block_t* const parameter_block = voice->parameter_block;
	
	f32 dsp_frequency = 1.f;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		dsp_frequency = ANSND_DSP_FREQ_32KHZ;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		dsp_frequency = ANSND_DSP_FREQ_48KHZ;
		break;
	default:
		break;
	}
	
	const u32 base_frequency = 0x00010000;
	f32 adjusted_samplerate  = voice->samplerate * voice->pitch;
	u32 relative_frequency   = lrintf((f32)base_frequency * (adjusted_samplerate / dsp_frequency));
	
	// funnel down samplerates that are very close to base already to avoid resampling
	if ((relative_frequency > (base_frequency - 0x100)) &&
		(relative_frequency < (base_frequency + 0x100))) {
		relative_frequency = base_frequency;
	}
	
	parameter_block->relative_frequency_high = HIGH(relative_frequency);
	parameter_block->relative_frequency_low  = LOW(relative_frequency);
	
	u16 filter_step = 0x7FFF;
	if (relative_frequency > base_frequency) {
		filter_step = lrintf((f32)base_frequency * (dsp_frequency / adjusted_samplerate) * 0.5f);
	}
	parameter_block->filter_step       = filter_step;
	parameter_block->correction_factor = filter_step & ~0xFF;
	
	parameter_block->filter_step_512 = (filter_step >> 6) & 0x01FC;
	
	// update the sample buffer(s)
	u16 new_sample_buffer_size     = lrintf(131071.f / filter_step);
	u16 new_sample_buffer_wrapping = new_sample_buffer_size - 1;
	u16 new_sample_buffer_index    = 16 - new_sample_buffer_size;
	
	u16 old_sample_buffer_size     = parameter_block->sample_buffer_wrapping + 1;
	u16 old_sample_buffer_wrapping = parameter_block->sample_buffer_wrapping;
	u16 old_sample_buffer_index    = parameter_block->sample_buffer_index;
	
	// if size is the same as before, return
	if (new_sample_buffer_size == old_sample_buffer_size) {
		return;
	}
	
	// update the position of the samples
	parameter_block->sample_buffer_wrapping = new_sample_buffer_wrapping;
	parameter_block->sample_buffer_index    = new_sample_buffer_index;
	
	u32 total_copies = 0;
	s16 sample_buffer_1[16] = {0};
	s16 sample_buffer_2[16] = {0};
	
	if (new_sample_buffer_size < old_sample_buffer_size) {
		old_sample_buffer_index += (old_sample_buffer_size - new_sample_buffer_size);
		total_copies = new_sample_buffer_size;
	} else {
		new_sample_buffer_index += (new_sample_buffer_size - old_sample_buffer_size);
		total_copies = old_sample_buffer_size;
	}
	
	for (u32 i = 0; i < total_copies; ++i) {
		if (old_sample_buffer_index > old_sample_buffer_wrapping) {
			old_sample_buffer_index = 16 - old_sample_buffer_size;
		}
		
		sample_buffer_1[new_sample_buffer_index] = parameter_block->sample_buffer[old_sample_buffer_index];
		sample_buffer_2[new_sample_buffer_index] = parameter_block->pcm.sample_buffer_2[old_sample_buffer_index];
		
		old_sample_buffer_index += 1;
		new_sample_buffer_index += 1;
	}
	
	memcpy(parameter_block->sample_buffer, sample_buffer_1, 16 * 2);
	if (!(parameter_block->flags & VOICE_FLAG_ADPCM)) {
		memcpy(parameter_block->pcm.sample_buffer_2, sample_buffer_2, 16 * 2);
	}
}

static void ansnd_update_voice_delay(ansnd_voice_t* voice) {
	f32 dsp_frequency = 1.f;
	u32 microseconds_per_cycle = 0;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		dsp_frequency = ANSND_DSP_FREQ_32KHZ;
		microseconds_per_cycle = 7500;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		dsp_frequency = ANSND_DSP_FREQ_48KHZ;
		microseconds_per_cycle = 5000;
		break;
	default:
		break;
	}
	
	if (voice->delay < microseconds_per_cycle) {
		// convert delay to output samples in 1 cycle
		voice->parameter_block->flags &= ~VOICE_FLAG_DELAY;
		voice->parameter_block->delay = lrint((voice->delay * dsp_frequency) / 1000000);
		voice->flags                  &= ~VOICE_FLAG_DELAY;
		voice->delay                  = 0;
	} else {
		voice->delay                  -= microseconds_per_cycle;
	}
}

static void ansnd_initialize_voice(ansnd_voice_t* voice) {
	ansnd_parameter_block_t* const parameter_block = voice->parameter_block;
	memset(parameter_block, 0, PARAMETER_BLOCK_STRUCT_SIZE);
	
	ansnd_update_voice_pitch(voice);
	
	if (voice->delay != 0) {
		parameter_block->flags |= VOICE_FLAG_DELAY;
		voice->flags           |= VOICE_FLAG_DELAY;
		ansnd_update_voice_delay(voice);
	}
	
	parameter_block->left_volume  = lrintf(0x7FFF * voice->left_volume);
	parameter_block->right_volume = lrintf(0x7FFF * voice->right_volume);
	
	u16 mask = 
		VOICE_FLAG_USED      | 
		VOICE_FLAG_RUNNING   | 
		VOICE_FLAG_FINISHED  | 
		VOICE_FLAG_PAUSED    | 
		VOICE_FLAG_STREAMING | 
		VOICE_FLAG_LOOPED    | 
		VOICE_FLAG_ADPCM     | 
		VOICE_FLAG_STEREO;
	
	parameter_block->flags |= voice->flags & mask;
	
	parameter_block->accelerator_format = voice->accelerator_format;
	parameter_block->accelerator_gain   = voice->accelerator_gain;
	
	parameter_block->accelerator_start_high = HIGH(voice->ram_buffer_start);
	parameter_block->accelerator_start_low  = LOW(voice->ram_buffer_start);
	
	parameter_block->accelerator_end_high = HIGH(voice->ram_buffer_end);
	parameter_block->accelerator_end_low  = LOW(voice->ram_buffer_end);
	
	parameter_block->accelerator_current_high = HIGH(voice->ram_buffer_first);
	parameter_block->accelerator_current_low  = LOW(voice->ram_buffer_first);
	
	parameter_block->initial_predictor_scale  = voice->initial_predictor_scale;
	parameter_block->initial_sample_history_1 = voice->initial_sample_history_1;
	parameter_block->initial_sample_history_2 = voice->initial_sample_history_2;
	
	if (voice->flags & VOICE_FLAG_LOOPED) {
		parameter_block->flags                   &= ~VOICE_FLAG_STREAMING;
		parameter_block->looping.loop_start_high = HIGH(voice->looping.loop_start);
		parameter_block->looping.loop_start_low  = LOW(voice->looping.loop_start);
		parameter_block->accelerator_end_high    = HIGH(voice->looping.loop_end);
		parameter_block->accelerator_end_low     = LOW(voice->looping.loop_end);
		
		parameter_block->looping.loop_predictor_scale  = voice->looping.loop_predictor_scale;
		parameter_block->looping.loop_sample_history_1 = voice->looping.loop_sample_history_1;
		parameter_block->looping.loop_sample_history_2 = voice->looping.loop_sample_history_2;
	}
	
	if (voice->flags & VOICE_FLAG_ADPCM) {
		for (u32 i = 0; i < 16; ++i) {
			parameter_block->adpcm.decode_coefficients[i] = voice->decode_coefficients[i];
		}
	}
	
	voice->flags |= VOICE_FLAG_INITIALIZED;
}

static void ansnd_sync_voice(ansnd_voice_t* voice) {
	if (voice->flags & VOICE_FLAG_ERASED) {
		if (voice->voice_callback) {
			voice->voice_callback(voice->user_pointer, ANSND_VOICE_STATE_ERASED);
		}
		ansnd_erase_voice(voice);
		return;
	}
	s32 voice_state = ANSND_VOICE_STATE_ERROR;
	
	// voice states in descending order of priority
	if (voice->flags & VOICE_FLAG_RUNNING) {
		if (voice->flags & VOICE_FLAG_PAUSED) {
			voice_state = ANSND_VOICE_STATE_PAUSED;
		} else {
			voice_state = ANSND_VOICE_STATE_RUNNING;
		}
	} else {
		voice_state = ANSND_VOICE_STATE_STOPPED;
	}
	
	ansnd_parameter_block_t* const parameter_block = voice->parameter_block;
	
	if (!(voice->flags & VOICE_FLAG_INITIALIZED)) {
		ansnd_initialize_voice(voice);
	}
	
	if (voice->flags & VOICE_FLAG_PITCH_CHANGE) {
		ansnd_update_voice_pitch(voice);
		voice->flags &= ~VOICE_FLAG_PITCH_CHANGE;
	}
	
	if (parameter_block->flags & VOICE_FLAG_FINISHED) {
		parameter_block->flags &= ~VOICE_FLAG_FINISHED;
		voice->flags           &= ~VOICE_FLAG_RUNNING;
		voice_state            = ANSND_VOICE_STATE_FINISHED;
	}
	
	u16 flags_diff = voice->flags ^ parameter_block->flags;
	if (flags_diff & VOICE_FLAG_RUNNING) {
		parameter_block->flags &= ~VOICE_FLAG_RUNNING;
	}
	if (flags_diff & VOICE_FLAG_PAUSED) {
		parameter_block->flags &= ~VOICE_FLAG_PAUSED;
		parameter_block->flags |= (voice->flags & VOICE_FLAG_PAUSED);
	}
	if (flags_diff & VOICE_FLAG_LOOPED) {
		parameter_block->flags &= ~VOICE_FLAG_LOOPED;
		parameter_block->accelerator_end_high = HIGH(voice->ram_buffer_end);
		parameter_block->accelerator_end_low  = LOW(voice->ram_buffer_end);
		
		if (voice->flags & VOICE_FLAG_STREAMING) {
			parameter_block->flags |= VOICE_FLAG_STREAMING;
			voice->streaming.next_buffer_start = 0;
		}
	}
	
	parameter_block->left_volume  = lrintf(0x7FFF * voice->left_volume);
	parameter_block->right_volume = lrintf(0x7FFF * voice->right_volume);
	
	voice->flags &= ~VOICE_FLAG_UPDATED;
	
	if (voice->voice_callback) {
		voice->voice_callback(voice->user_pointer, voice_state);
	}
}

static void ansnd_write_stream_buffers(ansnd_voice_t* voice) {
	ansnd_parameter_block_t* const parameter_block = voice->parameter_block;
	
	parameter_block->streaming.next_buffer_start_high   = HIGH(voice->streaming.next_buffer_start);
	parameter_block->streaming.next_buffer_start_low    = LOW(voice->streaming.next_buffer_start);
	parameter_block->streaming.next_buffer_end_high     = HIGH(voice->streaming.next_buffer_end);
	parameter_block->streaming.next_buffer_end_low      = LOW(voice->streaming.next_buffer_end);
	parameter_block->streaming.next_buffer_current_high = HIGH(voice->streaming.next_buffer_first);
	parameter_block->streaming.next_buffer_current_low  = LOW(voice->streaming.next_buffer_first);
	
	if (voice->flags & VOICE_FLAG_ADPCM) {
		parameter_block->streaming.next_buffer_predictor_scale  = voice->streaming.next_buffer_predictor_scale;
		parameter_block->streaming.next_buffer_sample_history_1 = voice->streaming.next_buffer_sample_history_1;
		parameter_block->streaming.next_buffer_sample_history_2 = voice->streaming.next_buffer_sample_history_2;
	}
	
	voice->streaming.next_buffer_start = 0;
	voice->streaming.next_buffer_end   = 0;
	voice->streaming.next_buffer_first = 0;
}

static void ansnd_fill_stream_buffers(ansnd_voice_t* voice) {
	if (voice->flags & VOICE_FLAG_ADPCM) {
		ansnd_adpcm_data_buffer_t data_buffer;
		memset(&data_buffer, 0, sizeof(ansnd_adpcm_data_buffer_t));
		
		voice->stream_callback.adpcm_callback(voice->user_pointer, &data_buffer);
		
		if ((data_buffer.data_ptr == 0) ||
			(data_buffer.sample_count == 0)) {
			return;
		}
	#if defined(HW_DOL)
		if ((data_buffer.data_ptr < AR_GetBaseAddress()) ||
			(data_buffer.data_ptr >= AR_GetSize())) {
			return;
		}
	#elif defined(HW_RVL)
		if (data_buffer.data_ptr & SYS_BASE_CACHED) {
			return;
		}
	#endif
		
		voice->streaming.next_buffer_start = data_buffer.data_ptr << 1;
		voice->streaming.next_buffer_end   = voice->streaming.next_buffer_start + SAMPLES_TO_NIBBLES(data_buffer.sample_count);
		voice->streaming.next_buffer_first = voice->streaming.next_buffer_start + SAMPLES_TO_NIBBLES(0);
		
		voice->streaming.next_buffer_predictor_scale  = data_buffer.predictor_scale;
		voice->streaming.next_buffer_sample_history_1 = data_buffer.sample_history_1;
		voice->streaming.next_buffer_sample_history_2 = data_buffer.sample_history_2;
	} else {
		ansnd_pcm_data_buffer_t data_buffer;
		memset(&data_buffer, 0, sizeof(ansnd_pcm_data_buffer_t));
		
		voice->stream_callback.pcm_callback(voice->user_pointer, &data_buffer);
		
		if ((data_buffer.frame_data_ptr == 0) ||
			(data_buffer.frame_count == 0)) {
			return;
		}
	#if defined(HW_DOL)
		if ((data_buffer.frame_data_ptr < AR_GetBaseAddress()) ||
			(data_buffer.frame_data_ptr >= AR_GetSize())) {
			return;
		}
	#elif defined(HW_RVL)
		if (data_buffer.frame_data_ptr & SYS_BASE_CACHED) {
			return;
		}
	#endif
		
		u32 channels   = 1;
		if (voice->flags & VOICE_FLAG_STEREO) {
			channels   = 2;
		}
		
		u32 memory_shift = 0;
		if (voice->accelerator_gain == DSP_ACCL_GAIN_16BIT) {
			memory_shift = 1;
		}
		
		voice->streaming.next_buffer_start = data_buffer.frame_data_ptr >> memory_shift;
		// offset the end address so that the DSP accelerator overflow interrupt is generated at the right time
		voice->streaming.next_buffer_end   = voice->streaming.next_buffer_start + data_buffer.frame_count * channels - 1;
		voice->streaming.next_buffer_first = voice->streaming.next_buffer_start;
	}
}

static void ansnd_update_stream_buffers(ansnd_voice_t* voice) {
	if (voice->streaming.next_buffer_start == 0) {
		ansnd_fill_stream_buffers(voice);
	}
	
	if ((voice->parameter_block->streaming.next_buffer_start_high == 0)	&&
		(voice->parameter_block->streaming.next_buffer_start_low == 0)) {
		ansnd_write_stream_buffers(voice);
	}
}

static void ansnd_dsp_initialized_callback(dsptask_t* task) {
	// send main memory locations
	DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_MM_LOCATION);
	while(DSP_CheckMailTo());
	
	ansnd_parameter_block_t* parameter_block_base = (ansnd_parameter_block_t*)ansnd_dsp_dram_image;
	
	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL(parameter_block_base));
	while(DSP_CheckMailTo());
	
	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL(ansnd_audio_buffer_out[0]));
	while(DSP_CheckMailTo());
	
	DSP_SendMailTo(MEM_VIRTUAL_TO_PHYSICAL(ansnd_audio_buffer_out[1]));
	while(DSP_CheckMailTo());
	
	DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_RESTART);
	while(DSP_CheckMailTo());
}

static void ansnd_dsp_resume_callback(dsptask_t* task) {
	ansnd_dsp_yielding = false;
	
	if (!ansnd_dsp_done_mixing) {
		DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_NEXT);
		while(DSP_CheckMailTo());
	}
}

static void ansnd_dsp_request_callback(dsptask_t* task) {
	ansnd_dsp_done_mixing = true;
	ansnd_dsp_stalled = false;
	
	ansnd_dsp_process_time = (gettime() - ansnd_dsp_start_time);
	
	ansnd_parameter_block_t* parameter_block_base = (ansnd_parameter_block_t*)ansnd_dsp_dram_image;
	DCInvalidateRange(parameter_block_base, PARAMETER_BLOCK_STRUCT_SIZE * MAX_PARAMETER_BLOCKS);
	
	ansnd_active_voices = 0;
	
	for (u32 i = 0; i < ANSND_MAX_VOICES; ++i) {
		ansnd_voice_t* voice = &ansnd_voices[i];
		
		if ((voice->flags & VOICE_FLAG_UPDATED) ||
			((voice->parameter_block) && (voice->parameter_block->flags & VOICE_FLAG_FINISHED))) {
			ansnd_sync_voice(voice);
		}
		
		if (voice->flags & VOICE_FLAG_RUNNING) {
			ansnd_active_voices++;
		} else {
			continue;
		}
		
		if ((voice->flags & VOICE_FLAG_STREAMING) &&
			!(voice->flags & VOICE_FLAG_LOOPED)) {
			ansnd_update_stream_buffers(voice);
		}
		
		if (voice->flags & VOICE_FLAG_DELAY) {
			ansnd_update_voice_delay(voice);
		}
	}
	
	DCFlushRange(parameter_block_base, PARAMETER_BLOCK_STRUCT_SIZE * MAX_PARAMETER_BLOCKS);
	
	DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_PREPARE);
	while(DSP_CheckMailTo());
	
	ansnd_dsp_yielding = true;
	
	if (ansnd_audio_callback) {
		DCInvalidateRange(ansnd_audio_buffer_out[ansnd_next_audio_buffer], ANSND_SOUND_BUFFER_SIZE);
		ansnd_audio_callback(ansnd_audio_buffer_out[ansnd_next_audio_buffer], ANSND_SOUND_BUFFER_SIZE, ansnd_audio_callback_arguments);
		DCFlushRange(ansnd_audio_buffer_out[ansnd_next_audio_buffer], ANSND_SOUND_BUFFER_SIZE);
	}
	
	ansnd_total_process_time = (gettime() - ansnd_total_start_time);
}

static void ansnd_audio_dma_callback() {
	if (!ansnd_dsp_done_mixing) {
		AUDIO_InitDMA((u32)ansnd_mute_buffer_out, ANSND_SOUND_BUFFER_SIZE);
		
		if (ansnd_dsp_stalled && !ansnd_dsp_yielding) {
			DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_RESTART);
		} else {
			// if the task is already running, this will prevent us from yielding on the next synchronization point
			DSP_AssertTask(&ansnd_dsp_task);
		}
		ansnd_dsp_stalled = true;
		return;
	}
	
	ansnd_dsp_done_mixing = false;
	
	ansnd_total_start_time = gettime();
	
	if (ansnd_dsp_yielding) {
		DSP_AssertTask(&ansnd_dsp_task);
	} else {
		DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_NEXT);
		while(DSP_CheckMailTo());
	}
	
	ansnd_dsp_start_time = gettime();
	
	AUDIO_InitDMA((u32)ansnd_audio_buffer_out[ansnd_next_audio_buffer], ANSND_SOUND_BUFFER_SIZE);
	
	ansnd_next_audio_buffer ^= 1;
}


// --- External Interface --- //


void ansnd_initialize() {
	ansnd_initialize_samplerate(ANSND_OUTPUT_SAMPLERATE_48KHZ);
}

s32 ansnd_initialize_samplerate(u8 output_samplerate) {
	u8 ai_rate;
	switch (output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		ai_rate = AI_SAMPLERATE_32KHZ;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		ai_rate = AI_SAMPLERATE_48KHZ;
		break;
	default:
		return ANSND_ERROR_INVALID_INPUT;
	}
	ansnd_output_samplerate = output_samplerate;
	
	DSP_Init();
	AUDIO_Init(NULL);
	AUDIO_StopDMA();
	AUDIO_SetDSPSampleRate(ai_rate);
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	if(!ansnd_library_initialized) {
		ansnd_library_initialized = true;
		
		ansnd_dsp_done_mixing = false;
		ansnd_dsp_stalled     = false;
		ansnd_dsp_yielding    = false;
		
		memset(ansnd_voices, 0, sizeof(ansnd_voice_t) * ANSND_MAX_VOICES);
		
		memset(ansnd_audio_buffer_out[0], 0, ANSND_SOUND_BUFFER_SIZE);
		memset(ansnd_audio_buffer_out[1], 0, ANSND_SOUND_BUFFER_SIZE);
		memset(ansnd_mute_buffer_out,     0, ANSND_SOUND_BUFFER_SIZE);
		memset(ansnd_dsp_dram_image,      0, DSP_DRAM_SIZE);
		
		DCFlushRange(ansnd_audio_buffer_out[0], ANSND_SOUND_BUFFER_SIZE);
		DCFlushRange(ansnd_audio_buffer_out[1], ANSND_SOUND_BUFFER_SIZE);
		DCFlushRange(ansnd_mute_buffer_out,     ANSND_SOUND_BUFFER_SIZE);
		DCFlushRange(ansnd_dsp_dram_image,      DSP_DRAM_SIZE);
		
		ansnd_load_dsp_task();
		do {
			_CPU_ISR_Flash(level);
		} while(ansnd_dsp_task.state != DSPTASK_RUN);
	}
	
	AUDIO_RegisterDMACallback(ansnd_audio_dma_callback);
	AUDIO_InitDMA((u32)ansnd_mute_buffer_out, ANSND_SOUND_BUFFER_SIZE);
	AUDIO_StartDMA();
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

void ansnd_uninitialize() {
	u32 level;
	_CPU_ISR_Disable(level);
	
	if(ansnd_library_initialized) {
		AUDIO_StopDMA();
		AUDIO_RegisterDMACallback(NULL);
		DSP_CancelTask(&ansnd_dsp_task);
		
		DSP_SendMailTo(DSP_MAIL_COMMAND | DSP_MAIL_END);
		while(DSP_CheckMailTo());
		
		do {
			_CPU_ISR_Flash(level);
		} while(ansnd_dsp_task.state != DSPTASK_DONE);
		
		ansnd_library_initialized = false;
	}
	
	_CPU_ISR_Restore(level);
}

s32 ansnd_allocate_voice() {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	s32 voice_id = -1;
	for (u32 i = 0; i < ANSND_MAX_VOICES; ++i) {
		if (!(ansnd_voices[i].flags & VOICE_FLAG_USED)) {
			voice_id = i;
			break;
		}
	}
	
	if (voice_id < 0) {
		_CPU_ISR_Restore(level);
		return ANSND_ERROR_ALL_VOICES_USED;
	}
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	memset(voice, 0, sizeof(ansnd_voice_t));
	
	voice->flags = VOICE_FLAG_USED;
	
	_CPU_ISR_Restore(level);
	
	return voice_id;
}

s32 ansnd_deallocate_voice(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags |= VOICE_FLAG_ERASED;
	
	if (linked_voice) {
		voice->linked_voice = NULL;
		linked_voice->linked_voice = NULL;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_configure_pcm_voice(u32 voice_id, const ansnd_pcm_voice_config_t* voice_config) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (voice_config == NULL) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	f32 max_samplerate = 1.f;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		max_samplerate = ANSND_MAX_SAMPLERATE_32KHZ;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		max_samplerate = ANSND_MAX_SAMPLERATE_48KHZ;
		break;
	default:
		break;
	}
	if (((voice_config->samplerate * voice_config->pitch) < 50) ||
		((voice_config->samplerate * voice_config->pitch) > max_samplerate)) {
		return ANSND_ERROR_INVALID_SAMPLERATE;
	}
	if ((voice_config->frame_data_ptr == 0) ||
		(voice_config->frame_count == 0)) {
		return ANSND_ERROR_INVALID_MEMORY;
	}
#if defined(HW_DOL)
	if ((voice_config->frame_data_ptr < AR_GetBaseAddress()) ||
		(voice_config->frame_data_ptr >= AR_GetSize())) {
		return ANSND_ERROR_INVALID_MEMORY;
	}
#elif defined(HW_RVL)
	if (voice_config->frame_data_ptr & SYS_BASE_CACHED) {
		return ANSND_ERROR_INVALID_MEMORY;
	}
#endif
	if ((voice_config->format == ANSND_VOICE_PCM_FORMAT_UNSET) ||
		(voice_config->format > ANSND_VOICE_PCM_FORMAT_SIGNED_16_PCM)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	if ((voice_config->channels == 0) || (voice_config->channels > 2)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	if ((voice_config->left_volume < -1.f) || (voice_config->left_volume > 1.f)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	if ((voice_config->right_volume < -1.f) || (voice_config->right_volume > 1.f)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	if ((voice_config->loop_start_offset > voice_config->frame_count) ||
		(voice_config->loop_end_offset > voice_config->frame_count)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	memset(voice, 0, sizeof(ansnd_voice_t));
	
	if (linked_voice) {
		voice->linked_voice = linked_voice;
	}
	
	u32 memory_shift = 0;
	switch (voice_config->format) {
	case ANSND_VOICE_PCM_FORMAT_SIGNED_8_PCM:
		voice->accelerator_format = DSP_ACCL_FMT_SIGNED_8BIT;
		voice->accelerator_gain   = DSP_ACCL_GAIN_8BIT;
		break;
	case ANSND_VOICE_PCM_FORMAT_SIGNED_16_PCM:
		voice->accelerator_format = DSP_ACCL_FMT_SIGNED_16BIT;
		voice->accelerator_gain   = DSP_ACCL_GAIN_16BIT;
		memory_shift = 1;
		break;
	}
	
	voice->samplerate = voice_config->samplerate;
	voice->pitch      = voice_config->pitch;
	
	voice->delay = voice_config->delay;
	
	voice->flags |= VOICE_FLAG_USED;
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags |= VOICE_FLAG_CONFIGURED;
	
	if (voice_config->channels == 2) {
		voice->flags |= VOICE_FLAG_STEREO;
	}
	
	voice->left_volume  = voice_config->left_volume;
	voice->right_volume = voice_config->right_volume;
	
	voice->ram_buffer_start = voice_config->frame_data_ptr >> memory_shift;
	voice->ram_buffer_end   = voice->ram_buffer_start + voice_config->frame_count * voice_config->channels - 1;
	voice->ram_buffer_first = voice->ram_buffer_start + voice_config->start_offset * voice_config->channels;
	
	if ((voice_config->loop_start_offset != 0) ||
		(voice_config->loop_end_offset != 0)) {
		voice->flags |= VOICE_FLAG_LOOPED;
		
		voice->looping.loop_start = voice->ram_buffer_start + voice_config->loop_start_offset * voice_config->channels;
		voice->looping.loop_end   = voice->ram_buffer_start + voice_config->loop_end_offset * voice_config->channels - 1;
	}
	
	ansnd_parameter_block_t* parameter_block_base = (ansnd_parameter_block_t*)ansnd_dsp_dram_image;
	voice->parameter_block = &parameter_block_base[voice_id];
	
	voice->voice_callback = voice_config->voice_callback;
	
	if (voice_config->stream_callback) {
		voice->flags |= VOICE_FLAG_STREAMING;
		voice->stream_callback.pcm_callback = voice_config->stream_callback;
	}
	
	voice->user_pointer = voice_config->user_pointer;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_configure_adpcm_voice(u32 voice_id, const ansnd_adpcm_voice_config_t* voice_config) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (voice_config == NULL) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	f32 max_samplerate = 1.f;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		max_samplerate = ANSND_MAX_SAMPLERATE_32KHZ;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		max_samplerate = ANSND_MAX_SAMPLERATE_48KHZ;
		break;
	default:
		break;
	}
	if (((voice_config->samplerate * voice_config->pitch) < 50) ||
		((voice_config->samplerate * voice_config->pitch) > max_samplerate)) {
		return ANSND_ERROR_INVALID_SAMPLERATE;
	}
	if ((voice_config->data_ptr == 0) ||
		(voice_config->sample_count == 0)) {
		return ANSND_ERROR_INVALID_MEMORY;
	}
#if defined(HW_DOL)
	if ((voice_config->data_ptr < AR_GetBaseAddress()) ||
		(voice_config->data_ptr >= AR_GetSize())) {
		return ANSND_ERROR_INVALID_MEMORY;
	}
#elif defined(HW_RVL)
	if (voice_config->data_ptr & SYS_BASE_CACHED) {
		return ANSND_ERROR_INVALID_MEMORY;
	}
#endif
	if ((voice_config->left_volume < -1.f) || (voice_config->left_volume > 1.f)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	if ((voice_config->right_volume < -1.f) || (voice_config->right_volume > 1.f)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	
	u32 start_offset_nibbles      = 0;
	u32 end_offset_nibbles        = SAMPLES_TO_NIBBLES(voice_config->sample_count);
	u32 loop_start_offset_nibbles = 0;
	u32 loop_end_offset_nibbles   = 0;
	if (voice_config->nibble_offsets_flag != 0) {
		start_offset_nibbles      = voice_config->start_offset;
		loop_start_offset_nibbles = voice_config->loop_start_offset;
		loop_end_offset_nibbles   = voice_config->loop_end_offset;
	} else {
		start_offset_nibbles      = SAMPLES_TO_NIBBLES(voice_config->start_offset);
		loop_start_offset_nibbles = SAMPLES_TO_NIBBLES(voice_config->loop_start_offset);
		loop_end_offset_nibbles   = SAMPLES_TO_NIBBLES(voice_config->loop_end_offset);
	}
	
	if ((loop_start_offset_nibbles > end_offset_nibbles) ||
		(loop_end_offset_nibbles > end_offset_nibbles)) {
		return ANSND_ERROR_INVALID_CONFIGURATION;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	memset(voice, 0, sizeof(ansnd_voice_t));
	
	if (linked_voice) {
		voice->linked_voice = linked_voice;
	}
	
	if (voice_config->adpcm_format == DSP_ACCL_FMT_ADPCM) {
		voice->flags |= VOICE_FLAG_ADPCM;
	}
	voice->accelerator_format = voice_config->adpcm_format;
	voice->accelerator_gain   = voice_config->adpcm_gain;
	
	voice->samplerate = voice_config->samplerate;
	voice->pitch      = voice_config->pitch;
	
	voice->delay = voice_config->delay;
	
	voice->flags |= VOICE_FLAG_USED;
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags |= VOICE_FLAG_CONFIGURED;
	
	voice->left_volume  = voice_config->left_volume;
	voice->right_volume = voice_config->right_volume;
	
	for (u32 i = 0; i < 16; ++i) {
		voice->decode_coefficients[i] = voice_config->decode_coefficients[i];
	}
	
	voice->ram_buffer_start = voice_config->data_ptr << 1;
	voice->ram_buffer_end   = voice->ram_buffer_start + end_offset_nibbles;
	voice->ram_buffer_first = voice->ram_buffer_start + start_offset_nibbles;
	
	voice->initial_predictor_scale  = voice_config->initial_predictor_scale;
	voice->initial_sample_history_1 = voice_config->initial_sample_history_1;
	voice->initial_sample_history_2 = voice_config->initial_sample_history_2;
	
	if (voice_config->loop_flag) {
		voice->flags              |= VOICE_FLAG_LOOPED;
		voice->looping.loop_start = voice->ram_buffer_start + loop_start_offset_nibbles;
		voice->looping.loop_end   = voice->ram_buffer_start + loop_end_offset_nibbles;
		
		voice->looping.loop_predictor_scale  = voice_config->loop_predictor_scale;
		voice->looping.loop_sample_history_1 = voice_config->loop_sample_history_1;
		voice->looping.loop_sample_history_2 = voice_config->loop_sample_history_2;
	}
	
	ansnd_parameter_block_t* parameter_block_base = (ansnd_parameter_block_t*)ansnd_dsp_dram_image;
	voice->parameter_block = &parameter_block_base[voice_id];
	
	voice->voice_callback = voice_config->voice_callback;
	
	if (voice_config->stream_callback) {
		voice->flags |= VOICE_FLAG_STREAMING;
		voice->stream_callback.adpcm_callback = voice_config->stream_callback;
	}
	
	voice->user_pointer = voice_config->user_pointer;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_link_voices(u32 voice_id_1, u32 voice_id_2) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if (voice_id_1 == voice_id_2) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if ((voice_id_1 < 0) ||
		(voice_id_1 >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id_1].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (ansnd_voices[voice_id_1].flags & VOICE_FLAG_RUNNING) {
		return ANSND_ERROR_VOICE_RUNNING;
	}
	if (ansnd_voices[voice_id_1].linked_voice) {
		return ANSND_ERROR_VOICE_ALREADY_LINKED;
	}
	if ((voice_id_2 < 0) ||
		(voice_id_2 >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id_2].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (ansnd_voices[voice_id_2].flags & VOICE_FLAG_RUNNING) {
		return ANSND_ERROR_VOICE_RUNNING;
	}
	if (ansnd_voices[voice_id_2].linked_voice) {
		return ANSND_ERROR_VOICE_ALREADY_LINKED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice_1 = &ansnd_voices[voice_id_1];
	ansnd_voice_t* voice_2 = &ansnd_voices[voice_id_2];
	
	voice_1->linked_voice = voice_2;
	voice_2->linked_voice = voice_1;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_unlink_voice(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	if (!ansnd_voices[voice_id].linked_voice) {
		return ANSND_ERROR_VOICE_NOT_LINKED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice_1 = &ansnd_voices[voice_id];
	ansnd_voice_t* voice_2 = voice_1->linked_voice;
	
	voice_1->linked_voice = NULL;
	voice_2->linked_voice = NULL;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_start_voice(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if (ansnd_dsp_stalled) {
		return ANSND_ERROR_DSP_STALLED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	if ((ansnd_voices[voice_id].flags & VOICE_FLAG_STREAMING) &&
		(ansnd_voices[voice_id].flags & VOICE_FLAG_INITIALIZED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	voice->flags |= VOICE_FLAG_UPDATED | VOICE_FLAG_RUNNING;
	voice->flags &= ~VOICE_FLAG_PAUSED;
	voice->flags &= ~VOICE_FLAG_INITIALIZED;
	
	if (linked_voice) {
		linked_voice->flags |= VOICE_FLAG_UPDATED | VOICE_FLAG_RUNNING;
		linked_voice->flags &= ~VOICE_FLAG_PAUSED;
		linked_voice->flags &= ~VOICE_FLAG_INITIALIZED;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_stop_voice(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags &= ~VOICE_FLAG_RUNNING;
	
	if (linked_voice) {
		linked_voice->flags |= VOICE_FLAG_UPDATED;
		linked_voice->flags &= ~VOICE_FLAG_RUNNING;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_pause_voice(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags |= VOICE_FLAG_PAUSED;
	
	if (linked_voice) {
		linked_voice->flags |= VOICE_FLAG_UPDATED;
		linked_voice->flags |= VOICE_FLAG_PAUSED;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_unpause_voice(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if (ansnd_dsp_stalled) {
		return ANSND_ERROR_DSP_STALLED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags &= ~VOICE_FLAG_PAUSED;
	
	if (linked_voice) {
		linked_voice->flags |= VOICE_FLAG_UPDATED;
		linked_voice->flags &= ~VOICE_FLAG_PAUSED;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_stop_looping(u32 voice_id) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_INITIALIZED)) {
		return ANSND_ERROR_VOICE_NOT_INITIALIZED;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags &= ~VOICE_FLAG_LOOPED;
	
	if (linked_voice) {
		linked_voice->flags |= VOICE_FLAG_UPDATED;
		linked_voice->flags &= ~VOICE_FLAG_LOOPED;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_set_voice_volume(u32 voice_id, f32 left_volume, f32 right_volume) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	if ((left_volume < -1.f) || (left_volume > 1.f)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if ((right_volume < -1.f) || (right_volume > 1.f)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	
	voice->flags |= VOICE_FLAG_UPDATED;
	
	voice->left_volume = left_volume;
	voice->right_volume = right_volume;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_set_voice_pitch(u32 voice_id, f32 pitch) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if ((voice_id < 0) ||
		(voice_id >= ANSND_MAX_VOICES)) {
		return ANSND_ERROR_INVALID_INPUT;
	}
	if (!(ansnd_voices[voice_id].flags & VOICE_FLAG_USED)) {
		return ANSND_ERROR_VOICE_ID_NOT_ALLOCATED;
	}
	
	ansnd_voice_t* voice = &ansnd_voices[voice_id];
	ansnd_voice_t* linked_voice = voice->linked_voice;
	
	if (!(voice->flags & VOICE_FLAG_CONFIGURED)) {
		return ANSND_ERROR_VOICE_NOT_CONFIGURED;
	}
	f32 max_samplerate = 1.f;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		max_samplerate = ANSND_MAX_SAMPLERATE_32KHZ;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		max_samplerate = ANSND_MAX_SAMPLERATE_48KHZ;
		break;
	default:
		break;
	}
	if (((voice->samplerate * pitch) < 50) ||
		((voice->samplerate * pitch) > max_samplerate)) {
		return ANSND_ERROR_INVALID_SAMPLERATE;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	voice->flags |= VOICE_FLAG_UPDATED;
	voice->flags |= VOICE_FLAG_PITCH_CHANGE;
	
	voice->pitch = pitch;
	
	if (linked_voice) {
		linked_voice->flags |= VOICE_FLAG_UPDATED;
		linked_voice->flags |= VOICE_FLAG_PITCH_CHANGE;
		
		linked_voice->pitch = pitch;
	}
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_get_dsp_usage_percent(f32* dsp_usage) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if (ansnd_dsp_stalled) {
		return ANSND_ERROR_DSP_STALLED;
	}
	if (!dsp_usage) {
		return ANSND_ERROR_OK;
	}
	
	f32 microseconds_per_cycle = 1.f;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		microseconds_per_cycle = 7500.f;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		microseconds_per_cycle = 5000.f;
		break;
	default:
		break;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	*dsp_usage = ticks_to_microsecs(ansnd_dsp_process_time) / microseconds_per_cycle;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_get_total_usage_percent(f32* total_usage) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if (ansnd_dsp_stalled) {
		return ANSND_ERROR_DSP_STALLED;
	}
	if (!total_usage) {
		return ANSND_ERROR_OK;
	}
	
	f32 microseconds_per_cycle = 1.f;
	switch (ansnd_output_samplerate) {
	case ANSND_OUTPUT_SAMPLERATE_32KHZ:
		microseconds_per_cycle = 7500.f;
		break;
	case ANSND_OUTPUT_SAMPLERATE_48KHZ:
		microseconds_per_cycle = 5000.f;
		break;
	default:
		break;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	*total_usage = ticks_to_microsecs(ansnd_total_process_time) / microseconds_per_cycle;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_get_total_active_voices(u32* active_voices) {
	if (!ansnd_library_initialized) {
		return ANSND_ERROR_NOT_INITIALIZED;
	}
	if (!active_voices) {
		return ANSND_ERROR_OK;
	}
	
	u32 level;
	_CPU_ISR_Disable(level);
	
	*active_voices = ansnd_active_voices;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}

s32 ansnd_register_audio_callback(ansnd_audio_callback_t callback, void* callback_arguments) {
	u32 level;
	_CPU_ISR_Disable(level);
	
	ansnd_audio_callback = callback;
	ansnd_audio_callback_arguments = callback_arguments;
	
	_CPU_ISR_Restore(level);
	
	return ANSND_ERROR_OK;
}
