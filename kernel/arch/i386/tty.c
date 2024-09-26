#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include <kernel/tty.h>

#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

static inline char* osname = "PotatoOS";
static inline char* version = "0";
static inline char* subversion = "2.4";

static inline char* username = "potato";
static inline char* hostname = "live";


size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}

char input_buffer[256];
size_t input_buffer_index = 0;

void terminal_initialize(void) 
{
	terminal_row = 1;
	terminal_column = 2;
	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color) 
{
	terminal_color = color;
}

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a" (value), "Nd" (port));
}


void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_putchar(char c) 
{
	if (terminal_row == VGA_HEIGHT - 1) {
        	terminal_scroll();
    	}
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_row = 0;
	}
}

void terminal_write(const char* data, size_t size) 
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_scroll() {
    for (int i = 0; i < VGA_HEIGHT - 1; i++) {
        for (int j = 0; j < VGA_WIDTH; j++) {
            terminal_buffer[i * VGA_WIDTH + j] = terminal_buffer[(i + 1) * VGA_WIDTH + j];
        }
    }
    for (int j = 0; j < VGA_WIDTH; j++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + j] = vga_entry(' ', terminal_color);
    }
    terminal_column = 2;
}


void terminal_writestring(const char* data) 
{
	terminal_write(data, strlen(data));
}

void terminal_newline(void)
{
	terminal_row += 1;
	terminal_column = 2;
	if (terminal_row == VGA_HEIGHT - 2) {
		terminal_buffer = (uint16_t*) 0xB8000;
		for (size_t y = 0; y < VGA_HEIGHT; y++) {
			for (size_t x = 0; x < VGA_WIDTH; x++) {
				const size_t index = y * VGA_WIDTH + x;
				terminal_buffer[index] = vga_entry(' ', terminal_color);
			}
		}
		terminal_row = 1;
		terminal_column = 2;
	}
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile ("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile ("outw %0, %1" : : "a" (value), "Nd" (port));
}


char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++));
    return original_dest;
}

int strcmp(const char* str1, const char* str2) {
    while (*str1 && (*str1 == *str2)) {
        str1++;
        str2++;
    }
    return *(unsigned char*)str1 - *(unsigned char*)str2;
}


char* strncpy(char* dest, const char* src, size_t n) {
    char* original_dest = dest;
    while (n && (*dest++ = *src++)) {
        n--;
    }
    while (n--) {
        *dest++ = '\0'; // Null-terminate the remaining space
    }
    return original_dest;
}
void beep(unsigned int frequency) {
    // Set the PIT to the desired frequency
    unsigned int divisor = 1193180 / frequency; // PIT frequency is 1193180 Hz
    outb(0x43, 0x36); // Command port: Set the PIT to mode 3 (square wave generator)
    outb(0x40, divisor & 0xFF); // Low byte of divisor
    outb(0x40, (divisor >> 8) & 0xFF); // High byte of divisor

    // Enable the speaker
    outb(0x61, inb(0x61) | 0x03);
}

void stop_beep() {
    // Disable the speaker
    outb(0x61, inb(0x61) & 0xFC);
}



#define MAX_FILES 128
#define MAX_FILENAME_LENGTH 32
#define BLOCK_SIZE 512
#define MAX_BLOCKS 1024

typedef struct {
    char name[MAX_FILENAME_LENGTH];
    uint32_t size; // Size of the file in bytes
    uint32_t start_block; // Starting block on disk
} File;

typedef struct {
    uint32_t file_count;
    File files[MAX_FILES];
} Directory;

typedef struct {
    uint8_t block_allocation_table[MAX_BLOCKS]; // 0 = free, 1 = allocated
    Directory root_directory;
} Filesystem;

Filesystem fs;

#define DISK_PORT 0x1F0

