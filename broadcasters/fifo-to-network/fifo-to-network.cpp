#include "cmdline.h"

struct gengetopt_args_info args;

int main(int argc, char ** argv) {
  cmdline_parser(argc, argv, &args);
  printf("srate = %f\n", args.srate_arg);
  return 0;
}
