TARGET = mbr.bin loader.bin kernel.bin

mbr.bin: boot/mbr.S
	nasm -I include/ -o $@ $<
loader.bin: boot/loader.S
	nasm -I include/ -o $@ $<

OBJS = main.o init.o interrupt.o print.o kernel.o

kernel.bin: $(OBJS)
	ld -Ttext 0xc0001500 -e main -o $@ $^

# asm
print.o: lib/kernel/print.S
	nasm -f elf -o $@ $<
kernel.o: kernel/kernel.S
	nasm -f elf -o $@ $<

# c
CFLAGS += -std=c99
INCLUDE = -I lib/kernel/ -I lib/ -I kernel/
main.o: kernel/main.c
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
init.o: kernel/init.c
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)
interrupt.o: kernel/interrupt.c 
	gcc $(INCLUDE) -c -o $@ $< $(CFLAGS)

clean: 
	rm *.o *.bin
