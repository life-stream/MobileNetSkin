# LTEService
有些LTE网卡，厂商提供的拨号程序与openwrt自带的拨号和网络管理无法兼容。LTEService是为这类网卡准备的网络管理外壳（?）程序，可以自行对网络状态做简单监控和尝试性的处理移动网络常见问题。

目标平台openwrt，如果其它平台使用则可能需要先编译安装UCI，从openwrt的源码中就可找到完整源码压缩档；以及需要自行处理权限问题，默认需要root权限才可运行，注意安全。

--------------------------------割--------------------------------

编译：
```
$ export CROSS_COMPILE=你的交叉编译器目录和前缀比如/home/.../openwrt/staging_dir/toolchain-aarch64_cortex-a53_gcc-8.4.0_musl/bin/aarch64-openwrt-linux-musl-
$ export STAGING_DIR=你的staging目录比如/home/.../openwrt/staging_dir
$ export TARGET=你的目标平台比如target-aarch64_cortex-a53_musl
$ make
```

运行前需要准备如下UCI配置文件放到你趁手的地方：
```
config ServiceArg 'conf'                    #请不要改动
	option enabled      	'1'             #请不要改动
	option ping_target  	'www.xxx...'    #用来测试网络是否通畅的网址或ip。
	option connect_exe  	'/usr/bin/xxx'  #网卡厂商提供的拨号程序，需要带完整路径。
	option wwan_dev     	'/dev/cdc-wdm0' #网卡猫设备文件，请参照用户手册确定文件名。
	option usb_dev      	'/dev/ttyUSB2'  #网卡模拟串口设备文件，请参照用户手册确定文件名。
	option ping_interval	'60'		#单位为秒。每隔这个时间做一次网络是否联通的检测。
```

运行：LTEService 配置文件 日志文件名 日志等级

文件名需要带完整路径，非openwrt请注意权限。配置文件默认为程序所在目录的LTEServiceConf，日志默认为程序目录的lte_status.log，等级0。

日志等级0~3，0最少，如果出现不稳定不妨试着逐步提高来摸摸情况。

example:
```
# LTEService /etc/config/LTEServiceConf /tmp/log/lte_status.log 0 &
```

------------------------------再割--------------------------------

一些碎碎念：

后续会完善一些细节。如果不忙可能还会增加其他的功能比如LUCI和短信收发。

这个全英文的云盘（?）既不是很会用，也不会经常来看。许可什么的更是完全看不懂。如果谁想搬运or二创，按ACG同人圈规矩来即可。我有尽量写注释了。

祝DIY路由器的朋友们一切顺利，希望将来有一天可以不再需要LTEService。

希望将来有一天大家都可以不用DIY路由器也能想打开哪个站就能打开哪个站。

2021/9/24
