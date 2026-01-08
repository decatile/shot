#include "miniaudio.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Taken from
// https://github.com/tsoding/nob.h/blob/main/nob.h

#define da_reserve(da, expected_capacity)                                      \
  do {                                                                         \
    if ((expected_capacity) > (da)->capacity) {                                \
      if ((da)->capacity == 0) {                                               \
        (da)->capacity = 1;                                                    \
      }                                                                        \
      while ((expected_capacity) > (da)->capacity) {                           \
        (da)->capacity *= 2;                                                   \
      }                                                                        \
      (da)->items =                                                            \
          realloc((da)->items, (da)->capacity * sizeof(*(da)->items));         \
    }                                                                          \
  } while (0)

#define da_append(da, item)                                                    \
  do {                                                                         \
    da_reserve((da), (da)->count + 1);                                         \
    (da)->items[(da)->count++] = (item);                                       \
  } while (0)

extern const unsigned char _binary_sound_mp3_start[];
extern const unsigned char _binary_sound_mp3_end[];

const char *program;
pthread_t sound_thread;

struct {
  bool force;
  int signal;
  int (*fprintf)(FILE *restrict __stream, const char *restrict __format, ...);
  struct {
    int *items;
    int count;
    int capacity;
  } pids;
} args = {.force = 0, .signal = SIGKILL, .fprintf = fprintf, .pids = {0}};

int noop_fprintf(FILE *restrict __stream, const char *restrict __format, ...) {
  (void)__stream;
  (void)__format;

  return 0;
}

// PARSE COMMAND LINE

bool args_parse_options(int argc, char **argv) {
  char *endptr;

  for (;;) {
    switch (getopt(argc, argv, "fqs:h")) {
    case -1:
      return true;

    case 'f':
      args.force = 1;
      break;

    case 'q':
      args.fprintf = noop_fprintf;
      break;

    case 's':
      args.signal = strtol(optarg, &endptr, 10);
      if (*endptr != '\0') {
        fprintf(stderr, "%s: failed to parse signal\n", program);
        return false;
      }
      break;

    case 'h':
      printf("Usage:\n"
             " %s [options...] pids...\n"
             "\n"
             "Options:\n"
             " -h           display this help and exit\n"
             " -f           assume killing is always succeed\n"
             " -q           silence messages\n"
             " -s [SIGNAL]  send another signal instead of SIGKILL\n",
             program);
      return false;

    case '?':
      return false;
    }
  }
}

bool args_parse_pids_as_pipe(void) {
  char *buffer;
  size_t buflen;
  char *endptr;

  buffer = NULL;
  buflen = 0;

  while (getline(&buffer, &buflen, stdin) != EOF) {
    da_append(&args.pids, strtol(buffer, &endptr, 10));
    if (strchr(" \n\0", *endptr) == NULL) {
      args.fprintf(stderr, "%s: failed to parse PID\n", program);
      return false;
    }
  }

  return true;
}

bool args_parse_pids_as_args(char **array, int arraylen) {
  char *endptr;

  for (int i = 0; i < arraylen; i++) {
    da_append(&args.pids, strtol(array[i], &endptr, 10));
    if (*endptr != '\0') {
      args.fprintf(stderr, "%s: failed to parse PID\n", program);
      return false;
    }
  }

  return true;
}

bool args_parse_pids(char **array, int arraylen) {
  if (arraylen == 0) {
    return args_parse_pids_as_pipe();
  }

  return args_parse_pids_as_args(array, arraylen);
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
  if (kill(pid, args.signal) == 0) {
    return true;
  }

  switch (errno) {
  case ESRCH:
    args.fprintf(stderr, "[%d] I can't find this thing...\n", pid);
    break;

  case EPERM:
    args.fprintf(stderr, "[%d] Nah, I have no right to kill this thing...\n",
                 pid);
    break;

  case EINVAL:
    args.fprintf(stderr, "[%d] Invalid signal, I guess...\n", pid);
    break;

  default:
    args.fprintf(stderr, "[%d] %s\n", pid, strerror(errno));
    break;
  }

  return args.force;
}

bool process_kill(void) {
  for (int i = 0; i < args.pids.count; i++) {
    if (!process_kill_single(args.pids.items[i])) {
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
