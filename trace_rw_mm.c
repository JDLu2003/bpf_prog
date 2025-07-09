#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include "trace_rw_mm.skel.h"

typedef char stringkey[64];
typedef __u32 u32;

int main(int argc, char **argv) {
    struct trace_rw_mm_bpf *skel;
    int err;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <target-pid>\n", argv[0]);
        return 1;
    }

    u32 pid = atoi(argv[1]);

    libbpf_set_strict_mode(LIBBPF_STRICT_ALL);
    skel = trace_rw_mm_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    err = trace_rw_mm_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    err = trace_rw_mm_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
        goto cleanup;
    }

    printf("Tracking PID %d...\n", pid);

    stringkey keys[4] = {"pid", "read", "write", "mmap"};
    u32 zero = 0;

    bpf_map__update_elem(skel->maps.my_map, &keys[0], sizeof(keys[0]), &pid, sizeof(pid), BPF_ANY);
    for (int i = 1; i < 4; i++) {
        bpf_map__update_elem(skel->maps.my_map, &keys[i], sizeof(keys[i]), &zero, sizeof(zero), BPF_ANY);
    }

    // Sleep and periodically print stats
    for (;;) {
        sleep(1);

        u32 val;
        printf("===== %ds =====\n", i + 1);
        for (int j = 1; j < 4; j++) {
            if (bpf_map__lookup_elem(skel->maps.my_map, &keys[j], sizeof(keys[j]), &val, sizeof(val), BPF_ANY) == 0) {
                printf("%s count = %u\n", keys[j], val);
            }
        }
        sleep(1);
    }

cleanup:
    trace_rw_mm_bpf__destroy(skel);
    return err < 0 ? -err : err;
}