void read_block(uint32_t block_num, void* buffer) {
    // Wait for the disk to be ready
    outb(DISK_PORT + 6, 0xE0); // Select drive
    outb(DISK_PORT + 1, 0); // Clear error register
    outb(DISK_PORT + 2, 0); // Sector count
    outb(DISK_PORT + 3, (block_num & 0xFF)); // LBA low
    outb(DISK_PORT + 4, (block_num >> 8) & 0xFF); // LBA mid
    outb(DISK_PORT + 5, (block_num >> 16) & 0xFF); // LBA high
    outb(DISK_PORT + 7, 0x20); // Read sectors command

    // Wait for the disk to finish
    while (!(inb(DISK_PORT + 7) & 0x08)); // Wait for DRQ

    // Read the data
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        ((uint16_t*)buffer)[i] = inw(DISK_PORT);
    }
}

void write_block(uint32_t block_num, const void* buffer) {
    // Wait for the disk to be ready
    outb(DISK_PORT + 6, 0xE0); // Select drive
    outb(DISK_PORT + 1, 0); // Clear error register
    outb(DISK_PORT + 2, 0); // Sector count
    outb(DISK_PORT + 3, (block_num & 0xFF)); // LBA low
    outb(DISK_PORT + 4, (block_num >> 8) & 0xFF); // LBA mid
    outb(DISK_PORT + 5, (block_num >> 16) & 0xFF); // LBA high
    outb(DISK_PORT + 7, 0x30); // Write sectors command

    // Wait for the disk to finish
    while (!(inb(DISK_PORT + 7) & 0x08)); // Wait for DRQ

    // Write the data
    for (int i = 0; i < BLOCK_SIZE / 2; i++) {
        outw(DISK_PORT, ((uint16_t*)buffer)[i]);
    }
}

void write_file(const char* filename, const char* content) {
    // Find a free block in the block allocation table
    for (size_t i = 0; i < MAX_BLOCKS; i++) {
        if (fs.block_allocation_table[i] == 0) {
            // Allocate the block
            fs.block_allocation_table[i] = 1;

            // Create the file entry
            File new_file;
            strcpy(new_file.name, filename);
            new_file.size = strlen(content);
            new_file.start_block = i;

            // Write the file content to the allocated block
            char buffer[BLOCK_SIZE] = {0};
            strncpy(buffer, content, BLOCK_SIZE);
            write_block(i, buffer);

            // Add the file to the root directory
            fs.root_directory.files[fs.root_directory.file_count++] = new_file;

            // Write the updated directory and allocation table back to disk
            write_block(0, &fs.root_directory);
            write_block(1, fs.block_allocation_table);
            return;
        }
    }
}

void read_file(const char* filename) {
    for (size_t i = 0; i < fs.root_directory.file_count; i++) {
        if (strcmp(fs.root_directory.files[i].name, filename) == 0) {
            char buffer[BLOCK_SIZE] = {0};
            read_block(fs.root_directory.files[i].start_block, buffer);
            terminal_writestring(buffer);
            return;
        }
    }
}

void filesystem_initialize() {
    // Read the root directory and block allocation table from disk
    read_block(0, &fs.root_directory); // Assuming the root directory is at block 0
    read_block(1, fs.block_allocation_table); // Assuming the block allocation table is at block 1
}


const char keyboard_layout[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ', 
    '!', '@', ' ', '$', '%', '^', '&', '*', '(', ')', '~',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '/', '|' 
};

uint8_t read_keyboard() {
    uint8_t data;
    while ((inb(0x64) & 0x01) == 0) {
        // Wait for data to be available
    }
    data = inb(0x60);

    if (data & 0x80) {
        return 0;
    }
    
    if (data < sizeof(keyboard_layout)) {
        return keyboard_layout[data];
    }
    
    return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n--) {
        if (*s1 != *s2) {
            return (*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : 1;
        }
        if (*s1 == '\0') {
            return 0;
        }
        s1++;
        s2++;
    }
    return 0;
}

