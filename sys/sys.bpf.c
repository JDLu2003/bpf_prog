#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "user_impl.inc"
#include "user_interface.h"

char LICENSE[] SEC("license") = "GPL";

SEC("kprobe/do_sys_openat2")
int handle_open(struct pt_regs *ctx) {
  int dfd = PT_REGS_PARM1(ctx);
  const char *filename = (const char *)PT_REGS_PARM2(ctx);
  struct open_how *how = (struct open_how *)PT_REGS_PARM3(ctx);

  char fname[256];
  bpf_probe_read_user_str(fname, sizeof(fname), filename);

  if (callbacks.on_open)
    callbacks.on_open(fname);

  return 0;
}
