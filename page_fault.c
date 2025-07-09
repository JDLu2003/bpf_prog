#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>

struct event {
    __u32 pid;
    char comm[16];
    unsigned long address;
};

static volatile bool exiting = false;

static void handle_event(void *ctx, int cpu, void *data, __u32 data_sz)
{
    struct event *e = data;
    printf("Redis pid=%d comm=%s page fault at address: 0x%lx\n", e->pid, e->comm, e->address);
}

static void handle_lost_events(void *ctx, int cpu, __u64 lost_cnt)
{
    fprintf(stderr, "Lost %llu events on CPU %d\n", lost_cnt, cpu);
}

static void sig_handler(int sig)
{
    exiting = true;
}

int main(int argc, char **argv)
{
    struct bpf_object *obj;
    struct bpf_program *prog;
    int err;
    struct perf_buffer *pb = NULL;

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    obj = bpf_object__open_file("bpf_prog.o", NULL);
    if (libbpf_get_error(obj)) {
        fprintf(stderr, "Failed to open BPF object\n");
        return 1;
    }

    err = bpf_object__load(obj);
    if (err) {
        fprintf(stderr, "Failed to load BPF object\n");
        return 1;
    }

    prog = bpf_object__find_program_by_name(obj, "trace_page_fault_user");
    if (!prog) {
        fprintf(stderr, "Failed to find BPF program\n");
        return 1;
    }

    err = bpf_program__attach(prog);
    if (err) {
        fprintf(stderr, "Failed to attach BPF program\n");
        return 1;
    }

    int map_fd = bpf_object__find_map_fd_by_name(obj, "events");
    if (map_fd < 0) {
        fprintf(stderr, "Failed to find events map\n");
        return 1;
    }

    pb = perf_buffer__new(map_fd, 8, handle_event, handle_lost_events, NULL, NULL);
    if (libbpf_get_error(pb)) {
        fprintf(stderr, "Failed to create perf buffer\n");
        return 1;
    }

    printf("Start tracing redis page faults... Ctrl-C to stop.\n");

    while (!exiting) {
        err = perf_buffer__poll(pb, 100);
        if (err < 0 && err != -EINTR) {
            fprintf(stderr, "Error polling perf buffer: %d\n", err);
            break;
        }
    }

    perf_buffer__free(pb);
    bpf_object__close(obj);
    return 0;
}
