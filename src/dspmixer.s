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

// Accelerator Control Registers
ACCOEF:     equ 0xFFA0 // Accelerator decode coefficients base

ACFMT:      equ 0xFFD1 // Accelerator format
ACDATA1:    equ 0xFFD3 // Accelerator alternate data in/out
ACSAH:      equ 0xFFD4 // Accelerator start address high
ACSAL:      equ 0xFFD5 // Accelerator start address low
ACEAH:      equ 0xFFD6 // Accelerator end address high
ACEAL:      equ 0xFFD7 // Accelerator end address low
ACCAH:      equ 0xFFD8 // Accelerator current address high
ACCAL:      equ 0xFFD9 // Accelerator current address low
ACPDS:      equ 0xFFDA // Accelerator predictor scale
ACYN1:      equ 0xFFDB // Accelerator history 1
ACYN2:      equ 0xFFDC // Accelerator history 2
ACDAT:      equ 0xFFDD // Accelerator data out
ACGAN:      equ 0xFFDE // Accelerator gain
AMDM:       equ 0xFFDF // ARAM DMA Request Mask

// Mailbox Registers
DIRQ:       equ 0xFFFB // DSP interrupt request
DMBH:       equ 0xFFFC // DSP mailbox high
DMBL:       equ 0xFFFD // DSP mailbox low
CMBH:       equ 0xFFFE // CPU mailbox high
CMBL:       equ 0xFFFF // CPU mailbox low

// DMA Control Registers
DMACR:      equ 0xFFC9 // DMA control
DMABLEN:    equ 0xFFCB // DMA byte length
DMADSPM:    equ 0xFFCD // DMA DSP memory location
DMAMMEMH:   equ 0xFFCE // DMA main memory location high
DMAMMEML:   equ 0xFFCF // DMA main memory location low

DMA_DMEM:   equ 0x0000 // DMA data memory
DMA_IMEM:   equ 0x0002 // DMA instruction memory
DMA_TO_DSP: equ 0x0000 // DMA direction CPU -> DSP
DMA_TO_CPU: equ 0x0001 // DMA direction DSP -> CPU

// --- Defines --- //

// System command specifiers
CMD_SYSTEM_IN:         equ 0xCDD1 // Identifier for system commands in
CMD_SYSTEM_IN_YIELD:   equ 0x0001 // System command in: yield to next task
CMD_SYSTEM_IN_END:     equ 0x0002 // System command in: end task
CMD_SYSTEM_IN_RESUME:  equ 0x0003 // System command in: resume task

CMD_SYSTEM_OUT:        equ 0xDCD1 // Identifier for system commands out
CMD_SYSTEM_OUT_INIT:   equ 0x0000 // System command out: initialized task
CMD_SYSTEM_OUT_RESUME: equ 0x0001 // System command out: resumed task
CMD_SYSTEM_OUT_YIELD:  equ 0x0002 // System command out: yield task
CMD_SYSTEM_OUT_END:    equ 0x0003 // System command out: terminate task
CMD_SYSTEM_OUT_IRQ:    equ 0x0004 // System command out: interrupt request

// Voice command specifiers
CMD_VOICE:             equ 0xFACE // Identifier for voice commands
CMD_VOICE_END:         equ 0xDEAD // Voice command terminate task
CMD_VOICE_NEXT:        equ 0x1111 // Voice command process next set of data
CMD_VOICE_PREPARE:     equ 0x2222 // Voice command prepare for next cycle
CMD_VOICE_MM_LOCATION: equ 0x3333 // Voice command receive main memory base locations
CMD_VOICE_RESTART:     equ 0x4444 // Voice command restart dsp processing cycle

// Accelerator formats
ACCL_FMT_U8BIT:        equ 0x0005 // Accelerator format U8 bit
ACCL_FMT_S8BIT:        equ 0x0019 // Accelerator format S8 bit
ACCL_FMT_U16BIT:       equ 0x0006 // Accelerator format U16 bit
ACCL_FMT_S16BIT:       equ 0x000A // Accelerator format S16 bit
ACCL_FMT_ADPCM:        equ 0x0000 // Accelerator format ADPCM

// Accelerator Gains
ACCL_GAIN_8BIT:        equ 0x0100 // Accelerator gain 8 bit
ACCL_GAIN_16BIT:       equ 0x0800 // Accelerator gain 16 bit
ACCL_GAIN_ADPCM:       equ 0x0000 // Accelerator gain ADPCM

