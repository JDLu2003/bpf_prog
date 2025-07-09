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

SEC("kprobe/page_cache_sync_ra")
int trace_page_cache_sync_ra(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "sync_ra";
    u32 flag = 1;
    bpf_map_update_elem(&my_map, &key, &flag, BPF_ANY);
    return 0;
}

SEC("kretprobe/page_cache_sync_ra")
int trace_page_cache_sync_ra_exit(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "sync_ra";
    u32 flag = 0;
    bpf_map_update_elem(&my_map, &key, &flag, BPF_ANY);
    return 0;
}

SEC("kprobe/page_cache_async_ra")
int trace_page_cache_async_ra(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "async_ra";
    u32 flag = 1;
    bpf_map_update_elem(&my_map, &key, &flag, BPF_ANY);
    return 0;
}

SEC("kretprobe/page_cache_async_ra")
int trace_page_cache_async_ra_exit(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "async_ra";
    u32 flag = 0;
    bpf_map_update_elem(&my_map, &key, &flag, BPF_ANY);
    return 0;
}

SEC("kprobe/add_to_page_cache_lru")
int trace_add_to_page_cache_lru(struct pt_regs *ctx) {
    if (!check_pid())
        return 0;
    stringkey key_sync = "sync_ra";
    u32 *v_sync = bpf_map_lookup_elem(&my_map, &key_sync);
    if (v_sync && *v_sync == 1) {
        stringkey key = "sync_accessed";
        u32 *v = bpf_map_lookup_elem(&my_map, &key);
        if (v) {
            (*v)++;
        }
    }
    stringkey key_async = "async_ra";
    u32 *v_async = bpf_map_lookup_elem(&my_map, &key_async);
    if (v_async && *v_async == 1) {
        stringkey key = "async_accessed";
        u32 *v = bpf_map_lookup_elem(&my_map, &key);
        if (v) {
            (*v)++;
        }
    }
    return 0;
}

SEC("tracepoint/exceptions/page_fault_user")
int handle_user_pf(struct trace_event_raw_page_fault* ctx) {
    if (!check_pid())
        return 0;
    stringkey key = "page_fault_user";
    u32 *v = bpf_map_lookup_elem(&my_map, &key);
    if (v)
        (*v)++;
    bpf_printk("[PF] pid=%d addr=0x%llx err=0x%x", bpf_get_current_pid_tgid() >> 32, ctx->address, ctx->error_code);
    return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
