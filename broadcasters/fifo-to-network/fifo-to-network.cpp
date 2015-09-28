#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include "cmdline.h"

struct gengetopt_args_info args;

struct packet_header_t {
  uint64_t audible_second;
  uint32_t audible_nanosecond;
  float sampling_rate;
  int32_t num_frames;
  int16_t num_channels;
  bool continuous;
};

int32_t pipe_size(int32_t num_channels, int32_t period_size) {
  static const int32_t page_size = 4096;
  int32_t s = num_channels * period_size * 2;
  s = (s / page_size + 1) * s;
  return s;
}
  

int main(int argc, char ** argv) {
  cmdline_parser(argc, argv, &args);
  std::vector<int16_t> data(args.period_arg * args.channels_arg, int16_t(0));
  struct timespec reception_time = {0,0};
  
  int fd = open(args.fifo_arg, O_RDONLY);
  int e = fcntl(fd, F_SETPIPE_SZ, pipe_size(args.channels_arg, args.period_arg));
  if (e < 0) {
    perror("could not set pipe size");
    exit(EXIT_FAILURE);
  }

  printf("starting the read loop\n");
  const ssize_t block_size = args.channels_arg * args.period_arg * 2;
  for(;;) {
    ssize_t bytes_read = 0;
    char * buf = reinterpret_cast<char*>(&data[0]);
    while (bytes_read < block_size) {
      ssize_t bytes_read_now = read(fd, buf, block_size - bytes_read);
      if (bytes_read_now < 0) {
        perror("reading from fifo");
        exit(EXIT_FAILURE);
      }
      bytes_read += bytes_read_now;
    }
    printf(".");
    fflush(stdout);
  }
  
  return 0;
}