// Voice flags
VOICE_FLAG_RUNNING:    equ 0x0080
VOICE_FLAG_FINISHED:   equ 0x0040
VOICE_FLAG_PAUSED:     equ 0x0020
VOICE_FLAG_DELAY:      equ 0x0010
VOICE_FLAG_STREAMING:  equ 0x0008
VOICE_FLAG_LOOPED:     equ 0x0004
VOICE_FLAG_ADPCM:      equ 0x0002
VOICE_FLAG_STEREO:     equ 0x0001

// Memory defines
MAX_PARAMETER_BLOCKS:        equ 48
NUMBER_SAMPLES:              equ 240
SOUND_BUFFER_SIZE:           equ 960  // size in bytes
PARAMETER_BLOCK_STRUCT_SIZE: equ 128  // size in bytes
WORKING_MEMORY_SIZE:         equ 64   // size in words
DATA_RAM_SIZE:               equ 4096 // size in words

PB_ARRAY_BASE:               equ 0x0000
PB_ARRAY_END:                equ PB_ARRAY_BASE + (PARAMETER_BLOCK_STRUCT_SIZE / 2) * MAX_PARAMETER_BLOCKS
SOUND_BUFFER_BASE:           equ PB_ARRAY_END
SOUND_BUFFER_END:            equ SOUND_BUFFER_BASE + (SOUND_BUFFER_SIZE / 2)
WORKING_MEMORY_BASE:         equ SOUND_BUFFER_END
WORKING_MEMORY_END:          equ WORKING_MEMORY_BASE + WORKING_MEMORY_SIZE

// --- Parameter block offsets --- //

PB_SAMPLE_BUF_1:      equ 0x00

// PCM stereo buffer
PB_SAMPLE_BUF_2:      equ 0x10
// ADPCM decode coefficients
PB_ACC_COEF:          equ 0x10

PB_R_VOL:             equ 0x20
PB_L_VOL:             equ 0x21
PB_REL_FREQ_HI:       equ 0x22
PB_REL_FREQ_LO:       equ 0x23
PB_COUNT_LO:          equ 0x24
PB_DELAY:             equ 0x25
PB_FLAGS:             equ 0x26
PB_SAMPLE_BUF_INDEX:  equ 0x27
PB_SAMPLE_BUF_WRAP:   equ 0x28
PB_FILTER_STEP:       equ 0x29
PB_FILTER_STEP_512:   equ 0x2A
PB_CORRECTION_FACTOR: equ 0x2B

PB_ACC_FORMAT:        equ 0x2C
PB_ACC_START_HI:      equ 0x2D
PB_ACC_START_LO:      equ 0x2E
PB_ACC_END_HI:        equ 0x2F
PB_ACC_END_LO:        equ 0x30
PB_ACC_CURR_HI:       equ 0x31
PB_ACC_CURR_LO:       equ 0x32
PB_ACC_PDS:           equ 0x33
PB_ACC_YN1:           equ 0x34
PB_ACC_YN2:           equ 0x35
PB_ACC_GAIN:          equ 0x36

// looping
PB_LOOP_START_HI:     equ 0x3B
PB_LOOP_START_LO:     equ 0x3C
PB_LOOP_PDS:          equ 0x3D
PB_LOOP_YN1:          equ 0x3E
PB_LOOP_YN2:          equ 0x3F
// streaming
PB_NEXT_START_HI:     equ 0x37
PB_NEXT_START_LO:     equ 0x38
PB_NEXT_END_HI:       equ 0x39
PB_NEXT_END_LO:       equ 0x3A
PB_NEXT_CURR_HI:      equ 0x3B
PB_NEXT_CURR_LO:      equ 0x3C
PB_NEXT_PDS:          equ 0x3D
PB_NEXT_YN1:          equ 0x3E
PB_NEXT_YN2:          equ 0x3F

// --- Working memory addresses --- //

WORK_MMEM_PB_ARRAY_BASE_HI:   equ WORKING_MEMORY_BASE + 0x00
WORK_MMEM_PB_ARRAY_BASE_LO:   equ WORKING_MEMORY_BASE + 0x01

WORK_MMEM_SOUND_BUF_BASE_HI:  equ WORKING_MEMORY_BASE + 0x02
WORK_MMEM_SOUND_BUF_BASE_LO:  equ WORKING_MEMORY_BASE + 0x03
WORK_MMEM_SOUND_BUF_BASE2_HI: equ WORKING_MEMORY_BASE + 0x04
WORK_MMEM_SOUND_BUF_BASE2_LO: equ WORKING_MEMORY_BASE + 0x05

WORK_CURR_PB_ADDR:            equ WORKING_MEMORY_BASE + 0x06

