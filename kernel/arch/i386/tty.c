#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define NO_ERROR 0
#define FILE_OPEN_ERROR -1
#define DATA_READ_ERROR -2
#define DATA_WRITE_ERROR -3
#define DATA_MISMATCH_ERROR -4



#include <kernel/tty.h>

#include "vga.h"


#define UART0_BASE 0x101f0000

typedef	int		fpos_t;

struct __sbuf {
	unsigned char *_base;
	int	_size;
};

struct __sFILE {
	unsigned char *_p;	/* (*) current position in (some) buffer */
	int	_r;		/* (*) read space left for getc() */
	int	_w;		/* (*) write space left for putc() */
	short	_flags;		/* (*) flags, below; this FILE is free if 0 */
	short	_file;		/* (*) fileno, if Unix descriptor, else -1 */
	struct	__sbuf _bf;	/* (*) the buffer (at least 1 byte, if !NULL) */
	int	_lbfsize;	/* (*) 0 or -_bf._size, for inline putc */

	/* operations */
	void	*_cookie;	/* (*) cookie passed to io functions */
	int	(*_close)(void *);
	int	(*_read)(void *, char *, int);
	fpos_t	(*_seek)(void *, fpos_t, int);
	int	(*_write)(void *, const char *, int);

	/* separate buffer for long sequences of ungetc() */
	struct	__sbuf _ub;	/* ungetc buffer */
	unsigned char	*_up;	/* saved _p when _p is doing ungetc data */
	int	_ur;		/* saved _r when _r is counting ungetc data */

	/* tricks to meet minimum requirements even when malloc() fails */
	unsigned char _ubuf[3];	/* guarantee an ungetc() buffer */
	unsigned char _nbuf[1];	/* guarantee a getc() buffer */

	/* separate buffer for fgetln() when line crosses buffer boundary */
	struct	__sbuf _lb;	/* buffer for fgetln() */

	/* Unix stdio files get aligned to block boundaries on fseek() */
	int	_blksize;	/* stat.st_blksize (may be != _bf._size) */
	fpos_t	_offset;	/* current lseek offset */

	struct pthread_mutex *_fl_mutex;	/* used for MT-safety */
	struct pthread *_fl_owner;	/* current owner */
	int	_fl_count;	/* recursive lock count */
	int	_orientation;	/* orientation for fwide() */
	//__mbstate_t _mbstate;	/* multibyte conversion state */
};

typedef struct __sFILE FILE;

volatile uint32_t * const UART0DR = (uint32_t *) (UART0_BASE + 0x00);
volatile uint32_t * const UART0FR = (uint32_t *) (UART0_BASE + 0x18);
volatile uint32_t * const UART0IBRD = (uint32_t *) (UART0_BASE + 0x24);
volatile uint32_t * const UART0FBRD = (uint32_t *) (UART0_BASE + 0x28);
volatile uint32_t * const UART0LCRH = (uint32_t *) (UART0_BASE + 0x2C);
volatile uint32_t * const UART0CR = (uint32_t *) (UART0_BASE + 0x30);

bool noprint;

void uart_init() {
    // Disable UART0
    *UART0CR = 0x00000000;
    // Set baud rate
    *UART0IBRD = 1;    // Integer part of the baud rate divisor
    *UART0FBRD = 40;   // Fractional part of the baud rate divisor
    // Set the line control register
    *UART0LCRH = (1 << 5) | (1 << 6); // 8 bits, no parity, 1 stop bit, FIFOs enabled
    // Enable UART0, TX, and RX
    *UART0CR = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putc(char c) {
    // Wait for UART to be ready to transmit
    while (*UART0FR & (1 << 5)) {}
    *UART0DR = c;
}

void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}


static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;
bool havebeeninitbefore;


static inline char* osname = "PotatoOS";
static inline char* version = "0";
static inline char* subversion = "2.5.2";

static inline char* username = "potato";
static inline char* hostname = "live";

#define MAX_EVENTS 10000

char* eventlog[MAX_EVENTS];
int size = 1;

enum taskstate {
    task_waiting = 0,
    task_ok = 1,
    task_failed = 2,
};

void addevent(char* new_event) {
    if (size < MAX_EVENTS) {
        eventlog[size] = new_event;
        size++;
    }
}

size_t strlen(const char* str) 
{
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}
char* task(char* name, int state) {
    addevent(state);
    addevent(name);
    if (!noprint) {
        terminal_newline();
        //terminal_column = 10;
        if (state == 0) {
            terminal_writestring("[      ] ");
            terminal_writestring(name);
        } else if (state == 1) {
            terminal_writestring("[  ");
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            terminal_writestring("OK");
            terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            terminal_writestring("  ] ");
            terminal_writestring(name);
        } else if (state == 2) {
            terminal_writestring("[ ");
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_writestring("FAIL");
            terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
            terminal_writestring(" ] ");
            terminal_writestring(name);
        }
    }
}


void shutdown() {
    task("Attempting to shutdown computer...", 0);
    task("Shutdown computer using old QEMU...", 0);
	
	outw(0xB004, 0x2000);

    task("Shutdown computer using new QEMU...", 0);

	outw(0x604, 0x2000);

    task("Shutdown computer using Virtualbox...", 0);

	outw(0x4004, 0x3400);

    task("Shutdown computer...", 2);
    task("Sending panic...", 0);

    panic("Failed to shutdown computer. It is now safe to turn off your computer.");

}

void reboot() {
    // Disable interrupts
    task("Reboot computer...", 0);
    asm volatile ("cli");

    // Use the BIOS interrupt to reboot
    asm volatile (
        "movb $0xFE, %al\n"  // Set the reset command
        "outb %al, $0x64\n"  // Send the command to the keyboard controller
    );

    // Hang the CPU if the reboot command fails
    task("Reboot computer...", 2);
    task("Sending hlt...", 0);
    while (1) {
        asm volatile ("hlt");
    }
}

char input_buffer[256];
size_t input_buffer_index = 0;

bool waitwrite;


int mainfat() {
    task("No FAT FS support yet.", 2);
    return 1;
}


void terminal_initialize(void) 
{
	terminal_row = 1;
	//terminal_column = 0;
    if (!havebeeninitbefore) {
        terminal_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    }
	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}


	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT - 20; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
            if (waitwrite) {
                for (volatile int i = 0; i < 60000; i++);
            }
		}
	}
    havebeeninitbefore = true;
    if (terminal_buffer == (uint16_t*) 0xB8005) {
        terminal_buffer = (uint16_t*) 0xB8000;
        panic("Invalid buffer");
    }
}

void terminal_setcolor(uint8_t color) 
{
	terminal_color = color;
}

uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a" (value), "Nd" (port));
}


void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) 
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
    update_cursor(x + 1,y);
}

