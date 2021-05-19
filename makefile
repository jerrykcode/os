BOCHS = bochs
VDISK = /home/jerry/bochs/hd60M.img
BOCHSRC = bochsrc.disk

INCLUDE = -I include/

run : ${VDISK}
	${BOCHS} -f ${BOCHSRC}

${VDISK} : mbr.bin loader.bin
	dd if=mbr.bin of=${VDISK} bs=512 count=1 conv=notrunc
	dd if=loader.bin of=${VDISK} bs=512 count=4 seek=2 conv=notrunc

%.bin : %.S
	nasm ${INCLUDE} -o $@ $<

clean : 
	rm *.bin
