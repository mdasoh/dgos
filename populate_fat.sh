#!/bin/bash

IMG="$1"
TOPSRC="$2"

echo Populating FAT staging directory
mkdir -p stage || exit
"$TOPSRC/mkposixdirs.sh" stage || exit

truncate --size 128K stage/.big || exit

ln -f kernel-generic stage/dgos-kernel-generic || exit
ln -f kernel-tracing stage/dgos-kernel-tracing || exit
cp -u hello.km stage/hello.km || exit

cp -u user-shell stage || exit
cp -u "$TOPSRC/user/background.png" stage || exit

mkdir -p stage/EFI/boot || exit
cp -u bootx64.efi stage/EFI/boot/bootx64.efi || exit
#cp -u bootia32.efi stage/EFI/boot/bootia32.efi || exit

echo Populating FAT image
mcopy -s -Q -i "$IMG" stage/* ::/ || exit
mcopy -s -Q -i "$IMG" stage/.b* ::/ || exit