WORK_RESAMPLE_FUNCTION:       equ WORKING_MEMORY_BASE + 0x07
WORK_MIX_FUNCTION:            equ WORKING_MEMORY_BASE + 0x08
WORK_NEXT_SAMPLE_FUNCTION:    equ WORKING_MEMORY_BASE + 0x09

WORK_R_VOL:                   equ WORKING_MEMORY_BASE + 0x0A
WORK_L_VOL:                   equ WORKING_MEMORY_BASE + 0x0B
WORK_REL_FREQ_HI:             equ WORKING_MEMORY_BASE + 0x0C
WORK_REL_FREQ_LO:             equ WORKING_MEMORY_BASE + 0x0D
WORK_COUNT_LO:                equ WORKING_MEMORY_BASE + 0x0E
WORK_DELAY:                   equ WORKING_MEMORY_BASE + 0x0F
WORK_FLAGS:                   equ WORKING_MEMORY_BASE + 0x10
WORK_SAMPLE_BUFFER_INDEX:     equ WORKING_MEMORY_BASE + 0x11
WORK_SAMPLE_BUFFER_WRAP:      equ WORKING_MEMORY_BASE + 0x12
WORK_FILTER_STEP:             equ WORKING_MEMORY_BASE + 0x13
WORK_FILTER_STEP_512:         equ WORKING_MEMORY_BASE + 0x14
WORK_CORRECTION_FACTOR:       equ WORKING_MEMORY_BASE + 0x15

WORK_COEF_PAD_1:              equ WORKING_MEMORY_BASE + 0x16
WORK_RESAMPLING_COEF_BUF:     equ WORKING_MEMORY_BASE + 0x17
WORK_COEF_PAD_2:              equ WORKING_MEMORY_BASE + 0x27
WORK_ERROR_FACTOR:            equ WORKING_MEMORY_BASE + 0x28

WORK_PCM_ACC_COEF:            equ WORKING_MEMORY_BASE + 0x30

// --- Code --- //

_start:
// v Interrupt Table v
	jmp       exception0
	jmp       exception1
	jmp       exception2
	jmp       exception3
	jmp       exception4
	jmp       exception5
	jmp       exception6
	jmp       exception7
// ^ Interrupt Table ^

init:
	lris      $acc0.m, #CMD_SYSTEM_OUT_INIT
	call      send_system_command
	jmp       setup
	
resume:
	lris      $acc0.m, #CMD_SYSTEM_OUT_RESUME
	call      send_system_command
	
setup:
	// configure settings
	lri       $config, #0xFF
	sbset     #0x03 // Interrupt Enable
	clr15
	s40
	m2
	
	// reset indexing registers
	clr       $acc0
	mrr       $ix0,    $acc0.m
	mrr       $ix1,    $acc0.m
	mrr       $ix2,    $acc0.m
	mrr       $ix3,    $acc0.m
	// reset wrapping registers
	lri       $wr0,    #0xFFFF
	mrr       $wr1,    $wr0
	mrr       $wr2,    $wr0
	mrr       $wr3,    $wr0
	
	lri       $acx0.h, #(32768 / 2) // constant used in resampling
	
wait_command:
	clr       $acc0
	clr       $acc1
	
	call      wait_mail_recv
	mrr       $acc1.m, $acc0.m
	
	cmpi      $acc1.m, #CMD_SYSTEM_IN
	jeq       recv_system_command
	
	cmpi      $acc1.m, #CMD_VOICE
	jne       wait_command // message is not a command, ignore
	
	mrr       $acc1.m, $acc0.l
	
	cmpi      $acc1.m, #CMD_VOICE_NEXT
	jeq       process_voices
	
	cmpi      $acc1.m, #CMD_VOICE_PREPARE
	jeq       prepare_for_processing
	
	cmpi      $acc1.m, #CMD_VOICE_RESTART
	jeq       restart_processing
	
	cmpi      $acc1.m, #CMD_VOICE_MM_LOCATION
	jeq       recv_mmem_base
	
	cmpi      $acc1.m, #CMD_VOICE_END
	jeq       terminate_task
	
	jmp       wait_command // unrecognized command, ignore

process_voices:
	call      mix_and_resample
	
	call      send_audio_buffer
	
	si        @DMACR,  #(DMA_DMEM | DMA_TO_CPU)
	call      init_pb_dma
	call      dma_no_wait
	
	call      swap_audio_buffers
	call      clear_audio_buffer
	
	call      wait_dma
	
	lris      $acc0.m, #CMD_SYSTEM_OUT_IRQ
	call      send_system_command
	jmp       wait_command

