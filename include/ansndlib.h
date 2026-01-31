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

#ifndef __ANSNDLIB_H__
#define __ANSNDLIB_H__

/**
 * @file ansndlib.h
 * @brief The header of the libansnd API
 * 
 * This is the header file of the Another Sound Library API. 
 * It defines all its types and declares all its functions.
 * 
 * For more information on how to use this library, see the included example programs.
 */

/**
 * @defgroup voices Voices
 * @brief Functions and types related to the use and management of voices.
 */

/**
 * @defgroup non-voices Non-Voices
 * @brief Functions and types not directly related to voices.
 */

#include <gctypes.h>

/**
 * @brief The maximum number of voices available for allocation
 * @ingroup voices
 */
#define ANSND_MAX_VOICES              48

#if defined(HW_DOL)
	#define ANSND_DSP_FREQ_32KHZ      (54000000.0f/1686.0f) // ~32028
	#define ANSND_DSP_FREQ_48KHZ      (54000000.0f/1124.0f) // ~48043
#elif defined(HW_RVL)
	#define ANSND_DSP_FREQ_32KHZ      (54000000.0f/1687.5f) // 32000
	#define ANSND_DSP_FREQ_48KHZ      (54000000.0f/1125.0f) // 48000
#else
#error "Neither HW_DOL nor HW_RVL are defined"
#endif

/**
 * @brief The maximum input sample rate supported.
 * 128000 on Wii and ~128114 on GameCube at 32 kHz.
 * 192000 on Wii and ~192171 on GameCube at 48 kHz.
 * @ingroup voices
 */
#define ANSND_MAX_SAMPLERATE_32KHZ    (ANSND_DSP_FREQ_32KHZ * 4)
#define ANSND_MAX_SAMPLERATE_48KHZ    (ANSND_DSP_FREQ_48KHZ * 4)

/**
 * @defgroup output_samplerates Output Samplerates
 * @brief Output Samplerates
 * @ingroup non-voices
 * @addtogroup output_samplerates
 * @{
 */
#define ANSND_OUTPUT_SAMPLERATE_32KHZ          0 ///< Output Samplerate of 32 kHz
#define ANSND_OUTPUT_SAMPLERATE_48KHZ          1 ///< Output Samplerate of 48 kHz
/** @} */

/**
 * @defgroup voice_states Voice States
 * @brief Voice States
 * @ingroup voices
 * @addtogroup voice_states
 * @{
 */
#define ANSND_VOICE_STATE_ERROR               -1 ///< The voice has encountered an error, indicates an error within the library and should not appear
#define ANSND_VOICE_STATE_STOPPED              0 ///< The voice has been stopped by the user
#define ANSND_VOICE_STATE_FINISHED             1 ///< The voice has finished, the available buffers have been exhausted
#define ANSND_VOICE_STATE_PAUSED               2 ///< The voice has been paused by the user
#define ANSND_VOICE_STATE_RUNNING              3 ///< The voice has been started by the user
#define ANSND_VOICE_STATE_ERASED               4 ///< The voice has been deallocated by the user
/** @} */

/**
 * @defgroup pcm_voice_formats PCM Voice Formats
 * @brief PCM Voice Formats
 * @ingroup voices
 * @addtogroup pcm_voice_formats
 * @{
 */
#define ANSND_VOICE_PCM_FORMAT_UNSET           0 //< Placeholder null format
#define ANSND_VOICE_PCM_FORMAT_SIGNED_8_PCM    1 //< Signed 8-bit PCM format, can be LR interleaved
#define ANSND_VOICE_PCM_FORMAT_SIGNED_16_PCM   2 //< Big-Endian Signed 16-bit PCM format, can be LR interleaved
/** @} */

/**
 * @defgroup errors Errors
 * @brief Errors
 * 
 * Errors that may be returned from functions.
 * 
 * @ingroup voices
 * @addtogroup errors
 * @{
 */
