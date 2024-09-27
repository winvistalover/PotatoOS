#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H
#include <stddef.h>
bool fsinit;
bool networkinit;
bool waitwrite;

size_t input_buffer_index = 0;
size_t strlen(const char* str);

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_scroll();
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void terminal_setcolor(uint8_t color);
void shutdown();
void stop_beep();
void reboot();
void outw(uint16_t port, uint16_t value);
void outb(uint16_t port, uint8_t value);
void beep(unsigned int frequency);
void int32(uint8_t intnum, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t *ax_out);

uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);
uint16_t inw(uint16_t port);
uint8_t inb(uint16_t port);
uint8_t read_keyboard();


char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
char *strtok(char *str, const char *delim);
char *strchr(const char *s, int c);
char input_buffer[256];
const char keyboard_layout[128];

int strcmp(const char* str1, const char* str2);

#endif
