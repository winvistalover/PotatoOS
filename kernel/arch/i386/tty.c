#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
static inline char* subversion = "2.3";

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
}


const char keyboard_layout[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' ', 
    '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '~',
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

void handle_keyboard_input() {
    uint8_t data = read_keyboard();
    if (data == 0) {
        return;
    }

    if (data == '\b') {
        if (terminal_column > 0) {
            terminal_column--;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        }
    } else {
        if (data == '\n') { 
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
            } else {
                terminal_newline();
            }
            terminal_writestring(username);
            terminal_writestring("@");
            terminal_writestring(hostname);
            terminal_writestring(" /> ");    
        } else {
            terminal_putchar(data);
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
void disable_cursor() {
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}
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
	if (load_ahci_driver) {
		terminal_writestring("[   ] Start basic AHCI driver...");
	} else {
		terminal_writestring("[ SKIP ] Start basic AHCI driver...");
	}

	terminal_newline();
	terminal_writestring("[ OK ] Start trash keyboard driver that I will fix later...");
	disable_cursor();
	//terminal_newline();
	//terminal_writestring("[   ] Start less basic but still basic VGA driver...");
	
	//basic_vga_driver();	
	
	terminal_newline();
	terminal_writestring(username);
	terminal_writestring("@");
	terminal_writestring(hostname);
	terminal_writestring(" /> ");	
	while (1) {
        	handle_keyboard_input();
    	}
}
