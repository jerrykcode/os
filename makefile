TARGET = mbr.bin loader.bin kernel.bin

mbr.bin: mbr.S
	nasm -I include/ -o $@ $<
loader.bin: loader.S
	nasm -I include/ -o $@ $<

OBJS = main.o print.o

kernel.bin: $(OBJS)
	ld -Ttext 0xc0001500 -e main -o $@ $^

print.o: lib/kernel/print.S
	nasm -f elf -o $@ $<

main.o: kernel/main.c
	gcc -I lib/kernel/ -c -o $@ $<

clean: 
	rm *.o *.bin
