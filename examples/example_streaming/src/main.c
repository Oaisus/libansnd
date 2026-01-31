#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <tremor/ivorbiscodec.h>
#include <tremor/ivorbisfile.h>
#include <ogc/machine/processor.h>

#include <ansndlib.h>
#include "Canon.h"

#define SOUND_BUFFER_SIZE 5120
#define SOUND_BUFFERS     2

static u8 sound_buffer[SOUND_BUFFERS][SOUND_BUFFER_SIZE] ATTRIBUTE_ALIGN(32);
static u32 bytes_available[SOUND_BUFFERS];
static u8  next_buffer = 0;

static FILE*          file = NULL;
static OggVorbis_File vorbis_file;
static vorbis_info*   info = NULL;

static ansnd_pcm_voice_config_t voice_config;
static s32                      voice_id;

#if defined(HW_DOL)
static u32 aram_memory[SOUND_BUFFERS];
static u32 aram_blocks[SOUND_BUFFERS];
#endif

static void stream_callback(void* user_pointer, ansnd_pcm_data_buffer_t* data_buffer) {
	if (bytes_available[next_buffer] == 0) {
		return;
	}
	
#if defined(HW_DOL)
	// ARAM pointers are not converted
	u32 sound_buffer_ptr = aram_blocks[next_buffer];
#elif defined(HW_RVL)
	// Wii needs to convert the pointer from virtual to physical
	u32 sound_buffer_ptr = (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer[next_buffer]);
#endif
	
	data_buffer->frame_data_ptr  = sound_buffer_ptr;
	data_buffer->frame_count     = bytes_available[next_buffer] / 2 / info->channels;
	bytes_available[next_buffer] = 0;
	
	next_buffer ^= 1;
}

static s32 reset_data();
static s32 read_data();
static s32 open_file(void* data, u32 data_size);
static void print_error(s32 error);
static void setup_video();

