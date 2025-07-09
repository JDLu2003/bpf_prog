#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

typedef __u32 u32;
typedef char stringkey[64];

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 128);
    stringkey *key;
    __type(value, u32);
} my_map SEC(".maps");

static __always_inline int check_pid() {
    stringkey key = "pid";
    u32 mypid = bpf_get_current_pid_tgid();
    u32 *val = bpf_map_lookup_elem(&my_map, &key);
    if (val && *val == mypid)
        return 1;
    return 0;
}

SEC("ksyscall/read")
int trace_read(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "read";
    u32 *v = bpf_map_lookup_elem(&my_map, &key);
    if (v)
        (*v)++;
    return 0;
}

SEC("ksyscall/write")
int trace_write(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "write";
    u32 *v = bpf_map_lookup_elem(&my_map, &key);
    if (v)
        (*v)++;
    return 0;
}

SEC("ksyscall/mmap")
int trace_mmap(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "mmap";
    u32 *v = bpf_map_lookup_elem(&my_map, &key);
    if (v)
        (*v)++;
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