void terminal_putchar(char c) 
{
	if (terminal_row == VGA_HEIGHT - 1) {
        	terminal_scroll();
    	}
    if (waitwrite) {
        for (volatile int i = 0; i < 60000; i++);
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
        if (waitwrite) {
            for (volatile int i = 0; i < 60000; i++);
        }
}

void terminal_scroll() {
    /*for (int i = 0; i < VGA_HEIGHT - 1; i++) {
        for (int j = 0; j < VGA_WIDTH; j++) {
            terminal_buffer[i * VGA_WIDTH + j] = terminal_buffer[(i + 1) * VGA_WIDTH + j];
            if (waitwrite) {
                for (volatile int i = 0; i < 60000000; i++);
            }
        }
    }
    for (int j = 0; j < VGA_WIDTH; j++) {
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + j] = vga_entry(' ', terminal_color);
        if (waitwrite) {
            for (volatile int i = 0; i < 60000000; i++);
        }
    }
    terminal_column = 0;*/
    terminal_initialize();
}


void terminal_writestring(const char* data) 
{
	terminal_write(data, strlen(data));
}

void terminal_newline(void)
{
	terminal_row += 1;
	terminal_column = 0;
	if (terminal_row == VGA_HEIGHT) {
		terminal_buffer = (uint16_t*) 0xB8000;
		for (size_t y = 0; y < VGA_HEIGHT; y++) {
			for (size_t x = 0; x < VGA_WIDTH; x++) {
				const size_t index = y * VGA_WIDTH + x;
				terminal_buffer[index] = vga_entry(' ', terminal_color);
			}
		}
		terminal_row = 0;
		terminal_column = 0;
	}
}

uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile ("inw %1, %0" : "=a" (result) : "Nd" (port));
    return result;
}

void outw(uint16_t port, uint16_t value) {
    asm volatile ("outw %0, %1" : : "a" (value), "Nd" (port));
}


bool fsinit;



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
    // Enable the speaker
    outb(0x61, inb(0x61) | 0x03);
    // Set the PIT to the desired frequency
    unsigned int divisor = 1193180 / frequency; // PIT frequency is 1193180 Hz
    outb(0x43, 0x28); // Command port: Set the PIT to mode 3 (square wave generator)
    outb(0x40, divisor & 0xFF); // Low byte of divisor
    outb(0x40, (divisor >> 80) & 0xFF); // High byte of divisor
    for (volatile int i = 0; i < 600000000; i++);
    stop_beep();
}

void stop_beep() {
    // Disable the speaker
    outb(0x61, inb(0x61) & 0xFC);
}




#define MAX_FILES 128
#define MAX_FILENAME_LENGTH 32
#define BLOCK_SIZE 512
#define MAX_BLOCKS 1024
#define MAX_MOUNTED_DRIVES 4


typedef struct {
    char name[MAX_FILENAME_LENGTH];
    uint32_t size; // Size of the file in bytes
    uint32_t start_cluster; // Starting cluster on disk
} FileEntry;

typedef struct {
    uint32_t file_count;
    FileEntry files[MAX_FILES];
} Directory;

typedef struct {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t max_root_dir_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t number_of_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t flags;
    uint16_t version;
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved1;
    uint8_t boot_signature;
    uint32_t volume_id;
    char volume_label[11];
    char file_system_type[8];
} __attribute__((packed)) FAT32BootSector;

typedef struct {
    bool is_mounted;
    FAT32BootSector boot_sector;
    uint32_t fat_start; // Start sector of the FAT
    uint32_t data_start; // Start sector of the data area
    Directory root_directory; // Add root directory structure
} MountedDrive;

MountedDrive mounted_drives[MAX_MOUNTED_DRIVES];


void read_boot_sector(uint8_t drive_number) {
    read_block(0, &mounted_drives[drive_number].boot_sector);
}

void initialize_fat32(uint8_t drive_number) {
    FAT32BootSector* boot_sector = &mounted_drives[drive_number].boot_sector;

    // Calculate the start of the FAT and data area
    mounted_drives[drive_number].fat_start = boot_sector->reserved_sectors;
    mounted_drives[drive_number].data_start = boot_sector->reserved_sectors + (boot_sector->num_fats * boot_sector->fat_size_32);
}


bool mount_drive(uint8_t drive_number) {
    if (drive_number >= MAX_MOUNTED_DRIVES) {
        return false; // Invalid drive number
    }

    MountedDrive* drive = &mounted_drives[drive_number];
    if (drive->is_mounted) {
        return false; // Drive already mounted
    }

    // Read the boot sector
    read_boot_sector(drive_number);
    
    // Initialize the FAT32 structure
    initialize_fat32(drive_number);

    drive->is_mounted = true;

    terminal_newline();
    terminal_writestring("[  OK  ] Drive mounted successfully.");
    return true;
}

#define DISK_PORT 0x0
void read_block(uint32_t block_num, void* buffer) {
    // Wait for the disk to be ready
    outb(DISK_PORT + 6, 0xE0); // Select drive
    outb(DISK_PORT + 1, 0); // Clear error register
    outb(DISK_PORT + 2, 1); // Sector count
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
    outb(DISK_PORT + 2, 1); // Sector count
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

uint32_t get_cluster_address(uint32_t cluster_number, MountedDrive* drive) {
    // Calculate the address of the cluster in the data area
    return drive->data_start + (cluster_number - 2) * drive->boot_sector.sectors_per_cluster;
}

void read_cluster(uint32_t cluster_number, void* buffer, MountedDrive* drive) {
    uint32_t cluster_address = get_cluster_address(cluster_number, drive);
    for (uint32_t i = 0; i < drive->boot_sector.sectors_per_cluster; i++) {
        read_block(cluster_address + i, (uint8_t*)buffer + (i * BLOCK_SIZE));
    }
}

void write_cluster(uint32_t cluster_number, const void* buffer, MountedDrive* drive) {
    uint32_t cluster_address = get_cluster_address(cluster_number, drive);
    for (uint32_t i = 0; i < drive->boot_sector.sectors_per_cluster; i++) {
        write_block(cluster_address + i, (const uint8_t*)buffer + (i * BLOCK_SIZE));
    }
}

void read_root_directory(uint8_t drive_number) {
    MountedDrive* drive = &mounted_drives[drive_number];
    uint32_t cluster = drive->boot_sector.root_cluster;
    FileEntry test;
    test.name[1] = 'test';
    drive->root_directory.files[drive->root_directory.file_count++] = test;

    // Read the root directory entries starting from the root cluster
    for (size_t i = 0; i < MAX_FILES; i++) {
        FileEntry entry;
        // Read the cluster into the entry (you will need to implement the logic to read the directory entries)
        // For example, read the first cluster of the root directory
        read_cluster(cluster, &entry, drive);
        
        // Check if the entry is valid (e.g., not deleted)
        //if (entry.size > 0) {
        //drive->root_directory.files[drive->root_directory.file_count++] = entry;
        //printf(test.name[1]);
        //}
        
        // Move to the next entry (you will need to implement the logic to find the next entry)
    }
}

#define FAT_ENTRY_SIZE 4 // FAT32 uses 32-bit entries

void read_fat_table(uint32_t* fat_table, MountedDrive* drive) {
    uint32_t fat_start = drive->fat_start;
    for (uint32_t i = 0; i < (drive->boot_sector.fat_size_32 * drive->boot_sector.num_fats); i++) {
        read_block(fat_start + i, (uint8_t*)&fat_table[i * BLOCK_SIZE / FAT_ENTRY_SIZE]);
    }
}


bool is_cluster_free(uint32_t cluster_number, uint32_t* fat_table) {
    return (fat_table[cluster_number] == 0);
}


uint32_t find_free_cluster(MountedDrive* drive) {
    uint32_t fat_table[BLOCK_SIZE / FAT_ENTRY_SIZE]; // Adjust size as needed
    read_fat_table(fat_table, drive); // Read the FAT table into memory

    // Iterate through the FAT table to find a free cluster
    for (uint32_t i = 2; i < (drive->boot_sector.total_sectors_32 / drive->boot_sector.sectors_per_cluster); i++) {
        if (is_cluster_free(i, fat_table)) {
            return i; // Return the first free cluster found
        }
    }
    return 0; // No free cluster found
}


void write_file(uint8_t drive_number, const char* filename, const char* content) {
    MountedDrive* drive = &mounted_drives[drive_number];

    // Find a free entry in the root directory
    for (size_t i = 0; i < MAX_FILES; i++) {
        if (drive->root_directory.files[i].size == 0) { // Assuming size 0 means free
            // Create the file entry
            FileEntry new_file;
            strncpy(new_file.name, filename, MAX_FILENAME_LENGTH);
            new_file.size = strlen(content);
            new_file.start_cluster = find_free_cluster(drive); // Find a free cluster

            // Write the file content to the allocated cluster
            char buffer[BLOCK_SIZE] = {0};
            strncpy(buffer, content, BLOCK_SIZE);
            write_cluster(new_file.start_cluster, buffer, drive);

            // Update the FAT table to mark the cluster as used
            update_fat_entry(new_file.start_cluster, 0xFFFFFFFF, drive); // Mark as end of file (EOF)

            // Add the file to the root directory
            drive->root_directory.files[drive->root_directory.file_count++] = new_file;

            return;
        }
    }
}

void update_fat_entry(uint32_t cluster_number, uint32_t value, MountedDrive* drive) {
    uint32_t fat_table[BLOCK_SIZE / FAT_ENTRY_SIZE];
    read_fat_table(fat_table, drive); // Read the FAT table into memory

    // Update the FAT entry for the specified cluster
    fat_table[cluster_number] = value;

    // Write the updated FAT table back to disk
    uint32_t fat_start = drive->fat_start;
    write_block(fat_start + (cluster_number * FAT_ENTRY_SIZE / BLOCK_SIZE), fat_table);
}


void read_file(uint8_t drive_number, const char* filename) {
    MountedDrive* drive = &mounted_drives[drive_number];

    for (size_t i = 0; i < drive->root_directory.file_count; i++) {
        if (strcmp(drive->root_directory.files[i].name, filename) == 0) {
            FileEntry* file = &drive->root_directory.files[i];
            char buffer[BLOCK_SIZE] = {0};

            // Read the file content from the allocated cluster
            read_cluster(file->start_cluster, buffer, drive);
            terminal_writestring(buffer);
            return;
        }
    }
}

char *strchr(const char *s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char *)s; // Return a pointer to the first occurrence
        }
        s++;
    }
    return NULL; // Return NULL if the character is not found
}