int main(int argc, char** argv) {
	// setup text terminal and controller input
	setup_video();
	
#if defined(HW_DOL)
	// Initialize ARAM for GameCube
	AR_Init(aram_memory, SOUND_BUFFERS);
	
	ARQ_Init();
	
	for (u32 i = 0; i < SOUND_BUFFERS; ++i) {
		aram_blocks[i] = AR_Alloc(SOUND_BUFFER_SIZE);
	}
#endif
	
	printf("ansnd library example program: streaming\n");
	
	printf("Initializing ansnd library...\n");
	ansnd_initialize();
	printf("ansnd library initialized.\n");
	
	printf("Reading audio file...\n");
	s32 error = open_file((void*)Canon, Canon_size);
	if (error < 0) {
		ov_clear(&vorbis_file);
		fclose(file);
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	
	// init the voice config struct
	memset(&voice_config, 0, sizeof(ansnd_pcm_voice_config_t));
	
	voice_config.samplerate      = info->rate;
	voice_config.format          = ANSND_VOICE_PCM_FORMAT_SIGNED_16_PCM;
	voice_config.channels        = info->channels;
	voice_config.pitch           = 1.0f;
	voice_config.left_volume     = 1.0f;
	voice_config.right_volume    = 1.0f;
	voice_config.stream_callback = stream_callback;
	
	printf("Allocating voice...\n");
	voice_id = ansnd_allocate_voice();
	if (voice_id < 0) {
		print_error(voice_id);
		printf("Voice allocation failed.\n");
		ov_clear(&vorbis_file);
		fclose(file);
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	printf("Voice allocation complete.\n");
	
	printf("Preparing stream buffers...\n");
	error = reset_data();
	if (error < 0) {
		printf("Error preparing buffers.\n");
	}
	
	printf("\n\nAudio Source: Canon in D Major - Kevin MacLeod.\n");
	
	printf("\n\nPress A to play.\n");
	printf("Press B to stop.\n");
	printf("\n\nPress the START button to exit.\n\n");
	
	while (true) {
		PAD_ScanPads();
		u32 pressed_buttons = PAD_ButtonsDown(0);
		
		if (pressed_buttons & PAD_BUTTON_START) {
			break;
		}
		
		if (pressed_buttons & PAD_BUTTON_A) {
			error = ansnd_stop_voice(voice_id);
			print_error(error);
			
			error = reset_data();
			if (error < 0) {
				printf("Error starting playback.\n");
			} else {
				error = ansnd_start_voice(voice_id);
				print_error(error);
			}
		}
		
		if (pressed_buttons & PAD_BUTTON_B) {
			error = ansnd_stop_voice(voice_id);
			print_error(error);
		}
		
		if (bytes_available[next_buffer] == 0) {
			read_data();
		}
		
		// Wait for the next frame
		VIDEO_WaitVSync();
	}
	
	printf("Deallocating voice\n");
	error = ansnd_deallocate_voice(voice_id);
	if (error < 0) {
		printf("Voice deallocation failed.\n");
	}
	printf("Voice deallocated.\n");
	
	printf("Shutting down ansnd library...\n");
	ansnd_uninitialize();
	
#if defined(HW_DOL)
	for (u32 i = 0; i < SOUND_BUFFERS; ++i) {
		AR_Free(NULL);
	}
#endif
	
	ov_clear(&vorbis_file);
	fclose(file);
	
	printf("Exiting...\n");
	
	// Update the screen one last time
	VIDEO_WaitVSync();
	
	return 0;
}

static s32 reset_data() {
	s32 error = ov_pcm_seek(&vorbis_file, 0);
	if (error < 0) {
		return -1;
	}
	
	// reset all buffers
	for (u32 i = 0; i < SOUND_BUFFERS; ++i) {
		bytes_available[i] = 0;
		next_buffer = i;
		read_data();
	}
	next_buffer = 0;
	
#if defined(HW_DOL)
	// ARAM pointers are not converted
	u32 sound_buffer_ptr = aram_blocks[next_buffer];
#elif defined(HW_RVL)
	// Wii needs to convert the pointer from virtual to physical
	u32 sound_buffer_ptr = (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer[next_buffer]);
#endif
	
	voice_config.frame_data_ptr  = sound_buffer_ptr;
	voice_config.frame_count     = bytes_available[next_buffer] / 2 / info->channels;
	bytes_available[next_buffer] = 0;
	
	next_buffer ^= 1;
	
	error = ansnd_configure_pcm_voice(voice_id, &voice_config);
	print_error(error);
	
	if (error < 0) {
		printf("Voice configuration failed.\n");
	}
	
	return error;
}

static s32 read_data() {
	if (bytes_available[next_buffer] != 0) {
		return -1;
	}
	
	s32 total_bytes_read = 0;
	while (total_bytes_read < SOUND_BUFFER_SIZE) {
		s32 bytes_read = ov_read(&vorbis_file, (char*)sound_buffer[next_buffer] + total_bytes_read, SOUND_BUFFER_SIZE - total_bytes_read, NULL);
		if (bytes_read <= 0) {
			break;
		}
		
		total_bytes_read += bytes_read;
	}
	
	if (total_bytes_read > 0) {
		DCFlushRange(sound_buffer[next_buffer], total_bytes_read);
		
	#if defined(HW_DOL)
		// on the GameCube, sound samples need to be transferred to ARAM before they can be used
		ARQRequest aram_request;
		ARQ_PostRequest(&aram_request, next_buffer, ARQ_MRAMTOARAM, ARQ_PRIO_HI, aram_blocks[next_buffer], (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer[next_buffer]), total_bytes_read);
	#endif
		
		bytes_available[next_buffer] = total_bytes_read;
	}
	
	if (total_bytes_read == 0) {
		return -1;
	}
	
	return total_bytes_read;
}

static s32 open_file(void* data, u32 data_size) {
	FILE* file = fmemopen(data, data_size, "r");
	if (file == NULL) {
		printf("Failed to open file.\n");
		return -1;
	}
	
	s32 error = ov_open(file, &vorbis_file, NULL, 0);
	if (error < 0) {
		printf("Failed to initialize Vorbis File.\n");
		return -1;
	}
	
	info = ov_info(&vorbis_file, -1);
	
	if (info == NULL) {
		printf("Error reading Vorbis info.\n");
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
