#include "miniaudio.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern const unsigned char _binary_sound_mp3_start[];
extern const unsigned char _binary_sound_mp3_end[];

const char *program;
pthread_t sound_thread;

struct {
  bool force;
  int (*fprintf)(FILE *restrict __stream, const char *restrict __format, ...);
  int *pids;
  int pidsize;
} args = {.force = 0, .fprintf = fprintf};

int noop_fprintf(FILE *restrict __stream, const char *restrict __format, ...) {
  (void)__stream;
  (void)__format;

  return 0;
}

// PARSE COMMAND LINE

bool args_parse_options(int argc, char **argv) {
  for (;;) {
    switch (getopt(argc, argv, "fqh")) {
    case -1:
      return true;

    case 'f':
      args.force = 1;
      break;

    case 'q':
      args.fprintf = noop_fprintf;
      break;

    case 'h':
      printf("Usage:\n"
             " %s [options...] pids...\n\n"
             "Options:\n"
             " -h  display this help and exit\n"
             " -f  ignore errors\n"
             " -q  shut up\n",
             program);
      return false;
    }
  }
}

bool args_parse_pids(char **array, int arraylen) {
  char *endptr;

  if (arraylen == 0) {
    args.fprintf(stderr, "%s: expected process PID\n", program);
    return false;
  }

  args.pids = malloc(sizeof(int) * arraylen);
  args.pidsize = arraylen;

  for (int i = 0; i < arraylen; i++) {
    args.pids[i] = strtol(array[i], &endptr, 10);
    if (*endptr != '\0') {
      args.fprintf(stderr, "%s: failed to parse process PID\n", program);
      return false;
    }
  }

  return true;
}

bool args_parse(int argc, char **argv) {
  if (!args_parse_options(argc, argv)) {
    return false;
  }

  if (!args_parse_pids(argv + optind, argc - optind)) {
    return false;
  }

  return true;
}

// SOUND

ma_device device;

void sound_callback(ma_device *device, void *output, const void *input,
                    ma_uint32 frameCount) {
  (void)input;

  ma_decoder_read_pcm_frames((ma_decoder *)device->pUserData, output,
                             frameCount, NULL);
}

void *sound_setup_routine(void *arg) {
  (void)arg;

  ma_decoder decoder;
  ma_decoder_config decoderConfig;
  ma_device_config deviceConfig;
  struct timespec time;

  decoderConfig = ma_decoder_config_init_default();

  if (ma_decoder_init_memory(_binary_sound_mp3_start,
                             _binary_sound_mp3_end - _binary_sound_mp3_start,
                             &decoderConfig, &decoder) != MA_SUCCESS) {
    args.fprintf(stderr, "%s: failed to initialize inmemory decoder", program);
    return NULL;
  }

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = decoder.outputFormat;
  deviceConfig.playback.channels = decoder.outputChannels;
  deviceConfig.sampleRate = decoder.outputSampleRate;
  deviceConfig.dataCallback = sound_callback;
  deviceConfig.pUserData = &decoder;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    args.fprintf(stderr, "%s: failed to open playback device", program);
    return NULL;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    args.fprintf(stderr, "%s: failed to start playback device", program);
    return NULL;
  }

  time.tv_sec = 1;
  time.tv_nsec = 1000 * 1000 * 570;
  nanosleep(&time, NULL);

  return NULL;
}

void sound_start(void) {
  pthread_create(&sound_thread, NULL, sound_setup_routine, NULL);
}

void sound_join(void) { pthread_join(sound_thread, NULL); }

// SIGNAL

bool process_kill_single(int pid) {
  if (kill(pid, 9) == 0) {
    return true;
  }

  switch (errno) {
  case ESRCH:
    args.fprintf(stderr, "[%d] I can find this thing...\n", pid);
    break;

  case EPERM:
    args.fprintf(stderr, "[%d] Nah, I have no right to kill this thing...\n",
                 pid);
    break;

  default:
    args.fprintf(stderr, "[%d] It's quite interesting about this thing: %s\n",
                 pid, strerror(errno));
    break;
  }

  return args.force;
}

bool process_kill(void) {
  for (int i = 0; i < args.pidsize; i++) {
    if (!process_kill_single(args.pids[i])) {
      return false;
    }
  }

  return true;
}

// MAIN

int main(int argc, char **argv) {
  program = argv[0];

  if (!args_parse(argc, argv)) {
    return 1;
  }

  sound_start();

  if (!process_kill()) {
    return 1;
  }

  sound_join();

  return 0;
}
