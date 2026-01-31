#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <ansndlib.h>
#include "C4L.h"
#include "C4R.h"

typedef struct {
	u32 sample_count;
	u32 nibble_count;
	u32 sample_rate;
	u16 loop_flag;
	u16 format;
	u32 loop_start_offset;
	u32 loop_end_offset;
	u32 current_address;
	u16 decode_coefficients[16];
	u16 gain;
	u16 initial_predictor_scale;
	u16 initial_sample_history_1;
	u16 initial_sample_history_2;
	u16 loop_predictor_scale;
	u16 loop_sample_history_1;
	u16 loop_sample_history_2;
	u16 reserved[11];
} adpcm_header_t;

static s32 generate_voice_config(ansnd_adpcm_voice_config_t* adpcm_config, void* data, u32 data_size, s16** sound_buffer);
static s32 read_adpcm_header(adpcm_header_t* adpcm_header, void* data, u32 data_size);
static void print_error(s32 error);
static void setup_video();

#if defined(HW_DOL)
#define ARAM_BUFFERS 2
static u32 aram_memory[ARAM_BUFFERS];
#endif

int main(int argc, char** argv) {
	// setup text terminal and controller input
	setup_video();
	
#if defined(HW_DOL)
	// Initialize ARAM for GameCube
	AR_Init(aram_memory, ARAM_BUFFERS);
	
	ARQ_Init();
#endif
	
	printf("ansnd library example program: pitch adpcm\n");
	
	printf("Initializing ansnd library...\n");
	ansnd_initialize();
	printf("ansnd library initialized.\n");
	
	printf("Reading audio data...\n");
	
	// ADPCM decoding does not support multiple channels so both left and right need their own voices
	ansnd_adpcm_voice_config_t adpcm_config_left;
	s16* sound_buffer_left = NULL;
	s32 error = generate_voice_config(&adpcm_config_left, (u8*)C4L, C4L_size, &sound_buffer_left);
	adpcm_config_left.left_volume = 0.5f;
	adpcm_config_left.right_volume = 0.0f;
	if (error < 0) {
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	
	ansnd_adpcm_voice_config_t adpcm_config_right;
	s16* sound_buffer_right = NULL;
	error = generate_voice_config(&adpcm_config_right, (u8*)C4R, C4R_size, &sound_buffer_right);
	adpcm_config_right.left_volume = 0.0f;
	adpcm_config_right.right_volume = 0.5f;
	if (error < 0) {
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	
	printf("Allocating voices...\n");
	const u32 number_voices = 16;
	s32 voices[number_voices];
	u32 voices_index = 0;
	
	// allocating multiple voices so a few can overlap
	for (u32 i = 0; i < number_voices; ++i) {
		s32 voice_id = ansnd_allocate_voice();
		
		if (voice_id < 0) {
			print_error(voice_id);
			printf("Voice allocation failed.\n");
			printf("Exiting...\n");
			VIDEO_WaitVSync();
			return 0;
		}
		
		voices[i] = voice_id;
	}
	printf("Voice allocation complete.\n");
	
	printf("Configuring voices...\n");
	for (u32 i = 0; i < (number_voices / 2); ++i) {
		s32 voice_id_1 = voices[i];
		s32 voice_id_2 = voices[i + (number_voices / 2)];
		
		error = ansnd_configure_adpcm_voice(voice_id_1, &adpcm_config_left);
		print_error(error);
		
		if (error < 0) {
			printf("Voice ID: %d configuration failed.\n", voice_id_1);
			printf("Exiting...\n");
			VIDEO_WaitVSync();
			return 0;
		}
		
		error = ansnd_configure_adpcm_voice(voice_id_2, &adpcm_config_right);
		print_error(error);
		
		if (error < 0) {
			printf("Voice ID: %d configuration failed.\n", voice_id_2);
			printf("Exiting...\n");
			VIDEO_WaitVSync();
			return 0;
		}
		
		error = ansnd_link_voices(voice_id_1, voice_id_2);
		print_error(error);
		
		if (error < 0) {
			printf("Failed to link voices %d and %d.\n", voice_id_1, voice_id_2);
			printf("Exiting...\n");
			VIDEO_WaitVSync();
			return 0;
		}
	}
	printf("Voice configuration complete.\n");
	
	printf("\n\nUse the Left Stick to control the pitch and press A to play.\n\n");
	printf("\n\nPress the START button to exit.\n\n");
	
	while (true) {
		PAD_ScanPads();
		u32 pressed_buttons = PAD_ButtonsDown(0);
		
		if (pressed_buttons & PAD_BUTTON_START) {
			break;
		}
		
		if (pressed_buttons & PAD_BUTTON_A) {
			s32 voice_id = voices[voices_index];
			voices_index = (voices_index + 1) % number_voices;
			
			error = ansnd_stop_voice(voice_id);
			print_error(error);
			
			u8 stick = PAD_StickY(0) + 128;
			f32 new_pitch = (stick / 170.f) + 0.25f;
			
			error = ansnd_set_voice_pitch(voice_id, new_pitch);
			print_error(error);
			
			error = ansnd_start_voice(voice_id);
			print_error(error);
		}
		
		// Wait for the next frame
		VIDEO_WaitVSync();
	}
	
	printf("Deallocating voices\n");
	for (u32 i = 0; i < number_voices; ++i) {
		s32 voice_id = voices[i];
		
		error = ansnd_deallocate_voice(voice_id);
		
		if (error < 0) {
			printf("Voice ID: %d deallocation failed.\n", voice_id);
		}
	}
	printf("All voices deallocated.\n");
	
	printf("Shutting down ansnd library...\n");
	ansnd_uninitialize();
	
	free(sound_buffer_left);
	free(sound_buffer_right);
	
#if defined(HW_DOL)
	for (u32 i = 0; i < ARAM_BUFFERS; ++i) {
		AR_Free(NULL);
	}
#endif
	
	printf("Exiting...\n");
	
	// Update the screen one last time
	VIDEO_WaitVSync();
	
	return 0;
}

static s32 generate_voice_config(ansnd_adpcm_voice_config_t* adpcm_config, void* data, u32 data_size, s16** sound_buffer) {
	adpcm_header_t adpcm_header;
	memset(&adpcm_header, 0, sizeof(adpcm_header_t));
	
	s32 error = read_adpcm_header(&adpcm_header, (void*)data, data_size);
	if (error < 0) {
		return error;
	}
	
	u32 sound_buffer_size = (adpcm_header.nibble_count + 1) / 2;
	
	// There are some errors that appear if the buffer is not
	// a multiple of 32 when using ADPCM on GameCube hardware
	u32 sound_buffer_size_32 = (((sound_buffer_size / 32) + 1) * 32);
	
	// generate a sound data buffer aligned at a 32 byte boundary
	*sound_buffer = (s16*)memalign(32, sound_buffer_size_32);
	
	// copy sound data to the buffer
	memcpy((void*)*sound_buffer, data + sizeof(adpcm_header_t), sound_buffer_size);
	
	// Zero out the rest of the buffer
	memset(((void*)*sound_buffer) + sound_buffer_size, 0, sound_buffer_size_32 - sound_buffer_size);
	
	// flush the sound data from the CPU cache
	DCFlushRange(*sound_buffer, sound_buffer_size_32);
	
#if defined(HW_DOL)
	// on the GameCube, sound samples need to be transferred to ARAM before they can be used
	u32 sound_buffer_ptr = AR_Alloc(sound_buffer_size_32);
	
	ARQRequest aram_request;
	ARQ_PostRequest(&aram_request, 0, ARQ_MRAMTOARAM, ARQ_PRIO_HI, sound_buffer_ptr, (u32)MEM_VIRTUAL_TO_PHYSICAL(*sound_buffer), sound_buffer_size_32);
#elif defined(HW_RVL)
	// Wii just needs to convert the pointer from virtual to physical
	u32 sound_buffer_ptr = (u32)MEM_VIRTUAL_TO_PHYSICAL(*sound_buffer);
#endif
	
	// init the voice config struct
	memset(adpcm_config, 0, sizeof(ansnd_adpcm_voice_config_t));
	
	adpcm_config->samplerate          = adpcm_header.sample_rate;
	adpcm_config->loop_flag           = adpcm_header.loop_flag;
	adpcm_config->nibble_offsets_flag = 1; // most ADPCM file formats use nibble addressing
	
	adpcm_config->adpcm_format = adpcm_header.format;
	adpcm_config->adpcm_gain   = adpcm_header.gain;
	
	adpcm_config->pitch        = 1.0f;
	adpcm_config->left_volume  = 0.5f;
	adpcm_config->right_volume = 0.5f;
	
	adpcm_config->data_ptr     = sound_buffer_ptr;
	adpcm_config->sample_count = adpcm_header.sample_count;
	adpcm_config->start_offset = adpcm_header.current_address;
	for (u32 i = 0; i < 16; ++i) {
		adpcm_config->decode_coefficients[i] = adpcm_header.decode_coefficients[i];
	}
	adpcm_config->initial_predictor_scale  = adpcm_header.initial_predictor_scale;
	adpcm_config->initial_sample_history_1 = adpcm_header.initial_sample_history_1;
	adpcm_config->initial_sample_history_2 = adpcm_header.initial_sample_history_2;
	adpcm_config->loop_predictor_scale     = adpcm_header.loop_predictor_scale;
	adpcm_config->loop_sample_history_1    = adpcm_header.loop_sample_history_1;
	adpcm_config->loop_sample_history_2    = adpcm_header.loop_sample_history_2;
	adpcm_config->loop_start_offset        = adpcm_header.loop_start_offset;
	adpcm_config->loop_end_offset          = adpcm_header.loop_end_offset;
	
	return 0;
}

static s32 read_adpcm_header(adpcm_header_t* adpcm_header, void* data, u32 data_size) {
	if (data_size < sizeof(adpcm_header_t)) {
		printf("Malformed ADPCM file.\n");
		return -1;
	}
	memcpy(adpcm_header, data, sizeof(adpcm_header_t));
	
	// disable looping for this program
	adpcm_header->loop_flag = 0;
	
	if (((adpcm_header->nibble_count + 1) / 2) != (data_size - sizeof(adpcm_header_t))) {
		printf("Malformed ADPCM file.\n");
		return -1;
	}
	return 0;
}

static void print_error(s32 error) {
	if (error == ANSND_ERROR_OK) {
		return;
	}
	printf("\tansnd library Error %d:\n\t\t", error);
	printf("%s\n", ansnd_get_error_string(error));
}

static void *xfb = NULL;

static void setup_video() {
	VIDEO_Init();
	PAD_Init();
	GXRModeObj* rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	console_init(xfb,0,0,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if (rmode->viTVMode & VI_NON_INTERLACE) {
		VIDEO_WaitVSync();
	}
	printf("\nTerminal Output Initialized\n");
}