prepare_for_processing:
	si        @DMACR,  #(DMA_DMEM | DMA_TO_DSP)
	call      init_pb_dma
	call      dma
	
	lris      $acc0.m, #CMD_SYSTEM_OUT_YIELD
	call      send_system_command
	jmp       wait_command

restart_processing:
	lris      $acc0.m, #CMD_SYSTEM_OUT_IRQ
	call      send_system_command
	jmp       wait_command

// clobbers everything
mix_and_resample:
	clr       $acc1
loop_mix_and_resample:
	cmpi      $acc1.m, #PB_ARRAY_END
	retge
	sr        @WORK_CURR_PB_ADDR, $acc1.m
	
	lri       $ix0,    #PB_FLAGS
	call      set_pb_address
	lrr       $acc0.m, @$ar0
	
	xori      $acc0.m, #VOICE_FLAG_RUNNING
	andf      $acc0.m, #(VOICE_FLAG_RUNNING | VOICE_FLAG_FINISHED | VOICE_FLAG_PAUSED | VOICE_FLAG_DELAY)
	jlnz      skip_pb
	
// v Parameter Block Section v
	call      init_parameter_block
// v Core Loop v
	lri       $wr0,    #0x01FC
	bloop     $acc1.m, mixing_complete
	lr        $ar0,    @WORK_RESAMPLE_FUNCTION
	jmpr      $ar0
core_loop_end:
// ^ Core Loop ^
	lri       $wr0,    #0xFFFF
	call      uninit_parameter_block
// ^ Parameter Block Section ^
	
	lr        $acc1.m, @WORK_CURR_PB_ADDR
skip_pb:
	addis     $acc1.m, #(PARAMETER_BLOCK_STRUCT_SIZE / 2)
	jmp       loop_mix_and_resample

// mono
//  next mono sample in $acc0
// stereo
//  next sample left in $acc1
//  next sample right in $acc0
//  part of sample left in $prod
// clobbers $acc0, $acc1, $acx1.h, $ar0, uses $ix3 == 0
mix_mono:
	mov       $acc1,   $acc0
	clrp
mix_stereo:
	lri       $ar0,    #WORK_R_VOL
	addp'l    $acc1                       : $acx1.h, @$ar0
	mulc'l    $acc0.m, $acx1.h            : $acc0.m, @$ar3
	lrr       $acx1.h, @$ar0
	mulcac'ln $acc1.m, $acx1.h, $acc0     : $acc1.m, @$ar3
	addp'dr   $acc1                       : $ar3
	srri      @$ar3,   $acc0.m
mixing_complete:
	srri      @$ar3,   $acc1.m
	jmp       core_loop_end

// clobbers $acc1.m
next_sample_stereo:
	lrs       $acc1.m, @ACDAT
	srri      @$ar2,   $acc1.m
next_sample_mono:
	lrs       $acc1.m, @ACDAT
next_sample_complete:
	srri      @$ar1,   $acc1.m
	jmpr      $ar3

// clobbers $acc0, $acc1, $ar0
resample_no_resample:
	mrr       $st1,    $ar3
	
	lr        $ar0,    @WORK_NEXT_SAMPLE_FUNCTION
	lri       $ar3,    #resample_no_resample_read_end
	jmpr      $ar0
resample_no_resample_read_end:
	dar       $ar1
	lrri      $acc0.m, @$ar1
	dar       $ar2
	clrp'l                                : $acc1.m, @$ar2
	
	jmp       resample_end

// clobbers everything
resample:
	mrr       $st1,    $ar3
	
// v Adjust Relative Frequency Offsets v
	lri       $ar3,    #WORK_REL_FREQ_HI
	clr'l     $acc0                       : $acx1.h, @$ar3
	clr'l     $acc1                       : $acx1.l, @$ar3
	lrr       $acc0.l, @$ar3
	addax     $acc0,   $acx1
	srr       @$ar3,   $acc0.l
// ^ Adjust Relative Frequency Offsets ^
	
// v Read Samples Loop v
	lr        $ar0,    @WORK_NEXT_SAMPLE_FUNCTION
	lri       $ar3,    #resample_read_loop_end
	bloop     $acc0.m, next_sample_complete
	jmpr      $ar0
resample_read_loop_end:
// ^ Read Samples Loop ^
	