#define ANSND_ERROR_OK                         0 ///< No error
#define ANSND_ERROR_NOT_INITIALIZED           -1 ///< Library not initialized
#define ANSND_ERROR_INVALID_CONFIGURATION     -2 ///< Invalid value(s) used to initialize a voice
#define ANSND_ERROR_INVALID_INPUT             -3 ///< Invalid value input into a function
#define ANSND_ERROR_INVALID_SAMPLERATE        -4 ///< Invalid samplerate used for voice initialization or resulting from pitch
#define ANSND_ERROR_INVALID_MEMORY            -5 ///< Invalid memory location given
#define ANSND_ERROR_ALL_VOICES_USED           -6 ///< No available voices to allocate
#define ANSND_ERROR_VOICE_ID_NOT_ALLOCATED    -7 ///< This voice id has not yet been allocated
#define ANSND_ERROR_VOICE_NOT_CONFIGURED      -8 ///< This voice has not been setup yet
#define ANSND_ERROR_VOICE_NOT_INITIALIZED     -9 ///< This voice has not been initialized yet
#define ANSND_ERROR_VOICE_RUNNING            -10 ///< This function cannot be called while the voice is running
#define ANSND_ERROR_VOICE_ALREADY_LINKED     -11 ///< This voice is already linked to another and cannot be linked to a third
#define ANSND_ERROR_VOICE_NOT_LINKED         -12 ///< This voice is not linked to another and cannot be unlinked
#define ANSND_ERROR_DSP_STALLED              -13 ///< The DSP has stalled, likely due to playing too many resampled voices at once
/** @} */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Voice state callback type.
 * 
 * This is the function pointer type for signaling voice states.
 * A voice state callback has the following signature:
 * @code
 * void callback_name(void* user_pointer, s32 voice_state)
 * @endcode
 * 
 * @param[in] user_pointer The user pointer that is supplied at voice configuration.
 * @param[in] voice_state  The current [voice state](@ref voice_states).
 * 
 * @ingroup voices
 */
typedef void (*ansnd_voice_callback_t) (void* user_pointer, s32 voice_state);

/**
 * @brief PCM data buffer type.
 * 
 * This is the type that is passed as an out parameter to the 
 * [PCM stream data callback type](@ref ansnd_pcm_stream_data_callback_t).  
 * 
 * For PCM data, one 'frame' is one sample for each channel.
 * 
 * @ingroup voices
 */
typedef struct ansnd_pcm_data_buffer_t {
	u32 frame_data_ptr; ///< pointer to the start of the data
	u32 frame_count;    ///< the number of frames in the buffer
} ansnd_pcm_data_buffer_t;

/**
 * @brief PCM stream data callback type.
 * 
 * This is the function pointer type for a voice to request more data for streaming a PCM voice. 
 * 
 * @note
 * data_buffer must be set with at least 5 milliseconds worth of data at 48 kHz output or
 * 7.5 milliseconds worth of data at 32 kHz output, 
 * otherwise the voice may finish before more data can be requested.
 * 
 * A PCM stream data callback has the following signature:
 * @code
 * void callback_name(void* user_pointer, ansnd_pcm_data_buffer_t* data_buffer)
 * @endcode
 * 
 * @param[in] user_pointer The user pointer that is supplied at voice configuration.
 * @param[out] data_buffer The [data buffer type](@ref ansnd_pcm_data_buffer_t).
 * 
 * @ingroup voices
 */
typedef void (*ansnd_pcm_stream_data_callback_t) (void* user_pointer, ansnd_pcm_data_buffer_t* data_buffer);

/**
 * @brief PCM voice config type.
 * 
 * This is the type that is used to configure a PCM voice in the corresponding 
 * [function](@ref ansnd_configure_pcm_voice).
 * 
 * @note
 * For PCM data, one 'frame' is one sample for each channel.  
 * i.e. for Stereo S16 PCM, 1 frame == 4 bytes
 * 
 * @ingroup voices
 */
