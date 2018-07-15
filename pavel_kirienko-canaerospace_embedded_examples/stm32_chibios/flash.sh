#!/bin/sh

bin=$(ls -1 build/*.bin)
if [ "$bin" == "" ]; then
    echo "No image found"
    exit 1
fi

elf="${bin%.*}.elf"
size $elf || exit 1

st-flash write $bin 8000000