// v Calculate Coefficients v
// v Setup Coefficients Addressing v
	lri       $ar3,    #WORK_FILTER_STEP
	
	lsl       $acc0,   #15
	not'l     $acc0.m                     : $acx1.h, @$ar3            // filter step
	andi      $acc0.m, #0x7FFF
	mulc      $acc0.m, $acx1.h
	movp      $acc0
	lsr       $acc0,   #6
	ori       $acc0.m, #0x1403
	mrr       $ar0,    $acc0.m
// ^ Setup Coefficients Addressing ^
	lrri      $ix0,    @$ar3                                          // filter step 512
	
	// set the initial value of the error factor
	// load first resampling coefficient
	// load the correction factor
	// increment $ar3 to the start of the resampling coefficient buffer (pad 1)
	cw        0x65D7 // movr'ldaxn $acc1, $acx0.h : $acx1, @$ar0
	
	// partially unroll the loop to reduce instructions inside
	mul       $acx1.l, $acx1.h
	bloop     $acx0.l, resample_coefficients_loop_end
	cw            0x5FB4 // subp'lsn $acc1 : $acx1.h, $acc0.m
resample_coefficients_loop_end:
	mulmv         $acx1.l, $acx1.h, $acc0
	
	clrp's                                : @$ar3,   $acc0.m
	addr'ir   $acc1.m, $acx0.h            : $ar3
	clr's     $acc0                       : @$ar3,   $acc1.m          // error factor
// ^ Calculate Coefficients ^
	
// v Calculate New Samples v
	lri       $ar3,    #WORK_RESAMPLING_COEF_BUF
	
	// partially unroll the loop to reduce instructions inside
	cw        0x89F3 // clr'ldax $acc1               : $acx1,   @$ar1
	bloop     $acx0.l, resample_samples_loop_end
	mulac'l       $acx1.l, $acx1.h, $acc1            : $acx1.h, @$ar2
resample_samples_loop_end:
	cw            0x9CF3 // mulac'ldax $acx1.l, $acx1.h, $acc0 : $acx1, @$ar1
	
	nx'l                                             : $acx1.h, @$ar3 // error factor
	mulcac'dr $acc0.m, $acx1.h, $acc1                : $ar1
	mulcac    $acc1.m, $acx1.h, $acc0
// ^ Calculate New Samples ^
resample_end:
	mrr       $ar3,    $st1
	
	lr        $ar0,    @WORK_MIX_FUNCTION
	jmpr      $ar0

// --- Parameter block Functions --- //

// loads $ar0 with the address of the current pb with offset in $ix0
// clobbers $acx1.l, $ar0, $ix0
set_pb_address:
	lr        $acx1.l, @WORK_CURR_PB_ADDR
set_pb_address_acx1: // use if $acx1.l already has the current pb address
	mrr       $ar0,    $acx1.l
	addarn    $ar0,    $ix0
	ret

// assumes current pb address is in $acx1.l
// assumes current pb flags is in $acc0.m
// clobbers $acc0, $acx1, $acx0.l, $acx1.h, $ix0, $wr1, $wr2, $ar0, $ar1, $ar2, $ar3
init_parameter_block:
	call      init_accelerator
	
// v load parameter block vals v
	lri       $ix0,    #PB_R_VOL
	call      set_pb_address_acx1
	lri       $ar3,    #WORK_R_VOL
	
	// copy 12 words $ar0 -> $ar3
	bloopi    #12,     init_pb_copy_end
	lrri          $acx0.l, @$ar0
init_pb_copy_end:
	srri          @$ar3,   $acx0.l
// ^ load parameter block vals ^
	
// v Circular Buffer setup v
	lri       $ar3,    #WORK_SAMPLE_BUFFER_INDEX
	clr'l     $acc1                                  : $acx1.h, @$ar3
	
	mrr       $acc1.m, $acx1.l // $acx1.l contains the current pb address
	orr       $acc1.m, $acx1.h
	mrr       $ar1,    $acc1.m // sample buffer 1 address
	addis     $acc1.m, #0x10
	mrr       $ar2,    $acc1.m // sample buffer 2 address
	
	lrr       $acc1.m, @$ar3   // sample buffer wrapping value
	mrr       $wr1,    $acc1.m
	mrr       $wr2,    $acc1.m
	
	incm      $acc1.m
	mrr       $acx0.l, $acc1.m // sample buffer size
// ^ Circular Buffer setup ^
	
// v Resampling Function Pointer setup v
	lri       $ar0,    #WORK_REL_FREQ_LO
	
	lrrd      $acx1.h, @$ar0
	tstaxh'l  $acx1.h                                : $acc1.m, @$ar0
	jne       init_pb_resample
	andcf     $acc1.m, #0x0001
	jlnz      init_pb_resample
	
	lri       $acc1.l, #resample_no_resample
	jmp       init_pb_resample_end