char *strtok(char *str, const char *delim) {
    static char *last = NULL; // Static variable to hold the last token
    char *current_token;

    // If str is NULL, continue tokenizing the last string
    if (str == NULL) {
        str = last;
    }

    // Skip leading delimiters
    while (*str && strchr(delim, *str)) {
        str++;
    }

    // If we reached the end of the string, return NULL
    if (*str == '\0') {
        last = NULL;
        return NULL;
    }

    // Find the end of the token
    current_token = str;
    while (*str && !strchr(delim, *str)) {
        str++;
    }

    // Null-terminate the token
    if (*str) {
        *str = '\0'; // Replace delimiter with null terminator
        last = str + 1; // Save the position for the next call
    } else {
        last = NULL; // No more tokens
    }

    return current_token; // Return the current token
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

uint16_t read_keyboard() {
    uint16_t data;
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


char* fakeroot[] = {"/usr", "/root", "/var", "/sys", "/boot", "/dev", "/dev/console", "/dev/random", "/dev/stdout", "/dev/stdin", "/dev/stderr"};

bool exitHKIloop;

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0) {
        if (*s1 == '\0' || *s2 == '\0') {
            return (unsigned char)(*s1) - (unsigned char)(*s2);
        }

        if (*s1 != *s2) {
            return (unsigned char)(*s1) - (unsigned char)(*s2);
        }

        s1++;
        s2++;
        n--;
    }

    return 0;
}

/* void ReadFromCMOS (unsigned char array [])
{
   unsigned char tvalue, index;

   for(index = 0; index < 128; index++)
   {
      __asm__
      {
         cli             /* Disable interrupts
         mov al, index   /* Move index address
         /* since the 0x80 bit of al is not set, NMI is active
         out 0x70,al     /* Copy address to CMOS register
         /* some kind of real delay here is probably best
         in al,0x71      /* Fetch 1 byte to al
         sti             /* Enable interrupts
         mov tvalue,al
       }

       array[index] = tvalue;
   }
}

void WriteTOCMOS(unsigned char array[])
{
   unsigned char index;

   for(index = 0; index < 128; index++)
   {
      unsigned char tvalue = array[index];
      __asm__
      {
         cli             /* Clear interrupts*/
   //      mov al,index    /* move index address*/
     //    out 0x70,al     /* copy address to CMOS register
         /* some kind of real delay here is probably best 
         mov al,tvalue   /* move value to al
         out 0x71,al     /* write 1 byte to CMOS
         sti             /* Enable interrupts
      }
   }
} */

#define CURRENT_YEAR        2023                            // Change this each year!

int century_register = 0x00;                                // Set by ACPI table parsing code if possible

unsigned char second;
unsigned char minute;
unsigned char hour;
unsigned char day;
unsigned char month;
unsigned int year;
FILE stdin;
FILE stdout;
FILE stderr;



enum {
      cmos_address = 0x70,
      cmos_data    = 0x71
};

int get_update_in_progress_flag() {
      outb(cmos_address, 0x0A);
      return (inb(cmos_data) & 0x80);
}

unsigned char get_RTC_register(int reg) {
      outb(cmos_address, reg);
      return inb(cmos_data);
}

void read_rtc() {
      task("Read RTC...", 0);
      unsigned char century;
      unsigned char last_second;
      int century_register = 0x00;   
      unsigned char last_minute;
      unsigned char last_hour;
      unsigned char last_day;
      unsigned char last_month;
      unsigned char last_year;
      unsigned char last_century;
      unsigned char registerB;

      // Note: This uses the "read registers until you get the same values twice in a row" technique
      //       to avoid getting dodgy/inconsistent values due to RTC updates

      while (get_update_in_progress_flag());                // Make sure an update isn't in progress
      second = get_RTC_register(0x00);
      minute = get_RTC_register(0x02);
      hour = get_RTC_register(0x04);
      day = get_RTC_register(0x07);
      month = get_RTC_register(0x08);
      year = get_RTC_register(0x09);
      if(century_register != 0) {
            century = get_RTC_register(century_register);
      }

      do {
            last_second = second;
            last_minute = minute;
            last_hour = hour;
            last_day = day;
            last_month = month;
            last_year = year;
            last_century = century;

            while (get_update_in_progress_flag());           // Make sure an update isn't in progress
            second = get_RTC_register(0x00);
            minute = get_RTC_register(0x02);
            hour = get_RTC_register(0x04);
            day = get_RTC_register(0x07);
            month = get_RTC_register(0x08);
            year = get_RTC_register(0x09);
            if(century_register != 0) {
                  century = get_RTC_register(century_register);
            }
      } while( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
               (last_day != day) || (last_month != month) || (last_year != year) ||
               (last_century != century) );

      registerB = get_RTC_register(0x0B);

      // Convert BCD to binary values if necessary

      if (!(registerB & 0x04)) {
            second = (second & 0x0F) + ((second / 16) * 10);
            minute = (minute & 0x0F) + ((minute / 16) * 10);
            hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
            day = (day & 0x0F) + ((day / 16) * 10);
            month = (month & 0x0F) + ((month / 16) * 10);
            year = (year & 0x0F) + ((year / 16) * 10);
            if(century_register != 0) {
                  century = (century & 0x0F) + ((century / 16) * 10);
            }
      }

      // Convert 12 hour clock to 24 hour clock if necessary

      if (!(registerB & 0x02) && (hour & 0x80)) {
            hour = ((hour & 0x7F) + 12) % 24;
      }

      // Calculate the full (4-digit) year

      if(century_register != 0) {
            year += century * 100;
      } else {
            year += (CURRENT_YEAR / 100) * 100;
            if(year < CURRENT_YEAR) year += 100;
      }
      task("Read RTC...", 1);
      /* terminal_putchar(second);
      terminal_putchar(minute);
      terminal_putchar(hour);
      terminal_putchar(month);
      terminal_putchar(year); */
}

