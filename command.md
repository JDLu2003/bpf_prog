# cgroup

## 常见的使用方法

把当前终端加入到这个组中

echo $$ | sudo tee /sys/fs/cgroup/unified/my_cgroup/cgroup.procs

移除

echo $$ | sudo tee /sys/fs/cgroup/cgroup.procs

## other

清除所有缓存

sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'

求文件的 md5 的值

md5sum

查看 bpf 的输出

cat /sys/kernel/debug/tracing/trace_pipe
