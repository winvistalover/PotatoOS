echo Create build directory...
echo ^^ mkdir buld
mkdir build

echo Assumble boot.s...
echo  ^^ i686-elf-as boot.s -o build/boot.o
i686-elf-as boot.s -o build/boot.o

echo Compile kernel.c...
echo  ^^ i686-elf-gcc -c kernel.c -o build/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra
i686-elf-gcc -c kernel.c -o build/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

echo Link kernel...
echo  ^^ i686-elf-gcc -T linker.ld -o pos.bin -ffreestanding -O2 -nostdlib build/boot.o build/kernel.o -lgcc
i686-elf-gcc -T linker.ld -o pos.bin -ffreestanding -O2 -nostdlib build/boot.o build/kernel.o -lgcc

echo Cleaning up...
echo  ^^ rm -rf build
rm -rf build

qemu-system-i386 -kernel pos.bin
