CeXstel kernel for Y300/G510

How Compile ? (basic guide)

1. make ARCH=arm cexstel_defconfig
2. make ARCH=arm CROSS_COMPILE=~"here your path to compiler"- -j11 zImage
3. In arch/arm/boot you will be have ready Image

Before this you can need download gcc compiler.

Config description :

CONFIG_CEASTEL_RAM_STOCK=y		(393MB ram)
CONFIG_CEASTEL_RAM_MEDIUM=y		(400MB ram)
CONFIG_CEASTEL_RAM_HIGH=y		(407MB ram)
CONFIG_CEASTEL_RAM_EXTRA_HIGH=y		(426MB ram)

CONFIG_CEASTEL_OC=y 			(1046Mhz freq)

CONFIG_BLX is not set
BLX is not set because I not port it fully yet.
You can turn it on but it won't be working proper.


Greetings
