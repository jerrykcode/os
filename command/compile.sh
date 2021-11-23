####  此脚本应该在command目录下执行

nasm -f elf ./start.S -o ./start.o
#ar rcs simple_crt.a ../string.o ../syscall.o \
#   ../stdio.o ../debug.o ./start.o
gcc -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes \
   -Wsystem-headers -fno-stack-protector -I ../lib/ -I ../lib/user -I ../fs -I ../usrprog cat.c -o cat.o
#ld prog_arg.o simple_crt.a -o prog_arg.bin
ld -e _start cat.o start.o ../string.o ../syscall.o ../stdio.o -o cat.bin

dd if=cat.bin of=/home/jerry/bochs/hd60M.img \
   bs=512 count=14 seek=300 conv=notrunc