void handle_keyboard_input() {
    uint8_t data = read_keyboard();
    if (data == 0) {
        return;
    }

    if (data == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            input_buffer_index -= 1;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
    } else {
        if (data == '\n') {
            input_buffer[input_buffer_index] = '\0'; // Null-terminate the input
            if (strcmp(input_buffer, "ls") == 0) {
				terminal_newline();
				printf("Filesystem support has not been added yet.");
			//} else if (strncmp(input_buffer, "cat ", 4) == 0) {
            //    read_file(input_buffer + 4); // Read file command
            //} else if (strncmp(input_buffer, "echo ", 5) == 0) {
            //   char* filename = strtok(input_buffer + 5, " ");
            //    char* content = strtok(NULL, "\0");
            //    if (filename && content) {
            //        write_file(filename, content);
            //    } else {
            //        terminal_writestring("Usage: echo <filename> <content>\n");
            //    }
			} else if (strcmp(input_buffer, "help") == 0) {
				terminal_newline();
				printf("PotatoOS ");
				printf(version);
				printf(".");
				printf(subversion);
				terminal_newline();
				terminal_newline();
				printf("Test");
			} else if (strcmp(input_buffer, "clear") == 0) {
				terminal_buffer = (uint16_t*) 0xB8000;
				for (size_t y = 0; y < VGA_HEIGHT; y++) {
					for (size_t x = 0; x < VGA_WIDTH; x++) {
						const size_t index = y * VGA_WIDTH + x;
						terminal_buffer[index] = vga_entry(' ', terminal_color);
					}
				}
				terminal_row = 1;
				terminal_column = 2;
			} else if (strcmp(input_buffer, "fsinit") == 0) {
				terminal_newline();
				terminal_writestring("[   ] Initialize filesystem...");
				filesystem_initialize();
			} else if (strcmp(input_buffer, "panic") == 0) {
				panic("User called panic.");
            } else {
            	terminal_newline();
				beep(440);
				char* command = input_buffer;
                printf("Unknown command.");
            }
            terminal_newline();
            terminal_writestring(username);
            terminal_writestring("@");
            terminal_writestring(hostname);
            terminal_writestring(" /> ");
            input_buffer_index = 0; // Reset input buffer index
        } else {
            if (input_buffer_index < sizeof(input_buffer) - 1) {
                input_buffer[input_buffer_index++] = data;
                terminal_putchar(data);
            }
        }
    }
}


const bool load_ahci_driver = false;

void panic(char* msg) 
{
	terminal_newline();
	terminal_writestring(msg);
	terminal_newline();
	terminal_writestring("Kernel panic!");
	__asm__("hlt");
}

//void basic_vga_driver() 
//{
//    __asm__ __volatile__ (
//        "int $0x10"
//        :
//        : "a"(0x00 | 0x03) 
//    );
//}

void kernel_main(void) 
{
	/* Initialize terminal interface */
	terminal_initialize();
	terminal_row = 23;
	terminal_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
	terminal_writestring(" help = help   ls = list   rm = delete   cd = change dir                    ");
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_LIGHT_BLUE);
	terminal_row = 1;
	terminal_column = 2;
    	terminal_writestring("Welcome to ");
    	terminal_writestring(osname);
    	terminal_writestring(" ");
    	terminal_writestring(version);
    	terminal_writestring(".");
    	terminal_writestring(subversion);
    	terminal_writestring("!");
	terminal_newline();
	terminal_color = vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_LIGHT_BLUE); 
	terminal_writestring("WARNING: PotatoOS is in alpha! I am NOT responsible for ANY data loss.");

	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_LIGHT_BLUE);
	terminal_newline();	


	
	terminal_newline();
	terminal_writestring(username);
	terminal_writestring("@");
	terminal_writestring(hostname);
	terminal_writestring(" /> ");	
	while (1) {
        	handle_keyboard_input();
    	}

}
