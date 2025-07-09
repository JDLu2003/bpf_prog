import re
import sys
import matplotlib.pyplot as plt

# 正则匹配 [PF] pid=xxx addr=0x...
PF_RE = re.compile(r'\[PF\] pid=(\d+) addr=0x([0-9a-fA-F]+)')

def main():
    if len(sys.argv) < 2:
        print(f"用法: python3 {sys.argv[0]} <logfile>")
        sys.exit(1)
    logfile = sys.argv[1]
    raw_times = []
    raw_addrs = []
    with open(logfile, 'r') as f:
        for line in f:
            m = PF_RE.search(line)
            if m:
                # 这里只用行号做伪时间
                raw_times.append(len(raw_times))
                addr = int(m.group(2), 16)
                raw_addrs.append(addr)
    if raw_times and raw_addrs:
        # 计算平均值
        avg_addr = sum(raw_addrs) / len(raw_addrs)
        # 只保留大于等于平均值的地址
        times = []
        addrs = []
        for t, a in zip(raw_times, raw_addrs):
            if a >= avg_addr:
                times.append(t)
                addrs.append(a)
        if times and addrs:
            plt.figure(figsize=(10, 5))
            plt.scatter(times, addrs, s=8, alpha=0.7)
            plt.xlabel('Event Index')
            plt.ylabel('Fault Address')
            plt.title('Page Fault Address Scatter (from log file, addr >= 平均值)')
            plt.grid(True)
            plt.tight_layout()
            plt.savefig('pf_scatter.png')
            print('图片已保存为 pf_scatter.png')
        else:
            print('所有 page fault 地址都小于平均值，未生成图片！')
    else:
        print('未在日志中发现 page fault 记录！')

if __name__ == '__main__':
    main() 