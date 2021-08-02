TARGET = mbr.bin loader.bin kernel.bin

mbr.bin: boot/mbr.S
	nasm -I include/ -o $@ $<
loader.bin: boot/loader.S
	nasm -I include/ -o $@ $<

OBJS = main.o debug.o init.o timer.o syscall-init.o stdio.o syscall.o interrupt.o thread.o process.o \
	memory.o console.o keyboard.o ioqueue.o sync.o bitmap.o tss.o string.o list.o print.o kernel.o \
	switch.o

kernel.bin: $(OBJS)
	ld -Ttext 0xc0001500 -e main -o $@ $^

# asm
print.o: lib/kernel/print.S
	nasm -f elf -o $@ $<
kernel.o: kernel/kernel.S
	nasm -f elf -o $@ $<
switch.o: thread/switch.S
	nasm -f elf -o $@ $<

# c
CFLAGS += -std=c99
INCLUDE = -I lib/kernel/ -I lib/usr -I lib/ -I kernel/ -I device/ -I thread/ -I usrprog/
main.o: kernel/main.c lib/kernel/print.h kernel/init.h kernel/memory.h lib/kernel/asm.h thread/thread.h kernel/debug.h \
	device/keyboard.h device/ioqueue.h usrprog/process.h usrprog/syscall-init.h lib/usr/syscall.h lib/stdio.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
init.o: kernel/init.c kernel/init.h kernel/memory.h kernel/interrupt.h device/timer.h lib/kernel/print.h \
	device/console.h device/keyboard.h usrprog/tss.h usrprog/syscall-init.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
interrupt.o: kernel/interrupt.c kernel/interrupt.h kernel/global.h \
	lib/stdint.h lib/kernel/print.h  lib/kernel/io.h lib/kernel/asm.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
timer.o: device/timer.c device/timer.h lib/stdint.h lib/kernel/io.h lib/kernel/print.h \
	thread/thread.h kernel/interrupt.h kernel/debug.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
debug.o: kernel/debug.c kernel/debug.h kernel/interrupt.h lib/kernel/print.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
memory.o: kernel/memory.c kernel/memory.h lib/string.h lib/stdint.h lib/kernel/bitmap.h lib/kernel/print.h lib/kernel/list.h \
	thread/thread.h kernel/global.h lib/kernel/asm.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h lib/stdint.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
string.o: lib/string.c lib/string.h lib/stdint.h lib/stddef.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
list.o: lib/kernel/list.c lib/kernel/list.h lib/stdbool.h lib/stddef.h kernel/debug.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
thread.o: thread/thread.c thread/thread.h lib/kernel/list.h lib/kernel/print.h usrprog/process.h thread/sync.h \
	lib/kernel/asm.h lib/kernel/bitmap.h lib/stddef.h lib/string.h kernel/memory.h kernel/interrupt.h kernel/debug.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
sync.o: thread/sync.c thread/sync.h thread/thread.h lib/stdint.h lib/stddef.h lib/kernel/list.h \
	kernel/interrupt.h kernel/debug.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
console.o: device/console.c device/console.h lib/stdint.h lib/kernel/print.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
keyboard.o: device/keyboard.c device/keyboard.h kernel/interrupt.h kernel/global.h lib/kernel/print.h lib/kernel/io.h \
	lib/stdbool.h device/ioqueue.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
ioqueue.o: device/ioqueue.c device/ioqueue.h lib/stdint.h lib/stddef.h lib/stdbool.h thread/sync.h thread/thread.h \
	kernel/interrupt.h lib/kernel/list.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
tss.o: usrprog/tss.c usrprog/tss.h kernel/memory.h kernel/global.h lib/stdint.h lib/kernel/print.h lib/kernel/asm.h thread/thread.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
process.o: usrprog/process.c usrprog/process.h kernel/memory.h kernel/global.h lib/stdint.h lib/kernel/asm.h thread/thread.h \
	lib/kernel/list.h kernel/interrupt.h kernel/debug.h lib/kernel/bitmap.h lib/stddef.h lib/string.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
syscall.o: lib/usr/syscall.c lib/usr/syscall.h lib/stdint.h lib/kernel/asm.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
syscall-init.o: usrprog/syscall-init.c usrprog/syscall-init.h lib/usr/syscall.h lib/stdint.h lib/kernel/print.h thread/thread.h \
	device/console.h lib/string.h kernel/memory.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
stdio.o: lib/stdio.c lib/stdio.h lib/stddef.h lib/usr/syscall.h lib/string.h
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)

clean: 
	rm *.o *.bin
