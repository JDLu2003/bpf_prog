#ifndef USER_INTERFACE_H
#define USER_INTERFACE_H

struct user_callbacks {
  void (*on_open)(const char *filename);
};

__attribute__((weak)) void default_on_open(const char *filename) {
  bpf_printk("default on_open: %s\n", filename);
}

#define on_open default_on_open

#endif
