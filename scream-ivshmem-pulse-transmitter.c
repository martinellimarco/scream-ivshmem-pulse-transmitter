// Scream-IVSHMEM transmitter for Pulseaudio.
// Author: Marco Martinelli - https://github.com/martinellimarco
/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <signal.h>

struct shmheader {
  uint32_t magic;
  uint16_t write_idx;
  uint8_t  offset;
  uint16_t max_chunks;
  uint32_t chunk_size;
  uint8_t  sample_rate;
  uint8_t  sample_size;
  uint8_t  channels;
  uint16_t channel_map;
};

struct shmheader *header = NULL;

void intHandler(int dummy) {
  if(header){
    memset(header, 0, sizeof(struct shmheader));
  }
  exit(0);
}

static void show_usage(const char *arg0) {
  fprintf(stderr, "\n");
  fprintf(stderr, "Usage: %s <ivshmem device path> [-r <sample rate>] [-s <sample size>] [-c <channels>]\n", arg0);
  fprintf(stderr, "\n");
  fprintf(stderr, "         All command line options are optional except for IVSHMEM device path.\n");
  fprintf(stderr, "         Default is to use 44100Hz, 16bit per sample and 2 channels.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "         -r <sample rate> : Any multiple of 44100Hz or 48000Hz up to 192000Hz.\n");
  fprintf(stderr, "         -s <sample size> : 16, 24 or 32 bits per sample.\n");
  fprintf(stderr, "         -c <channels>    : From 1 (mono) to 8 (7.1 surround).\n");
  fprintf(stderr, "\n");
  exit(1);
}

static void * open_mmap(const char *shmfile, int* ivshmem_size) {
  struct stat st;
  if (stat(shmfile, &st) < 0)  {
    fprintf(stderr, "Failed to stat the shared memory file: %s\n", shmfile);
    exit(2);
  }

  int shmFD = open(shmfile, O_RDWR, (mode_t)0600);
  if (shmFD < 0) {
    fprintf(stderr, "Failed to open the shared memory file: %s\n", shmfile);
    exit(3);
  }

  void * map = mmap(0, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFD, 0);
  if (map == MAP_FAILED) {
    fprintf(stderr, "Failed to map the shared memory file: %s\n", shmfile);
    close(shmFD);
    exit(4);
  }
  
  *ivshmem_size = st.st_size;

  return map;
}

int main(int argc, char*argv[]) {
  signal(SIGINT, intHandler);

  if (argc < 2) {
    show_usage(argv[0]);
  }
  
  int ivshmem_size;
  unsigned char * mmap = open_mmap(argv[1], &ivshmem_size);
  
  uint32_t sample_rate = 44100;
  uint32_t sample_size = 16;
  uint32_t pulse_format = PA_SAMPLE_S16LE;
  uint32_t channels = 2;

  int opt;
  while ((opt = getopt(argc, argv, "r:s:c:")) != -1) {
    switch (opt) {
    case 'r':
      sample_rate = atoi(optarg);
      if (!(sample_rate <= 192000 && (sample_rate%44100 == 0 || sample_rate%48000 == 0))){
        show_usage(argv[0]);
      }
      break;
    case 's':
      sample_size = atoi(optarg);
      switch(sample_size){
        case 16:
          pulse_format = PA_SAMPLE_S16LE;
          break;
        case 24:
          pulse_format = PA_SAMPLE_S24LE;
          break;
        case 32:
          pulse_format = PA_SAMPLE_S32LE;
          break;
        default:
          show_usage(argv[0]);
      }
      break;
    case 'c':
      channels = atoi(optarg);
      if (!(channels >= 1 && channels <= 8)){
        show_usage(argv[0]);
      }
      break;
    default:
      show_usage(argv[0]);
    }
  }
  if (1+optind < argc) {
    fprintf(stderr, "Expected argument after options\n");
    show_usage(argv[0]);
  }
  
  /* The sample type to use */
  pa_sample_spec ss = {
    .format = pulse_format,
    .rate = sample_rate,
    .channels = channels
  };
  pa_simple *s = NULL;
  int ret = 1;
  int error;
  
  header = (struct shmheader*)mmap;
  memset(header, 0, sizeof(struct shmheader));
  
  uint32_t ivshmem_offset = sizeof(struct shmheader);
  
  uint32_t chunk_size = (sample_size>>3)*channels*sample_rate/50;
  
  uint32_t write_idx = 0;
  uint32_t max_chunks = (uint16_t)(ivshmem_size / chunk_size);
  
  header->write_idx = write_idx;
  header->offset = ivshmem_offset;
  header->chunk_size = chunk_size;
  header->max_chunks = max_chunks;
  header->sample_rate = (uint8_t)((sample_rate % 44100) ? (0 + (sample_rate / 48000)) : (128 + (sample_rate / 44100)));;
  header->sample_size = (uint8_t)sample_size;
  header->channels = (uint8_t)channels;
  header->channel_map = (uint16_t)0xFFFF;//FIXME at the moment this doesn't support channel mapping other than the default one, if it's needed open a issue and I'll work on it

  header->magic = 0x11112014;

  /* Create the recording stream */
  if (!(s = pa_simple_new(NULL, "Pulseaudio IVSHMEM Transmitter (Scream)", PA_STREAM_RECORD, NULL, "record", &ss, NULL, NULL, &error))) {
    fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
    goto finish;
  }

  fprintf(stderr, "Streaming to IVSHMEM\n");

  for (;;) {
    if (++write_idx >= max_chunks) {
      write_idx = 0;
    }

    uint8_t *buf = &mmap[header->offset+header->chunk_size*write_idx];

    if (pa_simple_read(s, buf, chunk_size, &error) < 0) {
      fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
      goto finish;
    }

    header->write_idx = write_idx;
  }
  ret = 0;
finish:
  memset(header, 0, sizeof(struct shmheader));
  if (s)
    pa_simple_free(s);
  return ret;
} 
