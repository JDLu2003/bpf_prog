#include "sys.skel.h"
#include <bpf/libbpf.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef char stringkey[64];
typedef __u32 u32;

int main(int argc, char **argv) {

  // clear all cache
  int ret = system("sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");

  if (ret == -1) {
    fprintf(stderr, "Failed to clear all cache\n");
  } else {
    fprintf(stderr, "Succeessfil cleared all caches\n");
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <command> [args ...]\n", argv[0]);
    return 1;
  }

  struct sys_bpf *skel;
  int err;
  skel = sys_bpf__open();
  if (!skel) {
    fprintf(stderr, "Failed to open BPF skeleton\n");
    return 1;
  }
  err = sys_bpf__load(skel);
  if (err) {
    fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
    goto cleanup;
  }
  err = sys_bpf__attach(skel);
  if (err) {
    fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
    goto cleanup;
  }

  pid_t child = fork();
  if (child < 0) {
    perror("fork");
    goto cleanup;
  }

  if (child == 0) {
    // 子进程：执行测试命令
    execvp(argv[1], &argv[1]);
    perror("execvp");
    exit(127);
  }

  // 父进程：将子进程 pid 写入 bpf map
  stringkey keys[1] = {"pid"};
  bpf_map__update_elem(skel->maps.sys_map, &keys[0], sizeof(keys[0]), &child,
                       sizeof(child), BPF_ANY);

  printf("Running '%s' (pid=%d), monitoring...\n", argv[1], child);
  printf("check bpf output in '/sys/kernel/debug/tracing/trace_pipe'\n");
  int status;
  while (1) {
    // 检查子进程是否结束
    pid_t w = waitpid(child, &status, 0);
    if (w == child) {
      printf("Child process exited.\n");
      break;
    }
    sleep(1);
    printf(".");
  }

cleanup:
  sys_bpf__destroy(skel);
  return 0;
}
