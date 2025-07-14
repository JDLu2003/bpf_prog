#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

static void default_on_open(const char *filename) {
  bpf_printk("default on_open: %s\n", filename);
}

#endif

#ifndef on_open
#define on_open default_on_open

#endif
