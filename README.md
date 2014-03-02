CeXstel kernel for Y300/G510

How Compile ? (basic guide)

1. make ARCH=arm cexstel_defconfig
2. make ARCH=arm CROSS_COMPILE=~"here your path to compiler"- -j11 zImage
3. In arch/arm/boot you will be have ready Image

Before this you can need download gcc compiler.

Greetings
