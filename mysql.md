
启动 mysql 服务器

/usr/local/mysql/support-files/mysql.server start

进入 mysql 客户端

mysql -u root -p


sudo mysqld --initialize --user=mysql


sudo bpftrace -e '
  tracepoint:syscalls:sys_enter_p*read*64,
  tracepoint:syscalls:sys_enter_p*write*64
  /comm == "mysqld"/
  {
    printf("PID:%-6d FD:%-3d OFFSET:%-10d SIZE:%d\n", pid, args->fd, args->offset, args->count);
  }
'