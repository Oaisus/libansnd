#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <ansndlib.h>
#include "C4.h"

typedef struct {
	u32 FileTypeBlocID;
	u32 FileSize;
	u32 FileFormatID;
	u32 FormatBlocID;
	u32 BlocSize;
	u16 AudioFormat;
	u16 NbrChannels;
	u32 Frequency;
	u32 BytePerSec;
	u16 BytePerBloc;
	u16 BitsPerSample;
	u32 DataBlocID;
	u32 DataSize;
} wav_header_t;

static s32 read_wav_header(wav_header_t* wav_header, void* data, u32 data_size);
static void print_error(s32 error);
static void setup_video();

int main(int argc, char** argv) {
	// setup text terminal and controller input
	setup_video();
	
	printf("ansnd library example program: pitch\n");
	
	printf("Initializing ansnd library...\n");
	ansnd_initialize();
	printf("ansnd library initialized.\n");
	
	printf("Reading audio data...\n");
	wav_header_t wav_header;
	memset(&wav_header, 0, sizeof(wav_header_t));
	s32 error = read_wav_header(&wav_header, (void*)C4, C4_size);
	if (error < 0) {
		printf("Exiting...\n");
		VIDEO_WaitVSync();
		return 0;
	}
	
	// copy sound data to a buffer aligned at a 32 byte boundary
	u32 sound_buffer_size = wav_header.DataSize;
	u32 sound_buffer_size_32 = (((sound_buffer_size / 32) + 1) * 32);
	s16* sound_buffer = (s16*)memalign(32, sound_buffer_size_32);
	memcpy((void*)sound_buffer, C4 + sizeof(wav_header_t), sound_buffer_size);
	memset(((void*)sound_buffer) + sound_buffer_size, 0, sound_buffer_size_32 - sound_buffer_size);
	// convert sound data from little-endian to big-endian
	for (u32 i = 0; i < sound_buffer_size / 2; ++i) {
		sound_buffer[i] = __builtin_bswap16(sound_buffer[i]);
	}
	
	// flush the sound data from the CPU cache
	DCFlushRange(sound_buffer, sound_buffer_size_32);
	
#if defined(HW_DOL)
	// on the GameCube, sound samples need to be transferred to ARAM before they can be used
	const u32 aram_buffers = 1;
	u32 aram_memory[aram_buffers];
	AR_Init(aram_memory, aram_buffers);
	
	ARQ_Init();
	
	u32 sound_buffer_ptr = AR_Alloc(sound_buffer_size_32);
	
	ARQRequest aram_request;
	ARQ_PostRequest(&aram_request, 0, ARQ_MRAMTOARAM, ARQ_PRIO_HI, sound_buffer_ptr, (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer), sound_buffer_size_32);
#elif defined(HW_RVL)
	// Wii just needs to convert the pointer from virtual to physical
	u32 sound_buffer_ptr = (u32)MEM_VIRTUAL_TO_PHYSICAL(sound_buffer);
#endif
	
	// create a voice config struct
	ansnd_pcm_voice_config_t voice_config;
	memset(&voice_config, 0, sizeof(ansnd_pcm_voice_config_t));
	
	voice_config.samplerate   = wav_header.Frequency;
	voice_config.format       = ANSND_VOICE_PCM_FORMAT_SIGNED_16_PCM;
	voice_config.channels     = wav_header.NbrChannels;
	voice_config.pitch        = 1.0f;
	voice_config.left_volume  = 0.5f;
	voice_config.right_volume = 0.5f;
	
	voice_config.frame_data_ptr = sound_buffer_ptr;
	voice_config.frame_count    = sound_buffer_size / 2 / wav_header.NbrChannels;
	voice_config.start_offset   = 0;
	
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
		
		voices[voices_index] = voice_id;
		voices_index = (voices_index + 1) % number_voices;
	}
	printf("Voice allocation complete.\n");
	
	printf("Configuring voices...\n");
	for (u32 i = 0; i < number_voices; ++i) {
		s32 voice_id = voices[voices_index];
		voices_index = (voices_index + 1) % number_voices;
		
		error = ansnd_configure_pcm_voice(voice_id, &voice_config);
		print_error(error);
		
		if (error < 0) {
			printf("Voice ID: %d configuration failed.\n", voice_id);
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
		s32 voice_id = voices[voices_index];
		voices_index = (voices_index + 1) % number_voices;
		
		error = ansnd_deallocate_voice(voice_id);
		
		if (error < 0) {
			printf("Voice ID: %d deallocation failed.\n", voice_id);
		}
	}
	printf("All voices deallocated.\n");
	
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

static s32 read_wav_header(wav_header_t* wav_header, void* data, u32 data_size) {
	if (data_size < sizeof(wav_header_t)) {
		printf("Malformed WAV file.\n");
		return -1;
	}
	memcpy(wav_header, data, sizeof(wav_header_t));
	
	wav_header->FileSize      = __builtin_bswap32(wav_header->FileSize);
	wav_header->BlocSize      = __builtin_bswap32(wav_header->BlocSize);
	wav_header->AudioFormat   = __builtin_bswap16(wav_header->AudioFormat);
	wav_header->NbrChannels   = __builtin_bswap16(wav_header->NbrChannels);
	wav_header->Frequency     = __builtin_bswap32(wav_header->Frequency);
	wav_header->BytePerSec    = __builtin_bswap32(wav_header->BytePerSec);
	wav_header->BytePerBloc   = __builtin_bswap16(wav_header->BytePerBloc);
	wav_header->BitsPerSample = __builtin_bswap16(wav_header->BitsPerSample);
	wav_header->DataSize      = __builtin_bswap32(wav_header->DataSize);
	
	u32 error = 0;
	error |= wav_header->FileTypeBlocID ^ 0x52494646; // RIFF
	error |= wav_header->FileFormatID   ^ 0x57415645; // WAVE
	error |= wav_header->FormatBlocID   ^ 0x666D7420; // fmt_
	error |= wav_header->DataBlocID     ^ 0x64617461; // data
	if (error) {
		printf("Malformed WAV file.\n");
		return -1;
	}
	if (wav_header->FileSize != (data_size - 8)) {
		printf("Malformed WAV file.\n");
		return -1;
	}
	if (wav_header->BlocSize != 16) {
		printf("Malformed WAV file.\n");
		return -1;
	}
	if (wav_header->AudioFormat != 1) {
		printf("WAV contains unsupported audio format.\n");
		return -1;
	}
	if (wav_header->DataSize > (data_size - sizeof(wav_header_t))) {
		printf("Malformed WAV file.\n");
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
