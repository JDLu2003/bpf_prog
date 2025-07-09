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
    times = []
    addrs = []
    start_time = None
    with open(logfile, 'r') as f:
        for line in f:
            m = PF_RE.search(line)
            if m:
                # 假设日志每行前面有时间戳（如 12.345: ...），可自行扩展解析
                # 这里只用行号做伪时间
                times.append(len(times))
                addr = int(m.group(2), 16)
                addrs.append(addr)
    if times and addrs:
        plt.figure(figsize=(10, 5))
        plt.scatter(times, addrs, s=8, alpha=0.7)
        plt.xlabel('Event Index')
        plt.ylabel('Fault Address')
        plt.title('Page Fault Address Scatter (from log file)')
        plt.grid(True)
        plt.tight_layout()
        plt.savefig('pf_scatter.png')
        print('图片已保存为 pf_scatter.png')
    else:
        print('未在日志中发现 page fault 记录！')

if __name__ == '__main__':
    main() 