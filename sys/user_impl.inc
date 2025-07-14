#include "user_interface.h"

static void user_on_open(const char *filename) {
  bpf_printk("User callback impl: we are opening %s now!\n", filename);
}

#define on_open user_on_open
