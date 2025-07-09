import re
import time
import matplotlib.pyplot as plt
from datetime import datetime

# 可选：trace_pipe 路径
TRACE_PIPE = '/sys/kernel/debug/tracing/trace_pipe'

# 正则匹配 [PF] pid=xxx addr=0x...
PF_RE = re.compile(r'\[PF\] pid=(\d+) addr=0x([0-9a-fA-F]+)')

times = []
addrs = []
start_time = None

print('请用 root 权限运行，并确保 trace_pipe 有 page fault 日志...')

try:
    with open(TRACE_PIPE, 'r') as f:
        while True:
            line = f.readline()
            if not line:
                time.sleep(0.01)
                continue
            m = PF_RE.search(line)
            if m:
                now = time.time()
                if start_time is None:
                    start_time = now
                rel_time = now - start_time
                addr = int(m.group(2), 16)
                times.append(rel_time)
                addrs.append(addr)
                print(f"t={rel_time:.3f}s, addr=0x{addr:x}")
except KeyboardInterrupt:
    print('采集结束，开始绘图...')

if times and addrs:
    plt.figure(figsize=(10, 5))
    plt.scatter(times, addrs, s=8, alpha=0.7)
    plt.xlabel('Time (s)')
    plt.ylabel('Fault Address')
    plt.title('Page Fault Address vs. Time')
    plt.grid(True)
    plt.tight_layout()
    plt.show()
else:
    print('未采集到 page fault 日志！') 