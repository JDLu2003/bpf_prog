#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

struct user_callbacks {
  void (*on_open)(const char *filename);
}

__attrbute__((weak)) void default_on_open(const char *filename) {
  bpf_printk("default on_open: %s\n", filename);
}

extern const struct user_callbacks callbacks;

#endif
