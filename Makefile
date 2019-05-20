obj-m += tcp_mario.o
BUILD_PATH=/lib/modules/$(shell uname -r)/build
all:
	make -C  $(BUILD_PATH) M=$(PWD) modules

clean:
	make -C $(BUILD_PATH) M=$(PWD) clean