extern void initTasking();

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp, eip, eflags, cr3;
} Registers;

typedef struct Task {
    Registers regs;
    struct Task *next;
} Task;



    


void handle_keyboard_input() {
    uint8_t data = read_keyboard();
    if (data == 0) {
        return;
    }
    input_buffer[input_buffer_index] = '\0';
    if (data == '\b') {
        if (terminal_column > strlen(username) + strlen(hostname) + 5) {
            terminal_column--;
            input_buffer_index -= 1;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
            update_cursor(terminal_column, terminal_row);
        }
    } else {
        if (data == 'U') {
            data -= 'U';
            terminal_color = vga_entry_color(terminal_row * input_buffer_index, input_buffer_index);
            terminal_buffer = (uint16_t*) 0xB8000;
            terminal_row = 0;
            for (size_t y = 0; y < VGA_HEIGHT; y++) {
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    const size_t index = y * VGA_WIDTH + x;
                    terminal_buffer[index] = vga_entry(' ', terminal_color);
                    for (volatile int i = 0; i < 60000; i++);
                }
            }
            terminal_row = 6;
            terminal_column = 6;
            terminal_writestring("Select a color");
        }
        if (data == '\n') { // Null-terminate the input
            if (strcmp(input_buffer, "ls") == 0) {
				terminal_newline();
				if (fsinit != true) {
					panic("FS NOT INIT!");
				} else {
                    for (int i = 0; i < sizeof(fakeroot) / sizeof(fakeroot[0]); i++) {
                        printf("%s ", fakeroot[i]);
                    }
					//printf(mounted_drives[1].fs.root_directory.file_count);
					//for (size_t i = 0; i < mounted_drives[1].fs.root_directory.file_count; i++) {
					//	terminal_writestring("File or dir: ");
            		//	terminal_writestring(mounted_drives[1].fs.root_directory.files[i].name);
					//	terminal_newline();
        			//}
				}
            } else if (strcmp(input_buffer, "le") == 0) {
                printf("%n");
                for (int i = 0; i < size; i++) {
                    printf("%s | ", eventlog[i]);
                }
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
            } else if (strncmp(input_buffer, "echo ", 5) == 0) {
                char* content = strtok(input_buffer + 5, " ");
                terminal_newline();
                terminal_writestring(content);
            } else if (strcmp(input_buffer, "m") == 0){
                m();
            } else if (strcmp(input_buffer, "ppm") == 0) {
                terminal_newline();
                terminal_writestring("PotatoOS Package Manager");
                terminal_newline();
                terminal_writestring("Usage ppm [install][remove][update] [package name]");
                terminal_newline();
                terminal_writestring("This is a placeholder.");
			} else if (strcmp(input_buffer, "help") == 0) {
				terminal_newline();
				printf("PotatoOS ");
				printf(version);
				printf(".");
				printf(subversion);
				terminal_newline();
				terminal_newline();
                printf("fatinit         - Initialize the FAT.");
                terminal_newline();
                printf("shutdown        - Shut down the computer.");
                terminal_newline();
                printf("color           - Show the color test screen.");
                terminal_newline();
                printf("ls              - List all the contents of the currently open directory.");
                terminal_newline();
                printf("echo            - Output text onto the terminal.");
                terminal_newline();
                printf("cat             - View the contents of a file.");
                terminal_newline();
                printf("waitwrite       - Wait a duration of time before outputting a character onto the terminal, an example of this is in the kernel panic screen.");
                terminal_newline();
                printf("ld              - Link object files and libraries.");
			} else if (strcmp(input_buffer, "clear") == 0) {
				/*terminal_buffer = (uint16_t*) 0xB8000;
				for (size_t y = 0; y < VGA_HEIGHT; y++) {
					for (size_t x = 0; x < VGA_WIDTH; x++) {
						const size_t index = y * VGA_WIDTH + x;
						terminal_buffer[index] = vga_entry(' ', terminal_color);
					}
				}
				terminal_row = 1; */
	            terminal_initialize();
				//terminal_column = 2;
			//} else if (strcmp(input_buffer, "fsinit") == 0) {
			//	filesystem_initialize();
            } else if (strcmp(input_buffer, "exit") == 0) {
                exitHKIloop = true;
			} else if (strcmp(input_buffer, "panic") == 0) {
				//panic("User called panic.");
                fsinit = false;
            } else if (strcmp(input_buffer, "reboot") == 0) {
				reboot();
            } else if (strcmp(input_buffer, "g") == 0) {
                outb(0x3D4, 0x0A);
                outb(0x3D5, 0x02);
                /* terminal_newline();
                printf("▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄");
                terminal_newline();
                printf("█ ▄▄▄▄▄ █▄ ▄███ ▄▄▄▄▄ █");
                terminal_newline();
                printf("█ █   █ █   ▀██ █   █ █");
                terminal_newline();
                printf("█ █▄▄▄█ █▄ ▀ ▄█ █▄▄▄█ █");
                terminal_newline();
                printf("█▄▄▄▄▄▄▄█▄█▄▀▄█▄▄▄▄▄▄▄█");
                terminal_newline();
                printf("█    █▀▄▀ ▀▄██ █▀ ▄▄█ █");
                terminal_newline();
                printf("█ █▀▄██▄▀▀█  █ ▄ █ █ ▄█");
                terminal_newline();
                printf("█▄█▄███▄▄▀  ▀ █ ▄ ██ ▀█");
                terminal_newline();
                printf("█ ▄▄▄▄▄ ██ ▄▄▀ █▄▄▄▀▀██");
                terminal_newline();
                printf("█ █   █ █▀▀▄▄▀▀▀██▄▄█▄█");
                terminal_newline();
                printf("█ █▄▄▄█ █ ▄▀█  ▀▀▀█▄█▀█");
                terminal_newline();
                printf("█▄▄▄▄▄▄▄█▄▄████▄█▄█▄███"); */
                //set_vga_mode();
            } else if (strcmp(input_buffer, "fatinit") == 0) {
                terminal_newline();
                terminal_writestring("[      ] Attempting to initialize FAT...");
                if (mainfat() == 0) {
                    fsinit = true;
                } else {
                    fsinit = false;
                }
            } else if (strcmp(input_buffer, "waitwrite") == 0) {
                waitwrite = true;
                /* if (content == true) {
                    waitwrite = content;
                    terminal_newline();
                    terminal_writestring("waitwrite is now turned on.");
                } else if (content == false) {
                    waitwrite = content;
                    terminal_newline();
                    terminal_writestring("waitwrite is now turned off.");
                } */
			} else if (strcmp(input_buffer, "shutdown") == 0) {
				shutdown();
            //} else if (strcmp(input_buffer, "setup") == 0) {
            //    setup();
            } else if (strncmp(input_buffer, "un ", 2) == 0) {
                char** new_buffer = input_buffer;
                char* content = strtok(new_buffer + 2, " ");
                username = content;
                terminal_newline();
                terminal_writestring("Username is now: ");
                terminal_writestring(username);
            } else if (strcmp(input_buffer, "color") == 0) {
                while (1)   {
                    uint16_t wait = VGA_HEIGHT - VGA_WIDTH;
                    //terminal_row = 0;
                    //terminal_buffer = (uint16_t*) 0xB8005;
                    for (size_t y = 0; y < VGA_HEIGHT - wait; y++) {

                        terminal_buffer = (uint16_t*) 0xB8000 + y;
                        outb(0x3D4, 0x0A);
                        outb(0x3D5, 0x20);
                        for (size_t y = 0; y < VGA_HEIGHT; y++) {
                            for (size_t x = 0; x < VGA_WIDTH; x++) {
                                const size_t index = y * VGA_WIDTH + x;
                                terminal_buffer[index] = vga_entry(' ', terminal_color);
                                //for (volatile int i = 0; i < wait; i++);
                                //for (volatile int i = 0; i < wait; i++) {
                                terminal_color = vga_entry_color(y,x);
                                    //printf(" ");
                                //    terminal_color = vga_entry_color(i, i); 
                                //}
                                //wait -= 10;
                                //terminal_color = vga_entry_color(wait, wait); 
                                data = read_keyboard();
                                if (data == 0);
                                terminal_column += 1;
                                waitwrite = true;
                                nyan();
                                //for (volatile int i = 0; i < 6000; i++);
                            }
                            //beep(y);
                        }
                        terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK); 
                        
                       



                        outb(0x3D4, 0x0A);
                        outb(0x3D5, 0x02);
                    }
                }
            } else if (input_buffer_index != 0) {
            	terminal_newline();
				char* command = input_buffer;
                printf("'");
                printf(input_buffer);
                printf("'");
                printf(" unknown command or file name.");
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

#define RAM_START 0x00000000 
#define RAM_END   0x00003030
#define DUMP_SIZE 4

bool networkinit;

void nyan() {
    terminal_row = 1;
    terminal_newline();
    printf("           @@@@@@@@@@@@@@@@@@@@@@@@           ");
    terminal_newline();
    printf("         @::::------------------::::@@        ");
    terminal_newline();
    printf("         @:::--------+---+-------:::@@        ");
    terminal_newline();
    printf("         @:----------------@@-++---:@@  @@@   ");
    terminal_newline();
    printf("         @:--------------@@==@@----:@@ @===@  ");
    terminal_newline();
    printf("         @:---------+----@@=====@@@@@======@  ");
    terminal_newline();
    printf("@@==@@   @:-----+--------@@================@  ");
    terminal_newline();
    printf("@@==@@@@@@:----------+--@====  @======= @@==@@");
    terminal_newline();
    printf("   @@@==%@:-------------@====@@@====@@=@@@==@@");
    terminal_newline();
    printf("      @@@@:-------++----@=---=============--@@");
    terminal_newline();
    printf("        @@::::-----------@=====@@@@@@@@@===@  ");
    terminal_newline();
    printf("        @@@:::::::::::::::@@==============@   ");
    terminal_newline();
    printf("       @+==@@ @===@         @===@  @===@      ");
    terminal_newline();
    printf("       @@@@    @@@@          @@@@   @@@@      ");
    terminal_newline();
}

int paniccolor = 8;

void panic(char* msg) 
{
    panicwind();
	for (volatile int i = 0; i < 600000000; i++);
	beep(7040);
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
	terminal_writestring("PANIC!!!");
	terminal_color = vga_entry_color(VGA_COLOR_WHITE, paniccolor); 
    terminal_buffer = (uint16_t*) 0xB8000;
    input_buffer_index = 0;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
			for (volatile int i = 0; i < 60000; i++);
        }
    }
    waitwrite = true;
    terminal_row = 6;
    terminal_newline();
    terminal_column = 6;
	terminal_writestring("An error occurred and the system has been shutdown."); // ***The cake is a lie***
    terminal_newline();
    terminal_column = 6;
	terminal_writestring("Contact me at ospotato3@gmail.com with the error message below.");
    terminal_newline();
    terminal_column = 6;
	terminal_writestring("Github: https://github.com/winvistalover/PotatoOS/tree/nightly");
    terminal_newline();
    terminal_newline();
    terminal_column = 6;
	terminal_writestring("Panic: ");
	terminal_writestring(msg);
	terminal_newline();
    terminal_column = 6;
	terminal_writestring("Kernel: ");
	terminal_writestring(osname);
	terminal_writestring(" ");
	terminal_writestring(version);
	terminal_writestring(".");
	terminal_writestring(subversion);
	terminal_newline();
    terminal_newline();
    terminal_newline();
    terminal_column = 6;

    terminal_writestring("The system will automatically reboot");
    terminal_newline();
    terminal_row = VGA_HEIGHT - 3;
    terminal_column = 0;
    terminal_writestring("Timeout:");
    terminal_column = 0;
    terminal_row = VGA_HEIGHT - 2;
	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_WHITE); 
    for (volatile int i = 0; i < VGA_WIDTH; i++) {
        terminal_writestring("#");
        for (volatile int i = 0; i < 60000000; i++);
    }
    reboot();

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

