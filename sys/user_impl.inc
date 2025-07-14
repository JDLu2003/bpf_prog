#include "user_interface.h"

static void user_on_open(const char *filename) {
  bpf_printk("User callback impl: we are opening %s now!\n", filename);
}


struct user_callbacks callbacks SEC(".bss") = {
  .on_open = user_on_open,
};