typedef struct ansnd_pcm_voice_config_t {
	u32 samplerate;        ///< The sample rate of input data.
	u8  format;            ///< Signed 8-bit or 16-bit [PCM sample format](@ref pcm_voice_formats)
	u8  channels;          ///< Stereo or mono.
	
	u32 delay;             ///< The delay before input is played in microseconds.
	
	/** 
	 * @brief The pitch for the input to be played at, default is 1.0.
	 * @note
	 * This manipulates the input samplerate to change pitch.  
	 * Samplerates above @ref ANSND_MAX_SAMPLERATE_32KHZ are not supported in 32 kHz mode.  
	 * Samplerates above @ref ANSND_MAX_SAMPLERATE_48KHZ are not supported in 48 kHz mode.  
	 * Ensure that
	 * @code
	 * (pitch * samplerate) <= ANSND_MAX_SAMPLERATE_32KHZ or
	 * (pitch * samplerate) <= ANSND_MAX_SAMPLERATE_48KHZ
	 * @endcode
	 */
	f32 pitch;
	
	f32 left_volume;       ///< Left volume, valid between -1.0 and 1.0.
	f32 right_volume;      ///< Right volume, valid between -1.0 and 1.0.
	
	u32 frame_data_ptr;    ///< The pointer to the start of the data.
	u32 frame_count;       ///< The number of frames in the buffer.
	u32 start_offset;      ///< The offset from the start of frame_data_ptr in frames to the first frame.
	
	u32 loop_start_offset; ///< The offset from the start of frame_data_ptr in frames to the first frame of the loop, set to 0 if not looping.
	u32 loop_end_offset;   ///< The offset from the start of frame_data_ptr in frames to the last frame of the loop, set to 0 if not looping.
	
	ansnd_voice_callback_t           voice_callback;  ///< The [voice state callback](@ref ansnd_voice_callback_t), may be NULL.
	ansnd_pcm_stream_data_callback_t stream_callback; ///< The [stream data callback](@ref ansnd_pcm_stream_data_callback_t), set to NULL for single buffer playback.
	void*                            user_pointer;    ///< The pointer to user data.
} ansnd_pcm_voice_config_t;

/**
 * @brief ADPCM data buffer type.
 * 
 * This is the type that is passed as an out parameter to the 
 * [ADPCM stream data callback type](@ref ansnd_adpcm_stream_data_callback_t).
 * 
 * @ingroup voices
 */
typedef struct ansnd_adpcm_data_buffer_t {
	u32 data_ptr;         ///< pointer to the start of the data
	u32 sample_count;     ///< the number of samples in the buffer
	
	u16 predictor_scale;  ///< ADPCM predictor scale
	u16 sample_history_1; ///< sample history 1 before the sample at start_offset
	u16 sample_history_2; ///< sample history 2 before the sample at start_offset
} ansnd_adpcm_data_buffer_t;

/**
 * @brief ADPCM stream data callback type.
 * 
 * This is the function pointer type for a voice to request more data for streaming an ADPCM voice. 
 * 
 * @note
 * data_buffer must be set with at least 5 milliseconds worth of data at 48 kHz output or
 * 7.5 milliseconds worth of data at 32 kHz output, 
 * otherwise the voice may finish before more data can be requested.
 * 
 * An ADPCM stream data callback has the following signature:
 * @code
 * void callback_name(void* user_pointer, ansnd_adpcm_data_buffer_t* data_buffer)
 * @endcode
 * 
 * @param[in] user_pointer The user pointer that is supplied at voice configuration.
 * @param[out] data_buffer The [data buffer type](@ref ansnd_adpcm_data_buffer_t).
 * 
 * @ingroup voices
 */
typedef void (*ansnd_adpcm_stream_data_callback_t) (void* user_pointer, ansnd_adpcm_data_buffer_t* data_buffer);

/**
 * @brief ADPCM voice config type.
 * 
 * This is the type that is used to configure an ADPCM voice in the corresponding 
 * [function](@ref ansnd_configure_adpcm_voice).
 * 
 * @attention
 * Data offsets may be interpreted as offsets in nibbles or offsets in samples.  
 * Various available ADPCM formats use either.  
 * Use @ref nibble_offsets_flag to signal which type is in use.
 * 
 * @note
 * An ADPCM voice is mono only.  
 * To emulate a stereo voice using ADPCM, two mono voices must be [linked](@ref ansnd_link_voices).  
 * 
 * @ingroup voices
 */