//#define VGA_ADDRESS 0xB8000
//#define VGA_WIDTH 80
//#define VGA_HEIGHT 25

// VGA I/O ports
//#define VGA_CTRL_REGISTER 0x3D4
//#define VGA_DATA_REGISTER 0x3D5


uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF000000) >> 24) |
           ((hostlong & 0x00FF0000) >> 8)  |
           ((hostlong & 0x0000FF00) << 8)  |
           ((hostlong & 0x000000FF) << 24);
}

// Convert 16-bit integer from host to network byte order
uint16_t htons(uint16_t hostshort) {
    return (hostshort >> 8) | (hostshort << 8);
}

// Define network structures
struct ip_header {
    uint8_t  version_ihl;
    uint8_t  type_of_service;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment_offset;
    uint8_t  time_to_live;
    uint8_t  protocol;
    uint16_t header_checksum;
    uint32_t source_address;
    uint32_t destination_address;
};

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence_number;
};

// Function to calculate checksum
uint16_t checksum(void *data, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;
    for (size_t i = 0; i < len / 2; i++) {
        sum += *ptr++;
    }
    if (len % 2) {
        sum += *(uint8_t *)ptr;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
        printf(".");
    }
    return ~sum;
}

// Function to send data through the network controller
void network_send(uint8_t *packet, size_t length) {
    // Example I/O port addresses for the Intel PRO/100 VE
    uint16_t io_base = 0xB8000; // This should be the actual I/O base address of your network card

    // Write the packet to the transmit buffer
    for (size_t i = 0; i < length; i++) {
        outb(io_base + i, packet[i]);
        printf(".");
    }

    // Trigger the transmission
    outb(io_base + 0x10, 0x01); // Example command to start transmission
}

// Function to receive data from the network controller
size_t network_receive(uint8_t *buffer, size_t max_length) {
    // Example I/O port addresses for the Intel PRO/100 VE
    uint16_t io_base = 0x1500; // This should be the actual I/O base address of your network card

    // Check if data is available
    if (inb(io_base + 0x20) & 0x01) { // Example status register check
        size_t length = inb(io_base + 0x24); // Example length register
        if (length > max_length) {
            length = max_length;
        }

        // Read the packet from the receive buffer
        for (size_t i = 0; i < length; i++) {
            buffer[i] = inb(io_base + i);
            printf(".");
        }

        return length;
    }

    return 0; // No data received
}

