#!/system/bin/sh
# late_start service — 原件这里是最吵的一段，全部去掉
MODDIR=${0%/*}

until [ "$(getprop sys.boot_completed)" = "1" ]; do sleep 2; done

# 原件做的事情以及为什么删：
#
#   pkill -9 -f frida-server / gdbserver / gdb ...
#     → 你在开机脚本里杀进程，被杀的名字会出现在 logcat 和内核审计里，
#       等于自己广播「这台设备上有人跑过注入器」。
#       而且真有人挂 frida 时，stealth.cpp 的 killAnalyzers() 已经处理了。
#
#   iptables -I INPUT --dport 27042 -j DROP
#     → iptables -L 一条命令就能看见这条规则，特征比 frida 本身还明显。
#
#   echo "aux service up $(date)" >> /data/local/tmp/aux.log
#     → 明文日志文件 = 直接证据。要日志用 logcat，别落盘。
#
# 结论：这个脚本现在什么都不做，保留只是为了模块结构完整。
exit 0
