#!/bin/sh

if [ -d /mnt/tmpmp3 ]; then
    rm -rf /mnt/tmpmp3
fi

if [ -d /tmp/mp3 ]; then
    rm -rf /tmp/mp3
fi

mkdir /mnt/tmpmp3
mkdir /tmp/mp3

cp ./orig.img /tmp/mp3/mp3.img
mount -o loop,offset=32256 /tmp/mp3/mp3.img /mnt/tmpmp3
cp -f ./bootimg /mnt/tmpmp3/
cp -f ./filesys_img.new /mnt/tmpmp3/filesys_img
umount /mnt/tmpmp3
cp -f /tmp/mp3/mp3.img ./
chown --reference=orig.img mp3.img 2>/dev/null

rm -rf /tmp/mp3
rm -rf /mnt/tmpmp3