typedef struct ansnd_adpcm_voice_config_t {
	u32 samplerate;               ///< The sample rate of input data.
	u16 loop_flag;                ///< Set this to non-zero to enable looping.
	u16 nibble_offsets_flag;      ///< Set this to non-zero to interpret offsets as nibbles, default is offsets in samples.
	
	u16 adpcm_format;             ///< The ADPCM format of the input data.
	u16 adpcm_gain;               ///< The ADPCM gain of the input data.
	
	u32 delay;                    ///< The delay before input is played in microseconds.
	
	/** 
	 * @brief The pitch for the input to be played at, default is 1.0.
	 * @note
	 * This manipulates the input samplerate to change pitch.  
	 * Samplerates above @ref ANSND_MAX_SAMPLERATE_32KHZ are not supported in 32 kHz mode.  
	 * Samplerates above @ref ANSND_MAX_SAMPLERATE_48KHZ are not supported in 48 kHz mode.  
	 * Ensure that
	 * @code
	 * (pitch * samplerate) <= ANSND_MAX_SAMPLERATE_32KHZ or
	 * (pitch * samplerate) <= ANSND_MAX_SAMPLERATE_48KHZ
	 * @endcode
	 */
	f32 pitch;
	
	f32 left_volume;              ///< Left volume, valid between -1.0 and 1.0.
	f32 right_volume;             ///< Right volume, valid between -1.0 and 1.0.
	
	u32 data_ptr;                 ///< The pointer to the start of the data.
	u32 sample_count;             ///< The number of samples in the buffer.
	u32 start_offset;             ///< The [offset](@ref nibble_offsets_flag) from the start of data_ptr to the first sample.
	
	u16 decode_coefficients[16];  ///< The ADPCM decode coefficients.
	
	u16 initial_predictor_scale;  ///< The ADPCM initial predictor scale.
	u16 initial_sample_history_1; ///< The sample history 1 before the sample at @ref start_offset.
	u16 initial_sample_history_2; ///< The sample history 2 before the sample at @ref start_offset.
	
	u16 loop_predictor_scale;     ///< The ADPCM loop predictor scale, ignored if not looping.
	u16 loop_sample_history_1;    ///< The sample history 1 before the start of the loop, ignored if not looping.
	u16 loop_sample_history_2;    ///< The sample history 2 before the start of the loop, ignored if not looping.
	
	u32 loop_start_offset;        ///< The [offset](@ref nibble_offsets_flag) from the start of data_ptr to the first sample of the loop, ignored if not looping.
	u32 loop_end_offset;          ///< The [offset](@ref nibble_offsets_flag) from the start of data_ptr to the last sample of the loop, ignored if not looping.
	
	ansnd_voice_callback_t             voice_callback;  ///< The [voice state callback](@ref ansnd_voice_callback_t), may be NULL.
	ansnd_adpcm_stream_data_callback_t stream_callback; ///< The [stream data callback](@ref ansnd_adpcm_stream_data_callback_t), set to NULL for single buffer playback.
	void*                              user_pointer;    ///< The pointer to user data.
} ansnd_adpcm_voice_config_t;

/**
 * @brief Initializes the library with an output samplerate of 48 kHz.
 * 
 * This function must be called before any other ansndlib functions can be used.
 * 
 * @ingroup non-voices
 */
void ansnd_initialize();

/**
 * @brief Initializes the library with a specified output samplerate.
 * 
 * This function must be called before any other ansndlib functions can be used.
 * 
 * @param[in] output_samplerate The [Output Samplerate](@ref output_samplerates) of the library.
 * 
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * 
 * @ingroup non-voices
 */
s32 ansnd_initialize_samplerate(u8 output_samplerate);

/**
 * @brief Uninitializes the library.
 * 
 * This function should be called before the program exits.
 * 
 * @ingroup non-voices
 */
void ansnd_uninitialize();

/**
 * @brief Allocates a new voice.
 * 
 * This allocates a voice for use.
 * 
 * @return The ID of the voice on successful allocation.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_ALL_VOICES_USED.
 * 
 * @ingroup voices
 */
s32 ansnd_allocate_voice();

/**
 * @brief Deallocates a voice.
 * 
 * This releases the voice for later reallocation.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * 
 * @ingroup voices
 */
s32 ansnd_deallocate_voice(u32 voice_id);

