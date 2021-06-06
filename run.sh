BOCHS=bochs
VDISK=/home/jerry/bochs/hd60M.img
BOCHSRC=bochsrc.disk

dd if=mbr.bin of=$VDISK bs=512 count=1 conv=notrunc
dd if=loader.bin of=$VDISK bs=512 count=4 seek=2 conv=notrunc
dd if=kernel.bin of=$VDISK bs=512 count=200 seek=9 conv=notrunc

$BOCHS -f $BOCHSRC
