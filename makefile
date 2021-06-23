TARGET = mbr.bin loader.bin kernel.bin

mbr.bin: boot/mbr.S
	nasm -I include/ -o $@ $<
loader.bin: boot/loader.S
	nasm -I include/ -o $@ $<

OBJS = main.o debug.o init.o timer.o interrupt.o memory.o bitmap.o string.o print.o kernel.o

kernel.bin: $(OBJS)
	ld -Ttext 0xc0001500 -e main -o $@ $^

# asm
print.o: lib/kernel/print.S
	nasm -f elf -o $@ $<
kernel.o: kernel/kernel.S
	nasm -f elf -o $@ $<

# c
CFLAGS += -std=c99
INCLUDE = -I lib/kernel/ -I lib/ -I kernel/ -I device/
main.o: kernel/main.c
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
init.o: kernel/init.c kernel/init.h kernel/memory.h kernel/interrupt.h device/timer.h lib/kernel/print.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
interrupt.o: kernel/interrupt.c kernel/interrupt.h kernel/global.h \
	lib/stdint.h lib/kernel/print.h  lib/kernel/io.h lib/kernel/asm.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
timer.o: device/timer.c device/timer.h lib/stdint.h lib/kernel/io.h lib/kernel/print.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
debug.o: kernel/debug.c kernel/debug.h kernel/interrupt.h lib/kernel/print.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
memory.o: kernel/memory.c kernel/memory.h lib/string.h lib/stdint.h lib/kernel/bitmap.h lib/kernel/print.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h lib/stdint.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
string.o: lib/string.c lib/string.h lib/stdint.h lib/stddef.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)

clean: 
	rm *.o *.bin
