这里是LTEService。因为某些不知道出处的规范，LTEService需要改变名字和操作方式。

为了计划中的一些扩展，配置文件的一些名字变更了，请重新编辑配置文件。

--------------------------------割--------------------------------

有些移动网络网卡，厂商提供的拨号程序与openwrt自带的拨号和网络管理无法兼容。MNetS是为这类网卡准备的网络管理外壳（?）程序，可以自行对网络状态做简单监控和尝试性的处理移动网络常见问题。

如果你的操作系统不是openwrt，则可能需要先编译安装UCI库；以及需要自行处理权限问题，默认需要root权限才可运行，注意安全。

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
config ConnArg 'conn'
	option connect_exe  	'/usr/bin/xxx'  #网卡厂商提供的拨号程序，需要带完整路径。
	option wwan_dev     	'/dev/cdc-wdm0' #网卡猫设备文件，请参照用户手册确定文件名。
	option tty_dev      	'/dev/ttyUSB2'  #网卡模拟串口设备文件，请参照用户手册确定文件名。
	option ping_target  	'www.xxx...'    #用来测试网络是否通畅的网址或ip。 
	option ping_interval	'60'		#单位为秒。每隔这个时间做一次网络是否联通的检测。
```

运行：mnets -r 日志文件名 -n 日志等级 配置文件名

非openwrt请注意权限。配置文件默认为/etc/config/mnets。

日志可以没有，但这样一旦出错将无法留下任何信息。日志等级0~3，0最少，如果出现不稳定不妨试着逐步提高来摸摸情况。

记得在命令行最后加个&，你懂的。

example:
```
# ./mnets -r lte.log -n 2 myconfig &
```

--------------------------碎碎念的割----------------------------

曾经有一个用户向我提到，LTEService的工作方式让他想起“苹果皮”……一种很久以前与ipod touch一起工作的通信设备……所以给LTEService选择了现在的名字，简称MNetS。

我现在在试着将它变成一个可以用opkg来管理的软件包，但这方面好像几乎没有什么资料，所以不知道要摸索多久。

2022/7/23

后续会完善一些细节。如果不忙可能还会增加其他的功能比如LUCI和短信收发。

这个全英文的云盘（?）既不是很会用，也不会经常来看。许可什么的更是完全看不懂。如果谁想搬运or二创，按ACG同人圈规矩来即可。我有尽量写注释了。

祝DIY路由器的朋友们一切顺利，希望将来有一天可以不再需要MNetS。

希望将来有一天大家都可以不用DIY路由器也能想打开哪个站就能打开哪个站。

2021/9/24
