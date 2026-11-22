#!/system/bin/sh
# post-fs-data — 只做必要的设备权限，不留系统级痕迹
MODDIR=${0%/*}

# 触摸注入需要
chmod 666 /dev/uinput 2>/dev/null
chmod 666 /dev/input/event* 2>/dev/null

# 下面这几行原件里有，全部删掉了，原因写清楚免得又加回去：
#
#   echo 0 > /proc/sys/kernel/yama/ptrace_scope
#     → 全局降低内核安全阈值，getprop/sysctl 一读就知道被人动过，
#       而且这套根本不需要 ptrace（同进程直读内存）。
#
#   setprop ro.debuggable 0 / setprop service.adb.root 0
#     → ro.* 属性开机后是只读的，这行要么失败要么无效，
#       唯一效果是在属性服务日志里留一条「有人试图改 ro.debuggable」。
#
#   chmod 666 /dev/uhid
#     → 这套不用 uhid，白送一个可枚举的权限异常。
exit 0