// Function to send ICMP echo request (ping)
void send_ping(uint32_t dest_ip) {
    struct ip_header ip;
    struct icmp_header icmp;
    uint8_t packet[sizeof(ip) + sizeof(icmp)];

    // Fill IP header
    ip.version_ihl = 0x45;
    ip.type_of_service = 0;
    ip.total_length = htons(sizeof(ip) + sizeof(icmp));
    ip.identification = 0;
    ip.flags_fragment_offset = 0;
    ip.time_to_live = 64;
    ip.protocol = 0; // ICMP
    ip.header_checksum = 0;
    ip.source_address = htonl(0xC0A80001); // 192.168.0.1
    ip.destination_address = htonl(dest_ip);
    ip.header_checksum = checksum(&ip, sizeof(ip));

    // Fill ICMP header
    icmp.type = 8; // Echo request
    icmp.code = 0;
    icmp.checksum = 0;
    icmp.identifier = htons(1);
    icmp.sequence_number = htons(1);
    icmp.checksum = checksum(&icmp, sizeof(icmp));

    // Copy headers to packet
    memcpy(packet, &ip, sizeof(ip));
    memcpy(packet + sizeof(ip), &icmp, sizeof(icmp));

    printf(htonl(0xC0A80001));
    // Send packet
    network_send(packet, sizeof(packet));
}

// Function to receive ICMP echo reply (ping response)
void receive_ping() {
    uint8_t buffer[1500];
    size_t len = network_receive(buffer, sizeof(buffer));

    struct ip_header *ip = (struct ip_header *)buffer;
    struct icmp_header *icmp = (struct icmp_header *)(buffer + sizeof(struct ip_header));

    if (icmp->type == 0 && icmp->code == 0) { // Echo reply
        // Process the reply (e.g., print success message)
        terminal_newline();
        terminal_writestring("[  OK  ] Attempting to ping 192.168.0.2...");
        networkinit = true;
    } else {
        terminal_newline();
        terminal_writestring("[ FAIL ] Attempting to ping 192.168.0.2...");
        terminal_newline();
        networkinit = false;

    }
    terminal_newline();
    terminal_putchar(icmp->code);
    terminal_newline();
    terminal_putchar(icmp->type);
}

void int32(uint8_t intnum, uint16_t ax, uint16_t bx, uint16_t cx, uint16_t dx, uint16_t *ax_out) {
    /* asm volatile (
        "int $0x10"
        : "=a"(*ax_out)
        : "a"(ax), "b"(bx), "c"(cx), "d"(dx)
    ); */
}

#define PAGE_SIZE 4096
#define FRAMEBUFFER_ADDRESS 0xA0000
#define FRAMEBUFFER_SIZE (VGA_HEIGHT * 600)

uint32_t page_directory[1024] __attribute__((aligned(PAGE_SIZE)));
uint32_t page_table[1024] __attribute__((aligned(PAGE_SIZE)));

void setup_paging() {
    // Initialize page directory and page table
    task("Initialize page directory and page table...", 0);
    for (int i = 0; i < 1024; i++) {
        page_directory[i] = 0x00000002; // Not present
        page_table[i] = (i * PAGE_SIZE) | 3; // Present, R/W, User
        if (!noprint) {
            printf(".");
        }
    }
    task("Initialize page directory and page table...", 1);
    // Map the framebuffer
    task("Map the framebuffer...", 0);
    for (int i = 0; i < (FRAMEBUFFER_SIZE / PAGE_SIZE); i++) {
        if (!noprint) {
            printf(".");
        }
        page_table[(FRAMEBUFFER_ADDRESS / PAGE_SIZE) + i] = (FRAMEBUFFER_ADDRESS + (i * PAGE_SIZE)) | 3;
    }
    task("Map the framebuffer...", 1);

    // Point the page directory to the page table
    task("Point the page directory to the page table...", 0);
    page_directory[0] = ((uint32_t)page_table) | 3;
    task("Point the page directory to the page table...", 1);

    // Load the page directory
    task("Load the page directory...", 0);
    load_page_directory(page_directory);
    enable_paging();
    task("Load the page directory...", 1);
}

void load_page_directory(uint32_t* page_directory) {
    asm volatile("mov %0, %%cr3" : : "r"(page_directory));
}

void enable_paging() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

void draw_pixel(int x, int y, uint8_t color) {
    uint8_t* framebuffer = (uint8_t*)0xA0000; // VGA framebuffer address
    framebuffer[y * 800 + x] = color;
}


void draw_horizontal_line(int x1, int x2, int y, uint8_t color) {
    for (int x = x1; x <= x2; x++) {
        draw_pixel(x, y, color);
    }
}

void draw_vertical_line(int y1, int y2, int x, uint8_t color) {
    for (int y = y1; y <= y2; y++) {
        draw_pixel(x, y, color);
    }
}

void draw_box(int x1, int y1, int x2, int y2, uint8_t color) {
    draw_horizontal_line(x1, x2, y1, color); // Top edge
    draw_horizontal_line(x1, x2, y2, color); // Bottom edge
    draw_vertical_line(y1, y2, x1, color);   // Left edge
    draw_vertical_line(y1, y2, x2, color);   // Right edge
}