init_pb_resample:
	lri       $acc1.l, #resample
init_pb_resample_end:
	lri       $ar0,    #WORK_RESAMPLE_FUNCTION
	srri      @$ar0,   $acc1.l
// ^ Resampling Function Pointer setup ^
	
// v Mono or Stereo Function Pointers setup v
	andf      $acc0.m, #VOICE_FLAG_STEREO
	jlnz      channels_stereo
channels_mono:
	lri       $acc0.l, #mix_mono
	lri       $acc1.l, #next_sample_mono
	jmp       channels_end
channels_stereo:
	lri       $acc0.l, #mix_stereo
	lri       $acc1.l, #next_sample_stereo
channels_end:
	clr's     $acc0                                  : @$ar0,   $acc0.l
	clr's     $acc1                                  : @$ar0,   $acc1.l
// ^ Mono or Stereo Function Pointers setup ^
	
// v Output Sound Buffer Address & Delay setup v
	lri       $ar0,    #WORK_DELAY
	lrr       $acc0.m, @$ar0
	lri       $acc1.m, #NUMBER_SAMPLES
	sub       $acc1,   $acc0
	jge       init_pb_delay_some_samples
init_pb_delay_no_samples:
	lri       $acx1.h, #NUMBER_SAMPLES
	subr      $acc0.m, $acx1.h
	clr's     $acc1                                  : @$ar0,   $acc0.m
	jmp       init_pb_delay_end
init_pb_delay_some_samples:
	lsl       $acc0,   #1
	addi      $acc0.m, #SOUND_BUFFER_BASE
	mrr       $ar3,    $acc0.m
	srr       @$ar0,   $acc0.l
init_pb_delay_end:
// ^ Output Sound Buffer Address & Delay setup ^
	
	ret

// clobbers $acc0.m, $acx1.l, $ix0, $ar0, $ar3
uninit_parameter_block:
	call      uninit_accelerator
	
	lri       $ix0,    #PB_COUNT_LO
	call      set_pb_address_acx1
	lri       $ar3,    #WORK_COUNT_LO
	
	lrri      $acc0.m, @$ar3
	srri      @$ar0,   $acc0.m
	lrri      $acc0.m, @$ar3
	srri      @$ar0,   $acc0.m
	lrr       $acc0.m, @$ar3
	clr's     $acc1                                  : @$ar0,   $acc0.m
	
	mrr       $acc0.m, $ar1
	andi      $acc0.m, #0x000F
	clr's     $acc0                                  : @$ar0,   $acc0.m
	
	ret

// assumes current pb address is in $acx1.l
// assumes current pb flags is in $acc0.m
// clobbers $acc1.m, $ix0, $ar0
init_accelerator:
	lri       $ix0,    #PB_ACC_FORMAT
	call      set_pb_address_acx1
	
	lrri      $acc1.m, @$ar0
	srs       @ACFMT,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACSAH,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACSAL,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACEAH,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACEAL,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCAH,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCAL,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACPDS,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACYN1,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACYN2,  $acc1.m
	lrr       $acc1.m, @$ar0
	srs       @ACGAN,  $acc1.m
	
	andf      $acc0.m, #VOICE_FLAG_ADPCM
	jlnz      init_accelerator_coef_adpcm
init_accelerator_coef_pcm:
	lri       $ar0,    #WORK_PCM_ACC_COEF
	jmp       init_accelerator_coef_end
init_accelerator_coef_adpcm:
	lri       $ix0,    #PB_ACC_COEF
	call      set_pb_address_acx1
init_accelerator_coef_end:
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+00, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+01, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+02, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+03, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+04, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+05, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+06, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+07, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+08, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+09, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+10, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+11, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+12, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+13, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+14, $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCOEF+15, $acc1.m
	
	ret

// clobbers $acc1.m, $ix0, $ar0
uninit_accelerator:
	lri       $ix0,    #PB_ACC_START_HI
	call      set_pb_address
	
	lrs       $acc1.m, @ACSAH
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACSAL
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACEAH
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACEAL
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACCAH
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACCAL
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACPDS
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACYN1
	srri      @$ar0,   $acc1.m
	lrs       $acc1.m, @ACYN2
	srr       @$ar0,   $acc1.m
	
	ret

// clobbers $acc1.h, $acc1.l, $acx1.l, $ix0
accelerator_address_overflow:
	mrr       $st1,    $ar0
	mrr       $st1,    $acc1.m
	lri       $wr0,    #0xFFFF
	
