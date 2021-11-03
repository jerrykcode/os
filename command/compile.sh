####  此脚本应该在command目录下执行

gcc -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes \
   -Wsystem-headers -I ../lib -o prog_no_arg.o prog_no_arg.c
ld -e main prog_no_arg.o ../string.o ../syscall.o\
   ../stdio.o -o prog_no_arg.bin
dd if=prog_no_arg.bin of=/home/jerry/bochs/hd60M.img \
   bs=512 count=10 seek=300 conv=notrunc
