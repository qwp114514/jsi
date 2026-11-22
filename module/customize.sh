#!/system/bin/sh
# 安装时执行
SKIPUNZIP=0

ui_print "*******************************"
ui_print "  暗区 ESP 视觉套件"
ui_print "  by 富江"
ui_print "*******************************"

# 检测架构
ARCH=$(getprop ro.product.cpu.abi)
if [ "$ARCH" != "arm64-v8a" ]; then
  ui_print "! 仅支持 arm64-v8a, 当前: $ARCH"
  abort "架构不匹配"
fi

ui_print "- 写入 zygisk 模块"
mkdir -p $MODPATH/zygisk
cp -f $MODPATH/zygisk/arm64-v8a.so $MODPATH/zygisk/arm64-v8a.so 2>/dev/null
set_perm_recursive $MODPATH 0 0 0755 0644
set_perm $MODPATH/post-fs-data.sh 0 0 0755
set_perm $MODPATH/service.sh      0 0 0755

ui_print "- 完成, 重启后生效"
ui_print "- 热键: 音量上+下 同时按"