// v Accelerator Buffer Reset v
	lr        $acc1.m, @WORK_FLAGS
	andf      $acc1.m, #VOICE_FLAG_LOOPED
	jlnz      accelerator_address_overflow_looping
	andf      $acc1.m, #VOICE_FLAG_STREAMING
	jlz       accelerator_address_overflow_no_loop_or_stream
accelerator_address_overflow_streaming:
	lri       $ix0,    #PB_NEXT_START_HI
	call      set_pb_address
	
	clr       $acc1
	mrr       $acx1.l, $acc1.l
	lrr       $acc1.m, @$ar0
	srri      @$ar0,   $acx1.l
	lrr       $acc1.l, @$ar0
	srri      @$ar0,   $acx1.l
	
	// if starting address is zero then it's an invalid buffer
	tst       $acc1
	jeq       accelerator_address_overflow_no_loop_or_stream
	
	srs       @ACSAH,  $acc1.m
	srs       @ACSAL,  $acc1.l
	lrri      $acc1.m, @$ar0
	srs       @ACEAH,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACEAL,  $acc1.m
	
	jmp       accelerator_address_overflow_loop_setup_skip
accelerator_address_overflow_looping:
	lri       $ix0,    #PB_LOOP_START_HI
	call      set_pb_address
accelerator_address_overflow_loop_setup_skip: // same memory used for streaming and looping
	lrri      $acc1.m, @$ar0
	srs       @ACCAH,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACCAL,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACPDS,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACYN1,  $acc1.m
	lrri      $acc1.m, @$ar0
	srs       @ACYN2,  $acc1.m
	
	jmp       accelerator_address_overflow_end
accelerator_address_overflow_no_loop_or_stream:
	
// v Voice Finished v
	lr        $acc1.m, @WORK_FLAGS
	ori       $acc1.m, #VOICE_FLAG_FINISHED
	sr        @WORK_FLAGS, $acc1.m
// ^ Voice Finished ^
	
accelerator_address_overflow_end:
	lri       $wr0,    #0x01FC
	mrr       $acc1.m, $st1
	mrr       $ar0,    $st1
	ret

// --- Memory Operations --- //

// clobbers $acc0, $ar0
clear_audio_buffer:
	clr       $acc0
	lri       $acc0.l, #(SOUND_BUFFER_SIZE / 2)
	lri       $ar0,    #SOUND_BUFFER_BASE
	loop      $acc0.l
	srri      @$ar0,   $acc0.m
	ret

send_audio_buffer:
	si        @DMACR,  #(DMA_DMEM | DMA_TO_CPU)
	lr        $acc0.m, @WORK_MMEM_SOUND_BUF_BASE_HI
	lr        $acc0.l, @WORK_MMEM_SOUND_BUF_BASE_LO
	lri       $acc1.m, #SOUND_BUFFER_BASE
	lri       $acc1.l, #SOUND_BUFFER_SIZE
	call      dma
	ret

// clobbers $acc0.m, $acc1.m, $ar0, $ar3
swap_audio_buffers:
	lri       $ar0,    #WORK_MMEM_SOUND_BUF_BASE_HI
	lri       $ar3,    #WORK_MMEM_SOUND_BUF_BASE2_HI
	
	bloopi    #2,      swap_audio_buffers_loop_end
	lrr           $acc1.l, @$ar0
	lrr           $acc0.l, @$ar3
	srri          @$ar0,   $acc0.l
swap_audio_buffers_loop_end:
	srri          @$ar3,   $acc1.l
	
	ret

recv_mmem_base:
	lri       $ar0,    #WORK_MMEM_PB_ARRAY_BASE_HI
	
	bloopi    #3,      recv_mmem_base_loop_end
	call          wait_mail_recv
	srri          @$ar0,   $acc0.m
recv_mmem_base_loop_end:
	srri          @$ar0,   $acc0.l
	
	jmp       wait_command

init_pb_dma:
	lr        $acc0.m, @WORK_MMEM_PB_ARRAY_BASE_HI
	lr        $acc0.l, @WORK_MMEM_PB_ARRAY_BASE_LO
	lri       $acc1.m, #PB_ARRAY_BASE
	lri       $acc1.l, #(PARAMETER_BLOCK_STRUCT_SIZE * MAX_PARAMETER_BLOCKS)
	ret

// --- Communications --- //

