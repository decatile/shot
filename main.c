#include "miniaudio.h"
#include <asm-generic/errno-base.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

extern const unsigned char _binary_sound_mp3_start[];
extern const unsigned char _binary_sound_mp3_end[];

const char *program;

// PARSE COMMAND LINE

struct {
  bool force;
  int pid;
} args = {0};

bool args_parse(int argc, char **argv) {
  char *endptr;

  for (;;) {
    switch (getopt(argc, argv, "fh")) {
    case -1:
      goto args;

    case 'f':
      args.force = 1;
      break;

    case 'h':
      printf("Usage:\n"
             " %s [options...] [pid]\n\n"
             "Options:\n"
             " -h  display this help and exit\n"
             " -f  ignore if process does not exist\n",
             program);
      return false;
    }
  }

args:
  if (argc == optind) {
    fprintf(stderr, "%s: expected process PID\n", program);
    return false;
  }

  args.pid = strtol(argv[optind], &endptr, 10);
  if (*endptr != '\0') {
    fprintf(stderr, "%s: failed to parse process PID\n", program);
    return false;
  }

  return true;
}

// SOUND

ma_device device;

void sound_dispose(void) { ma_device_uninit(&device); }

void sound_callback(ma_device *device, void *output, const void *input,
                    ma_uint32 frameCount) {
  (void)input;

  ma_decoder_read_pcm_frames((ma_decoder *)device->pUserData, output,
                             frameCount, NULL);
}

bool sound_setup(void) {
  ma_decoder decoder;
  ma_decoder_config decoderConfig;
  ma_device_config deviceConfig;

  decoderConfig = ma_decoder_config_init_default();

  if (ma_decoder_init_memory(_binary_sound_mp3_start,
                             _binary_sound_mp3_end - _binary_sound_mp3_start,
                             &decoderConfig, &decoder) != MA_SUCCESS) {
    fprintf(stderr, "%s: failed to initialize inmemory decoder", program);
    return false;
  }

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = decoder.outputFormat;
  deviceConfig.playback.channels = decoder.outputChannels;
  deviceConfig.sampleRate = decoder.outputSampleRate;
  deviceConfig.dataCallback = sound_callback;
  deviceConfig.pUserData = &decoder;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    fprintf(stderr, "%s: failed to open playback device", program);
    return false;
  }

  atexit(sound_dispose);

  if (ma_device_start(&device) != MA_SUCCESS) {
    fprintf(stderr, "%s: failed to start playback device", program);
    return false;
  }

  return true;
}

// SIGNAL

bool process_kill(void) {
  if (kill(args.pid, 9) == 0) {
    return true;
  }

  switch (errno) {
  case ESRCH:
    if (args.force) {
      return true; // We don't care
    }
    fprintf(stderr, "I can't find this process...\n");
    return false;

  case EPERM:
    fprintf(stderr, "Nah, I have no right to kill this thing...\n");
    return false;

  default:
    perror("It's quite interesting");
    return false;
  }
}

int main(int argc, char **argv) {
  struct timespec time;

  program = argv[0];

  if (!args_parse(argc, argv)) {
    return 1;
  }

  if (!sound_setup()) {
    return 1;
  }

  if (!process_kill()) {
    return 1;
  }

  time.tv_sec = 1;
  time.tv_nsec = 1000 * 1000 * 570;
  nanosleep(&time, NULL);

  return 0;
}
