#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/version.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

struct event {
    __u32 pid;
    char comm[16];
    unsigned long address;
};

struct {
    __uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
} events SEC(".maps");

SEC("tracepoint/exceptions/page_fault_user")
int trace_page_fault_user(struct trace_event_raw_page_fault* ctx)
{
    __u32 pid = bpf_get_current_pid_tgid() >> 32;
    char comm[16];
    bpf_get_current_comm(&comm, sizeof(comm));

    // 只过滤 redis-server 进程
    if (comm[0] != 'r' || comm[1] != 'e' || comm[2] != 'd' || comm[3] != 'i' || comm[4] != 's' || comm[5] != '-' || comm[6] != 's') {
        return 0;
    }

    struct event ev = {};
    ev.pid = pid;
    __builtin_memcpy(ev.comm, comm, sizeof(comm));
    ev.address = ctx->address;

    bpf_perf_event_output(ctx, &events, BPF_F_CURRENT_CPU, &ev, sizeof(ev));
    return 0;
}
