#include "user_interface.h"

static void user_on_open(const char *filename) {
  bpf_printf("User callback impl: we are opening %s now!\n", filename);
}


const struct user_callbacks callbacks = {
  .on_open = user_on_open,
};
