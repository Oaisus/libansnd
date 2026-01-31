#include <gccore.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <ansndlib.h>

static void print_error(s32 error);
static void setup_video();

static void voice_callback(void* user_pointer, s32 voice_state) {
	// move cursor to position saved earlier
	printf("\x1b[u\x1b[0K");
	
	switch (voice_state) {
	case ANSND_VOICE_STATE_ERROR:
		printf("error");
		break;
	case ANSND_VOICE_STATE_STOPPED:
		printf("stopped");
		break;
	case ANSND_VOICE_STATE_FINISHED:
		printf("finished");
		break;
	case ANSND_VOICE_STATE_PAUSED:
		printf("paused");
		break;
	case ANSND_VOICE_STATE_RUNNING:
		printf("running");
		break;
	case ANSND_VOICE_STATE_ERASED:
		printf("erased");
		break;
	default:
		printf("unrecognized state");
	}
}

int main(int argc, char** argv) {
	// setup text terminal and controller input
	setup_video();
	
	printf("ansnd library example program: state callback\n");
	
	printf("Initializing ansnd library...\n");
	ansnd_initialize();
	printf("ansnd library initialized.\n");
	
	printf("Generating audio data...\n");
	
	// generate a simple 500Hz sine wave
	u32 sound_length_seconds = 5;
	u32 sound_sample_rate    = 48000;
	u32 sound_frequency_hz   = 500;
	
	// create a buffer aligned at a 32 byte boundary
	u32 sound_samples     = sound_sample_rate * sound_length_seconds;
	u32 sound_buffer_size = sound_samples * 2;
	s16* sound_buffer     = (s16*)memalign(32, sound_buffer_size);
	
	// generate samples
	for (u32 i = 0; i < sound_samples; ++i) {
		sound_buffer[i] = sin(2 * M_PI * sound_frequency_hz * ((f32)i / (f32)sound_sample_rate)) * 0.95f * 32767;
	}
	
	// flush the sound data from the CPU cache
	DCFlushRange(sound_buffer, sound_buffer_size);
	
#if defined(HW_DOL)
	// on the GameCube, sound samples need to be transferred to ARAM before they can be used
	const u32 aram_buffers = 1;
	u32 aram_memory[aram_buffers];
	AR_Init(aram_memory, aram_buffers);
	
	ARQ_Init();
	
	u32 sound_buffer_ptr = AR_Alloc(sound_buffer_size);
	
	ARQRequest aram_request;
	ARQ_PostRequest(&aram_request, 0, ARQ_MRAMTOARAM, ARQ_PRIO_HI, sound_buffer_ptr, (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer), sound_buffer_size);
#elif defined(HW_RVL)
	// Wii just needs to convert the pointer from virtual to physical
	u32 sound_buffer_ptr = (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer);
#endif
	
	// create a voice config struct
	ansnd_pcm_voice_config_t voice_config;
	memset(&voice_config, 0, sizeof(ansnd_pcm_voice_config_t));
	
	voice_config.samplerate   = sound_sample_rate;
	voice_config.format       = ANSND_VOICE_PCM_FORMAT_SIGNED_16_PCM;
	voice_config.channels     = 1;
	voice_config.pitch        = 1.0f;
	voice_config.left_volume  = 0.5f;
	voice_config.right_volume = 0.5f;
	
	// must convert sound buffer pointer from virtual to physical
	voice_config.frame_data_ptr = sound_buffer_ptr;
	voice_config.frame_count    = sound_samples;
	voice_config.start_offset   = 0;
	
	// set the voice state callback
	voice_config.voice_callback = voice_callback;
	
	printf("Allocating voice...\n");
	s32 voice_id = ansnd_allocate_voice();
	if (voice_id < 0) {
		print_error(voice_id);
		printf("Voice allocation failed.\n");
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	printf("Voice allocation complete.\n");
	
	printf("Configuring voice...\n");
	s32 error = ansnd_configure_pcm_voice(voice_id, &voice_config);
	if (error < 0) {
		print_error(error);
		printf("Voice configuration failed.\n");
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	printf("Voice configuration complete.\n");
	
	printf("\n\nGenerated sound:\n");
	printf("\t%dHz sine wave\n", sound_frequency_hz);
	printf("\t%d seconds long\n", sound_length_seconds);
	
	// save this cursor position for later
	printf("\nVoice state: \x1b[s\n");
	
	printf("\nPress A to play.\n");
	printf("Press B to stop.\n");
	printf("Press X to pause.\n");
	printf("Press Y to resume.\n");
	printf("\nPress the START button to exit.\n\n");
	
	while (true) {
		PAD_ScanPads();
		u32 pressed_buttons = PAD_ButtonsDown(0);
		
		if (pressed_buttons & PAD_BUTTON_START) {
			break;
		}
		
		if (pressed_buttons & PAD_BUTTON_A) {
			error = ansnd_start_voice(voice_id);
			print_error(error);
		}
		
		if (pressed_buttons & PAD_BUTTON_B) {
			error = ansnd_stop_voice(voice_id);
			print_error(error);
		}
		
		if (pressed_buttons & PAD_BUTTON_X) {
			error = ansnd_pause_voice(voice_id);
			print_error(error);
		}
		
		if (pressed_buttons & PAD_BUTTON_Y) {
			error = ansnd_unpause_voice(voice_id);
			print_error(error);
		}
		
		// Wait for the next frame
		VIDEO_WaitVSync();
	}
	
	// move cursor to the bottom of the screen
	printf("\x1b[8B\n");
	printf("Deallocating voice\n");
	// flush stdout
	fflush(stdout);
	
	error = ansnd_deallocate_voice(voice_id);
	
	// wait a bit for the erased voice state to print
	usleep(4000);
	// move cursor back to the bottom of the screen
	printf("\x1b[9B\n");
	
	if (error < 0) {
		printf("Voice deallocation failed.\n");
	}
	printf("Voice deallocated.\n");
	
	printf("Shutting down ansnd library...\n");
	ansnd_uninitialize();
	
	free(sound_buffer);
	
#if defined(HW_DOL)
	for (u32 i = 0; i < aram_buffers; ++i) {
		AR_Free(NULL);
	}
#endif
	
	printf("Exiting...\n");
	
	// Update the screen one last time
	VIDEO_WaitVSync();
	
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
