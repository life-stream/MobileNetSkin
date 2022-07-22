
#export CROSS_COMPILE=/home/.../openwrt/staging_dir/toolchain-aarch64_cortex-a53_gcc-8.4.0_musl/bin/aarch64-openwrt-linux-musl-
#export STAGING_DIR=/home/.../openwrt/staging_dir
#export TARGET=target-aarch64_cortex-a53_musl

HPATH=$(STAGING_DIR)/$(TARGET)/usr/include
LPATH=$(STAGING_DIR)/$(TARGET)/usr/lib
CC=$(CROSS_COMPILE)g++ -std=c++17 

SRC=LTEModemIf.cpp main.cpp Log.cpp

release:
	$(CC) -Wall -s $(SRC)  -I $(HPATH)  -o mnets  -L $(LPATH) -lstdc++fs -luci -lubox


.PHONY: clean
clean:
	-rm -rf  mnets