/**
 * @brief Configures a PCM voice for playback.
 * 
 * @param[in] voice_id     The ID of the voice.
 * @param[in] voice_config The [PCM voice config](@ref ansnd_pcm_voice_config_t) parameters.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_INVALID_SAMPLERATE.
 * @return May return @ref ANSND_ERROR_INVALID_MEMORY.
 * @return May return @ref ANSND_ERROR_INVALID_CONFIGURATION.
 * 
 * @ingroup voices
 */
s32 ansnd_configure_pcm_voice(u32 voice_id, const ansnd_pcm_voice_config_t* voice_config);

/**
 * @brief Configures an ADPCM voice for playback.
 * 
 * @param[in] voice_id     The ID of the voice.
 * @param[in] voice_config The [ADPCM voice config](@ref ansnd_adpcm_voice_config_t) parameters.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_INVALID_SAMPLERATE.
 * @return May return @ref ANSND_ERROR_INVALID_MEMORY.
 * @return May return @ref ANSND_ERROR_INVALID_CONFIGURATION.
 * 
 * @ingroup voices
 */
s32 ansnd_configure_adpcm_voice(u32 voice_id, const ansnd_adpcm_voice_config_t* voice_config);

/**
 * @brief Links two voices.
 * 
 * All functions used to control a voice will be mirrored to the linked voice except volume.
 * e.g. start, stop, pause, unpause, stop looping, and set pitch.  
 * Voices cannot be linked if either is currently playing, paused, or already linked.  
 * Deallocating a voice will automatically unlink it.  
 * Configuring a voice will preserve a link but will not do anything else.  
 * Reconfiguring one linked voice and not the other will cause them to desync.  
 * This is meant to be used to keep two ADPCM voices in sync to emulate a stereo voice.  
 * 
 * @param[in] voice_id_1 The ID of the first voice.
 * @param[in] voice_id_2 The ID of the second voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_RUNNING.
 * @return May return @ref ANSND_ERROR_VOICE_ALREADY_LINKED.
 * 
 * @ingroup voices
 */
s32 ansnd_link_voices(u32 voice_id_1, u32 voice_id_2);

/**
 * @brief Unlinks previously linked voices.
 * 
 * See @ref ansnd_link_voices for context on voice linking.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_LINKED.
 * 
 * @ingroup voices
 */
s32 ansnd_unlink_voice(u32 voice_id);

/**
 * @brief Starts the voice.
 * 
 * If the voice has a streaming callback, the initial buffer will 
 * be cleared and the stream can not be started again without 
 * reconfiguring the voice with new data.
 * 
 * If the DSP is taking too long to process the currently playing voices, 
 * then this function will fail to prevent making the problem worse.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_DSP_STALLED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * 
 * @ingroup voices
 */
s32 ansnd_start_voice(u32 voice_id);

/**
 * @brief Stops the voice.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * 
 * @ingroup voices
 */
s32 ansnd_stop_voice(u32 voice_id);

/**
 * @brief Pauses the voice.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * 
 * @ingroup voices
 */
s32 ansnd_pause_voice(u32 voice_id);

/**
 * @brief Unpauses the voice.
 * 
 * If the DSP is taking too long to process the currently playing voices, 
 * then this function will fail to prevent making the problem worse.
 * 
 * If the voice is not paused then this function does nothing.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_DSP_STALLED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * 
 * @ingroup voices
 */
s32 ansnd_unpause_voice(u32 voice_id);

/**
 * @brief Sets the voice to stop looping.
 * 
 * The voice will stop looping and play to the end of its buffer.
 * If the voice has a streaming callback, it will be used to request more data.
 * 
 * If the voice is not looped then this function does nothing.
 * 
 * @param[in] voice_id The ID of the voice.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_INITIALIZED.
 * 
 * @ingroup voices
 */
s32 ansnd_stop_looping(u32 voice_id);

/**
 * @brief Sets the volume of a voice.
 * 
 * @param[in] voice_id     The ID of the voice.
 * @param[in] left_volume  The new left volume of the voice, valid between -1.0 and 1.0.
 * @param[in] right_volume The new right volume of the voice, valid between -1.0 and 1.0.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * 
 * @ingroup voices
 */
s32 ansnd_set_voice_volume(u32 voice_id, f32 left_volume, f32 right_volume);