void draw_char(uint8_t* framebuffer, int x, int y, char c) {
    // Define a simple font
    uint8_t font[256][8] = {
        // A
        {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // B
        {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // C
        {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
        // D
        {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // E
        {0x1F, 0x01, 0x01, 0x1F, 0x01, 0x01, 0x01, 0x1F},
        // F
        {0x1F, 0x01, 0x01, 0x1F, 0x01, 0x01, 0x01, 0x01},
        // G
        {0x1F, 0x01, 0x01, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // H
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
        // I
        {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
        // J
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x1F},
        // K
        {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01, 0x11},
        // L
        {0x11, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
        // M
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
        // N
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
        // O
        {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // P
        {0x1F, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01, 0x01},
        // Q
        {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // R
        {0x1F, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01, 0x11},
        // S
        {0x1F, 0x01, 0x01, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // T
        {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        // U
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // V
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01, 0x01},
        // W
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // X
        {0x11, 0x01, 0x01, 0x11, 0x11, 0x01, 0x01, 0x11},
        // Y
        {0x11, 0x11, 0x11, 0x01, 0x01, 0x01, 0x01, 0x01},
        // Z
        {0x1F, 0x01, 0x01, 0x11, 0x11, 0x11, 0x01, 0x1F},
        // [
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // \
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        // ]
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // ^
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        // _
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // `
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        // a
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // b
        {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // c
        {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
        // d
        {0x1F, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // e
        {0x1F, 0x01, 0x01, 0x1F, 0x01, 0x01, 0x01, 0x1F},
        // f
        {0x1F, 0x01, 0x01, 0x1F, 0x01, 0x01, 0x01, 0x01},
        // g
        {0x1F, 0x01, 0x01, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // h
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
        // i
        {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
        // j
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x11, 0x1F},
        // k
        {0x11, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01, 0x11},
        // l
        {0x11, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1F},
        // m
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
        // n
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11, 0x11},
        // o
        {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // p
        {0x1F, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01, 0x01},
        // q
        {0x1F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // r
        {0x1F, 0x11, 0x11, 0x1F, 0x01, 0x01, 0x01, 0x11},
        // s
        {0x1F, 0x01, 0x01, 0x1F, 0x11, 0x11, 0x11, 0x1F},
        // t
        {0x1F, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        // u
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1F},
        // v
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x01, 0x01},
        // w
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // x
        {0x11, 0x01, 0x01, 0x11, 0x11, 0x01, 0x01, 0x11},
        // y
        {0x11, 0x11, 0x11, 0x01, 0x01, 0x01, 0x01, 0x01},
        // z
        {0x1F, 0x01, 0x01, 0x11, 0x11, 0x11, 0x01, 0x1F},
        // {
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // |
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01},
        // }
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11},
        // ~
        {0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}
    };

    



    // Get the font data for the character
    uint8_t* font_data = font[c];

    // Draw the character on the screen
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (font[c][i] /* & (1 << j) */)  {
                framebuffer[(y + i) + (x + j)] = 0x30; // White
            }
        }
    }
}

void draw_string(uint8_t* framebuffer, int x, int y, char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(framebuffer, x + i * 8, y, str[i]);
    }
}

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end)
{
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void disable_cursor()
{
	outb(0x3D4, 0x0A);
	outb(0x3D5, 0x20);
}



void setup_framebuffer() {
    uint8_t* framebuffer = (uint8_t*)FRAMEBUFFER_ADDRESS;
    for (int i = 0; i < FRAMEBUFFER_SIZE; i++) {
        framebuffer[i] = 0x03; // Black
    }
}

void update_cursor(int x, int y)
{
	uint16_t pos = y * VGA_WIDTH + x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

uint16_t get_cursor_position(void)
{
    uint16_t pos = 0;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    outb(0x3D4, 0x0E);
    pos |= ((uint16_t)inb(0x3D5)) << 8;
    return pos;
}

int wait(int num) {
    int waitnum = num * 1000000;
    for (volatile int i = 0; i < waitnum; i++);
    return 0;
}

void set_vga_mode() {
    //terminal_initialize();
    task("Stop for 6000000...", 0);
    //for (volatile int i = 0; i < 6000000; i++);
    wait(6);
    task("Set VESA mode number...", 0);
    outb(0x3C4, 0x4F01);
    outb(0x3C5, 0x101); // Set VESA mode number
    task("Set VESA mode number...", 2);
    task("Switch to 800x600x16 mode...", 0);
    outb(0x3C4, 0x4F02);
    outb(0x3C5, 0x101); // Switch to 800x600x16 mode
    task("Switch to 800x600x16 mode...", 2);
    task("Change cursor", 0);
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x02);
    task("Change cursor", 1);

    // Clear the display memory
    uint8_t* framebuffer = (uint8_t*)0xB8000;
    // Ensure the framebuffer address is correct for the mode

    task("Stop for 6000000...", 0);
    wait(6);
    task("Stop for 6000000...", 1);
    for (int i = 0; i < 800 * 600; i++) {
        //for (volatile int i = 0; i < 6000; i++);
        //draw_vertical_line(i,i + 10,i + 20, 0xFF);
       //draw_string(framebuffer, 100, 100, "testaaaaaaaaaaaaaaaaaaaaaaa");
        framebuffer[i] = 0x33; // Clear to black
     //   //terminal_newline();
    }

    while (1) {
        printf(".");
        int d = 0;
        d += 1;
       framebuffer[d] = 0x00;
        //for (volatile int i = 0; i < 6000; i++);
    }
}





bool fixed;

void m() {
    terminal_initialize();
    outb(0x3C4, 0x01);
    outb(0x3C5, 0x01);
    outb(0x3C4, 0x02);
    outb(0x3C5, 0x99);
    outb(0x3C4, 0x03);
    outb(0x3C5, 0x00);
    outb(0x3C4, 0x04);
    outb(0x3C5, 0x80);
    terminal_initialize();
}

int fixflash() {
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, paniccolor); 
    terminal_buffer = (uint16_t*) 0xB8000;
    input_buffer_index = 0;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
            for (volatile int i = 0; i < 60000; i++);
        }
    }
    waitwrite = true;
    terminal_row = 6;
    terminal_newline();
    terminal_column = 6;
    terminal_writestring("Is this flashing? [Y/n]");

    uint8_t data2 = read_keyboard();
    terminal_column = 6;
    terminal_putchar(data2);
    if (data2 == 'y' || data2 == '\n') {
        paniccolor = VGA_COLOR_BLACK;
    }
}

int drawbg() {
    terminal_buffer = (uint16_t*) 0xB8000;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_color = vga_entry_color(VGA_COLOR_DARK_GREY + y, VGA_COLOR_DARK_GREY + y);
            terminal_buffer[index] = vga_entry(' ', terminal_color);
            //for (volatile int i = 0; i < 60000; i++);
        }
    }
}
int cury = 6;
int curx = 55;

int home() {

    drawbg();
    waitwrite = true;
    for (volatile int i = 0; i < 60000000; i++);
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_row = 6;
    for (int i = 0; i < 13; i++) {
        terminal_column = 11;
        terminal_writestring("                                                       ");
        terminal_newline();
        for (volatile int i = 0; i < 60000; i++);
    }
    terminal_row = 3;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BROWN); 
    terminal_newline();
    terminal_column = 10;
    terminal_writestring(" Programs                                            x ");
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
    terminal_newline();
    terminal_column = 10;
    terminal_writestring("                                                       ");
    terminal_newline();
    terminal_column = 10;	
    terminal_writestring(" Placeholder                                           ");
    terminal_newline();
    terminal_column = 10;	
    terminal_writestring(" Hello, World!                                         ");
    terminal_newline();
    for (int i = 0; i < 10; i++) {
        terminal_column = 10;
        terminal_writestring("                                                       ");
        terminal_newline();
    }
    update_cursor(55, 6);
    terminal_column = 10;
    read_keyboard();
    while (1) {
        uint8_t data = read_keyboard();
        if (data == 0) {
        }
        input_buffer[input_buffer_index] = '\0';
        if (data == 'S') {
            curx += 1;
            update_cursor(curx, cury);
        } else if (data == 'P') {
            curx -= 1;
            update_cursor(curx, cury);
        } else if (data == 'U') {
            cury -= 1;
            update_cursor(curx, cury);
        } else if (data == 'G') {
            cury += 1;
            update_cursor(curx, cury);
        }
        if (data == '\n') {
            if (cury == 4 && curx == 63) {
                welcomewind();
            }
        }
        if (data == 'c') {
            terminal_initialize();
            shell();
            home();
        }
        if (data == 'p') {
            panic("User called panic");
        }
        //terminal_putchar(data);
    }
}

int panicwind() {
    drawbg();
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_row = 7;
    for (int i = 0; i < 8; i++) {
        terminal_column = 16;
        terminal_writestring("                                                     ");
        terminal_newline();
    }
    terminal_row = 5;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BROWN); 
    terminal_newline();
    terminal_column = 15;
    terminal_writestring(" Error                                               ");
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_LIGHT_GREY);
    terminal_newline();
    terminal_column = 15;
    terminal_writestring("                                                     ");
    terminal_newline();
    terminal_column = 15;	
    terminal_writestring(" An error occurred and the system has been shutdown. ");
    terminal_newline();
    terminal_column = 15;
    for (int i = 0; i < 5; i++) {
        terminal_column = 15;
        terminal_writestring("                                                     ");
        terminal_newline();
    }
    update_cursor(curx, cury);
}

