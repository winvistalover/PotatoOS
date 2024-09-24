mkdir build
i686-elf-as arch/i686/boot.s -o build/boot.o
i686-elf-gcc -c arch/i686/kernel.c -o build/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -T linker.ld -o kernel.bin -ffreestanding -O2 -nostdlib build/boot.o build/kernel.o -lgcc