/**
 * @brief Sets the pitch of a voice.
 * 
 * @note
 * This manipulates the input samplerate to change pitch.  
 * Samplerates above @ref ANSND_MAX_SAMPLERATE_32KHZ are not supported in 32 kHz mode.  
 * Samplerates above @ref ANSND_MAX_SAMPLERATE_48KHZ are not supported in 48 kHz mode.  
 * Ensure that
 * @code
 * (pitch * samplerate) <= ANSND_MAX_SAMPLERATE_32KHZ or
 * (pitch * samplerate) <= ANSND_MAX_SAMPLERATE_48KHZ
 * @endcode
 * 
 * @param[in] voice_id The ID of the voice.
 * @param[in] pitch    The new pitch of the voice, default is 1.0.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_INVALID_INPUT.
 * @return May return @ref ANSND_ERROR_VOICE_ID_NOT_ALLOCATED.
 * @return May return @ref ANSND_ERROR_VOICE_NOT_CONFIGURED.
 * @return May return @ref ANSND_ERROR_VOICE_RUNNING.
 * @return May return @ref ANSND_ERROR_INVALID_SAMPLERATE.
 * 
 * @ingroup voices
 */
s32 ansnd_set_voice_pitch(u32 voice_id, f32 pitch);

/**
 * @brief Gets the DSP processing time.
 * 
 * The DSP processing time is the time taken by the DSP to process voices in one cycle.
 * 
 * @param[out] dsp_usage The time used by the DSP expressed as a percentage in the range 0.0f - 1.0f, may be NULL.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_DSP_STALLED.
 * 
 * @ingroup non-voices
 */
s32 ansnd_get_dsp_usage_percent(f32* dsp_usage);

/**
 * @brief Gets the total processing time.
 * 
 * The total processing time is the time taken to process everything in one cycle.
 * This includes the DSP processing time and time taken in user callbacks.
 * 
 * @param[out] total_usage The total time used expressed as a percentage in the range 0.0f - 1.0f, may be NULL.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * @return May return @ref ANSND_ERROR_DSP_STALLED.
 * 
 * @attention
 * Taking too much time to process everything will result in errors in the final output stream as it is starved for input.
 * 
 * @ingroup non-voices
 */
s32 ansnd_get_total_usage_percent(f32* total_usage);

/**
 * @brief Gets the total number of active voices.
 * 
 * Active voices are those that are actively playing.  
 * Voices that are unallocated, stopped, paused, or finished are not active.
 * 
 * @param[out] active_voices The number of active voices, may be NULL.
 * 
 * @return May return @ref ANSND_ERROR_NOT_INITIALIZED.
 * 
 * @ingroup non-voices
 */
s32 ansnd_get_total_active_voices(u32* active_voices);

/**
 * @brief Audio buffer callback type.
 * 
 * This is the function pointer type for retrieving the final audio buffer 
 * before it is sent to the console's audio output.
 * 
 * @attention
 * The format of the audio buffer is Right-Left interleaved Big-Endian Signed 16-bit PCM.
 * 
 * @note
 * Any changes made will be reflected in the final output.
 * 
 * An audio buffer callback has the following signature:
 * @code
 * void callback_name(void* audio_buffer, u32 buffer_length, void* callback_arguments)
 * @endcode
 * 
 * @param[in] audio_buffer       The pointer to the first byte of the buffer.
 * @param[in] buffer_length      The length of the buffer in bytes.
 * @param[in] callback_arguments The pointer that was supplied to @ref ansnd_register_audio_callback.
 * 
 * @ingroup non-voices
 */
typedef void (*ansnd_audio_callback_t) (void* audio_buffer, u32 buffer_length, void* callback_arguments);

/**
 * @brief Sets the audio buffer callback.
 * 
 * This function sets the audio buffer callback, 
 * which is called after the DSP has finished generating it.
 * 
 * @param[in] callback           The new [audio buffer callback](@ref ansnd_audio_callback_t).
 * @param[in] callback_arguments A pointer that will be supplied to the new callback.
 * 
 * @return Will return @ref ANSND_ERROR_OK.
 * 
 * @ingroup non-voices
 */
s32 ansnd_register_audio_callback(ansnd_audio_callback_t callback, void* callback_arguments);

#ifdef __cplusplus
}
#endif

#endif