// never used
//// send mail to CPU and wait for confirmed read
//// load mail into $acc0.ml
//send_mail:
//	srs       @DMBH,   $acc0.m
//	srs       @DMBL,   $acc0.l
//wait_mail_sent:
//	lrs       $acc0.m, @DMBH
//	andf      $acc0.m, #0x8000
//	jlnz      wait_mail_sent
//	ret

// wait for new incoming mail
// loads mail into $acc0.ml
wait_mail_recv:
	lrs       $acc0.m, @CMBH
	andcf     $acc0.m, #0x8000
	jlnz      wait_mail_recv
	lrs       $acc0.l, @CMBL
	ret

// initiate DMA transfer
// set direction before calling
// load registers with the following:
// $acc0.m - main mem high
// $acc0.l - main mem low
// $acc1.m - dsp mem
// $acc1.l - length in bytes
// clobbers $acc0.m
dma:
	call      dma_no_wait
wait_dma:
	lrs       $acc0.m, @DMACR
	andf      $acc0.m, #0x0004
	jlnz      wait_dma
	ret

// initiate DMA transfer without waiting for it to finish
dma_no_wait:
	srs       @DMAMMEMH, $acc0.m
	srs       @DMAMMEML, $acc0.l
	srs       @DMADSPM,  $acc1.m
	srs       @DMABLEN,  $acc1.l
	ret

// --- Exceptions --- //

exception0: // Reset
	rti

exception1: // Stack under/overflow
	rti

exception2:
	rti

exception3:
	rti

exception4:
	rti

exception5: // Accelerator address overflow
	call      accelerator_address_overflow
	rti

exception6:
	rti

exception7: // External interrupt from CPU
	rti

// --- Functions for the DSP task system --- //

terminate_task:
	lris      $acc0.m, #CMD_SYSTEM_OUT_END
	call      send_system_command
	jmp       wait_command

// send a system command with the value in $acc0.m
send_system_command:
	si        @DMBH,   #CMD_SYSTEM_OUT
	srs       @DMBL,   $acc0.m
	si        @DIRQ,   #0x0001
	ret

recv_system_command:
	mrr       $acc1.m, $acc0.l
	
	cmpis     $acc1.m, #CMD_SYSTEM_IN_RESUME
	jeq       wait_command
	
	cmpis     $acc1.m, #CMD_SYSTEM_IN_YIELD
	jeq       run_next_task
	
	cmpis     $acc1.m, #CMD_SYSTEM_IN_END
	jeq       0x8000
	halt

run_next_task:
// v Start DMA DSP DRAM to CPU v
	si        @DMACR,  #(DMA_DMEM | DMA_TO_CPU)
	call      wait_mail_recv
	srs       @DMAMMEMH, $acc0.m
	srs       @DMAMMEML, $acc0.l
	call      wait_mail_recv
	mrr       $acc1.m, $acc0.l // save DRAM length
	call      wait_mail_recv
	srs       @DMADSPM, $acc0.l
	srs       @DMABLEN, $acc1.m
// ^ Start DMA DSP DRAM to CPU ^
	
// v Get Values for DMA New Task DSP IRAM to DSP v
// load new task IRAM values into registers
// $ix0 - Main Memory IRAM base location high
// $ix1 - Main Memory IRAM base location low
// $ix2 - DSP IRAM base location
// $ix3 - DSP IRAM length
	call      wait_mail_recv
	mrr       $ix0,    $acc0.m
	mrr       $ix1,    $acc0.l
	call      wait_mail_recv
	mrr       $ix3,    $acc0.l
	call      wait_mail_recv
	mrr       $ix2,    $acc0.l
// ^ Get Values for DMA New Task DSP IRAM to DSP ^
	
// v Get New Task DSP IRAM Entry Vector v
// load new task IRAM entry vector into register
// $ar0 - IRAM entry vector
	call      wait_mail_recv
	mrr       $ar0,    $acc0.l
// ^ Get New Task DSP IRAM Entry Vector ^
	
// v Get Values for DMA New Task DSP DRAM to DSP v
// load new task DRAM values into registers
// $acx0   - Main Memory DRAM base location
// $acx1.h - DSP DRAM base location
// $acx1.l - DSP DRAM length
	call      wait_mail_recv
	mrr       $acx0.h, $acc0.m
	mrr       $acx0.l, $acc0.l
	call      wait_mail_recv
	mrr       $acx1.l, $acc0.l
	call      wait_mail_recv
	mrr       $acx1.h, $acc0.l
// ^ Get Values for DMA New Task DSP DRAM to DSP ^
	
	call      wait_dma // wait for earlier DMA to finish
	jmp       0x80B5   // jump to task switch function in IROM
	halt
