#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "user_impl.inc"
#include "user_interface.h"

char LICENSE[] SEC("license") = "GPL";

SEC("kprobe/do_sys_openat2")
int BPF_KPROBE(handle_open, int dfd, const char __user *filename,
               struct open_how *how) {
  char fname[256];
  bpf_probe_read_user_str(fname, sizeof(fname), filename);

  if (callbacks.on_open)
    callbacks.on_open(fname);

  return 0;
}
