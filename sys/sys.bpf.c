#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

#include "user_impl.inc"
#include "user_interface.h"

typedef __u32 u32;
typedef char stringkey[64];

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __uint(max_entries, 128);
  stringkey *key;
  __type(value, u32);
} sys_map SEC(".maps");

static __always_inline int check_pid() {
  stringkey key = "pid";
  u32 mypid = bpf_get_current_pid_tgid();
  u32 *val = bpf_map_lookup_elem(&sys_map, &key);
  if (val && *val == mypid)
    return 1;
  return 0;
}
#define CHECK_PID()                                                            \
  do {                                                                         \
    if (check_pid() == 0) {                                                    \
      return 0;                                                                \
    }                                                                          \
  } while (0)

char LICENSE[] SEC("license") = "GPL";

SEC("kprobe/do_sys_openat2")
int handle_open(struct pt_regs *ctx) {

  CHECK_PID();
  int dfd = PT_REGS_PARM1(ctx);
  const char *filename = (const char *)PT_REGS_PARM2(ctx);
  struct open_how *how = (struct open_how *)PT_REGS_PARM3(ctx);

  char fname[256];
  bpf_probe_read_user_str(fname, sizeof(fname), filename);

  on_open(fname);
  return 0;
}
