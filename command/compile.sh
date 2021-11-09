####  此脚本应该在command目录下执行

gcc -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes \
   -Wsystem-headers -I ../lib -o prog_no_arg.o prog_no_arg.c
ld -e main prog_no_arg.o ../string.o ../syscall.o\
   ../stdio.o -o prog_no_arg.bin
dd if=prog_no_arg.bin of=/home/jerry/bochs/hd60M.img \
    bs=512 count=10 seek=300 conv=notrunc


nasm -f elf ./start.S -o ./start.o
ar rcs simple_crt.a ../string.o ../syscall.o \
   ../stdio.o ../debug.o ./start.o
gcc -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes \
   -Wsystem-headers -I ../lib/ -I ../lib/user -I ../fs prog_arg.c -o prog_arg.o
ld prog_arg.o simple_crt.a -o prog_arg.bin

dd if=prog_arg.bin of=/home/jerry/bochs/hd60M.img \
   bs=512 count=11 seek=360 conv=notrunc