int welcomewind() {
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_row = 7;
    for (int i = 0; i < 8; i++) {
        terminal_column = 13;
        terminal_writestring("                                      ");
        terminal_newline();
    }
    terminal_row = 5;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BROWN); 
    terminal_newline();
    terminal_column = 12;
    terminal_writestring(" Welcome                            x ");
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_LIGHT_GREY);
    terminal_newline();
    terminal_column = 12;
    terminal_writestring("                                      ");
    terminal_newline();
    terminal_column = 12;	
    terminal_writestring(" Welcome to ");
    terminal_writestring(osname);
    terminal_writestring(" ");
    terminal_writestring(version);
    terminal_writestring(".");
    terminal_writestring(subversion);
    terminal_writestring("!         ");
    terminal_newline();
    terminal_column = 12;
    for (int i = 0; i < 5; i++) {
        terminal_column = 12;
        terminal_writestring("                                      ");
        terminal_newline();
    }
    update_cursor(curx, cury);
}

int displayscreen() {

    if (noprint) {
        drawbg();
        waitwrite = true;
        for (volatile int i = 0; i < 60000000; i++);
        terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        terminal_row = 6;
        for (int i = 0; i < 12; i++) {
            terminal_column = 11;
            terminal_writestring("                                                       ");
            terminal_newline();
            for (volatile int i = 0; i < 60000; i++);
        }
        terminal_row = 3;
        terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BROWN); 
        terminal_newline();
        terminal_column = 10;
        terminal_writestring(" PotatoOS Setup                                      x ");
        terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
        terminal_newline();
        terminal_column = 10;
        terminal_writestring("                                                       ");
        terminal_newline();
        terminal_column = 10;	
    } else {
        terminal_newline();
    }

    if (noprint) {
        terminal_writestring(" Do you have a black and white display? [y/N]          ");
        terminal_newline();
        for (int i = 0; i < 10; i++) {
            terminal_column = 10;
            terminal_writestring("                                                       ");
            terminal_newline();
        }
        update_cursor(55, 6);
        terminal_column = 10;
    } else {
        terminal_writestring(" Do you have a black and white display? [y/N]");
        terminal_newline();
    }
    read_keyboard();
    uint8_t data = read_keyboard();
    terminal_putchar(data);
    if (data == 0) {
    } else if (data == 'y') {
        m();
    }
}



void setup() {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x02);
    task("Running setup...", 0);
    if (noprint) {
        terminal_initialize();
    }

    displayscreen();
    if (noprint) {
        terminal_initialize();
    }
    task("Running setup...", 1);
}


void kernel_main(void) 
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x02);
    waitwrite = true;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, 23); 
    terminal_buffer = (uint16_t*) 0xB8000;

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        terminal_color = vga_entry_color(VGA_COLOR_DARK_GREY + y, VGA_COLOR_DARK_GREY + y);
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
            for (volatile int i = 0; i < 60000; i++);
        }
    }
    waitwrite = true;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    terminal_row = 6;
    for (int i = 0; i < 13; i++) {
        terminal_column = 11;
        terminal_writestring("                                                       ");
        terminal_newline();
    }
    terminal_row = 3;
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BROWN); 
    terminal_newline();
    terminal_column = 10;
    terminal_writestring(" PotatoOS                                              ");
    terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_DARK_GREY);
    terminal_newline();
    terminal_column = 10;
    terminal_writestring("                                                       ");
    terminal_newline();
    terminal_column = 10;	
    terminal_writestring(" Welcome to ");
    terminal_writestring(osname);
    terminal_writestring(" ");
    terminal_writestring(version);
    terminal_writestring(".");
    terminal_writestring(subversion);
    terminal_writestring("!                          ");
	terminal_newline();
    terminal_column = 10;
    terminal_writestring(" Press v to log, or enter to continue.                 ");
    terminal_newline();
    for (int i = 0; i < 10; i++) {
        terminal_column = 10;
        terminal_writestring("                                                       ");
        terminal_newline();
    }
    havebeeninitbefore = true;
    update_cursor(48, 7);
    terminal_row = 8;
    noprint = true;
    uint8_t data2 = read_keyboard();
    if (data2 == 'v') {
        noprint = false;
        terminal_initialize();
    }
	//terminal_initialize();
    //set_vga_mode();

    //setup();
    read_keyboard();


    //terminal_initialize();
	//terminal_row = 23;
	//terminal_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
	//terminal_writestring(" help = help   ls = list   rm = delete   cd = change dir                    ");
	//terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
	//terminal_row = 1;
	//terminal_column = 2;
    /*terminal_writestring("                                                        ");
	terminal_newline();
    terminal_writestring(" ####           ##            ##            ###    ###  ");
	terminal_newline();
    terminal_writestring(" ## ##          ##            ##           ## ##  ## ## ");
	terminal_newline();
    terminal_writestring(" ## ##   ###   ####    ####  ####    ###   ## ##  ##    ");
	terminal_newline();
    terminal_writestring(" ####   ## ##   ##    ## ##   ##    ## ##  ## ##   ###  ");
	terminal_newline();
    terminal_writestring(" ##     ## ##   ##    ## ##   ##    ## ##  ## ##     ## ");
	terminal_newline();
    terminal_writestring(" ##     ## ##   ##    ## ##   ##    ## ##  ## ##  ## ## ");
	terminal_newline();
    terminal_writestring(" ##      ###     ##    ## #    ##    ###    ###    ###  "); 
    terminal_writestring(" ");
    terminal_writestring("Kernel ");
    terminal_writestring(version);
    terminal_writestring(".");
    terminal_writestring(subversion);
    terminal_writestring("!");*/


	//terminal_color = vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK); 
	//terminal_writestring("WARNING: PotatoOS is in alpha! I am NOT responsible for ANY data loss.");

	//terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    task("Setup paging...", 0);
    setup_paging();
    task("Setup paging...", 1);
    task("Setup framebuffer...", 0);
    setup_framebuffer();
    task("Setup framebuffer...", 1);
    read_rtc();
    task("Attempting to initialize FAT...", 0);
    if (mainfat() == 0) {
        fsinit = true;
        task("Attempting to initialize FAT...", 1);
    }

    /* terminal_newline();
    terminal_writestring("[      ] Attempting to ping ???.???.???.???...");
    uint32_t dest_ip = 0xC0A80002; // 192.168.0.2
    send_ping(dest_ip);
    receive_ping(); */
	
    waitwrite = false;
    setup();
    /* if (!networkinit && !fsinit) {
        //task("Boot PotatoOS", 2);
        task("Entering recovery shell...", task_waiting);
        if (noprint) {
            terminal_initialize();
        }
        shell();
        panic("");
    } */


    terminal_newline();
    //terminal_writestring("Press 's' to enter builtin shell or enter to continue boot process: ");

    //uint8_t data = read_keyboard();
    //terminal_putchar(data);
    //if (data == 's') {
    //    shell();
    //} 

    char* config;

    if (findconfig("/sys/config") == 0) {
        home();
    } else {
        //panic("Config file invalid or not found.");
        shell();
    }


}

int findconfig(char* file) {
    return 0;
}

void shell() {
    waitwrite = false;
    task("Entering recovery shell...", 1);
    terminal_newline();
	terminal_writestring(username);
	terminal_writestring("@");
	terminal_writestring(hostname);
	terminal_writestring(" /> ");	

	while (1) {
        if (exitHKIloop == true) {
            panic("exitHLKloop");
            break;
        }
        handle_keyboard_input();
    }
}