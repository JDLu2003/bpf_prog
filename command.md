# cgroup

## 常见的使用方法

把当前终端加入到这个组中

echo $$ | sudo tee /sys/fs/cgroup/unified/my_cgroup/cgroup.procs

移除

echo $$ | sudo tee /sys/fs/cgroup/cgroup.procs

## other

清除所有缓存

sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches
q
求文件的 md5 的值

md5sum

查看 bpf 的输出

cat /sys/kernel/debug/tracing/trace_pipe

查看内存使用

free -h

查看磁盘信息

df -h

## Linux 编译过程

复制/boot/config... 为 .config

make oldconfig

make -j$(nproc)          # 编译内核
make modules -j$(nproc)  # 编译模块
sudo make modules_install  # 安装模块
sudo make install         # 安装内核

## 查看 bpf  kprobe hook 点

sudo cat /proc/kallsyms | grep "函数名"


# BPF

## kfuncs

允许 bpf 程序直接调用的内核函数，通过__bpf_kfunc宏标记


