#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "FAT.h"


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
static inline char* subversion = "2.4.1";

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

bool waitwrite;


void terminal_initialize(void) 
{
	terminal_row = 1;
	//terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
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
}

void terminal_putchar(char c) 
{
	if (terminal_row == VGA_HEIGHT - 1) {
        	terminal_scroll();
    	}
    if (waitwrite) {
        for (volatile int i = 0; i < 600000; i++);
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
            if (waitwrite) {
                for (volatile int i = 0; i < 60000000; i++);
            }
            terminal_buffer[i * VGA_WIDTH + j] = terminal_buffer[(i + 1) * VGA_WIDTH + j];
        }
    }
    for (int j = 0; j < VGA_WIDTH; j++) {
        if (waitwrite) {
            for (volatile int i = 0; i < 60000000; i++);
        }
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + j] = vga_entry(' ', terminal_color);
    }
    terminal_column = 0;
}


void terminal_writestring(const char* data) 
{
	terminal_write(data, strlen(data));
}

void terminal_newline(void)
{
	terminal_row += 1;
	terminal_column = 0;
	if (terminal_row == VGA_HEIGHT - 2) {
		terminal_buffer = (uint16_t*) 0xB8000;
		for (size_t y = 0; y < VGA_HEIGHT; y++) {
			for (size_t x = 0; x < VGA_WIDTH; x++) {
				const size_t index = y * VGA_WIDTH + x;
				terminal_buffer[index] = vga_entry(' ', terminal_color);
                if (waitwrite) {
			        for (volatile int i = 0; i < 60000000; i++);
                }
			}
		}
		terminal_row = 1;
		terminal_column = 2;
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
    outb(0x43, 0x26); // Command port: Set the PIT to mode 3 (square wave generator)
    outb(0x40, divisor & 0xFF); // Low byte of divisor
    outb(0x40, (divisor >> 8) & 0xFF); // High byte of divisor
    for (volatile int i = 0; i < 600000; i++);
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

#define DISK_PORT 0x0F1
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


void shutdown() {
    terminal_newline();
    terminal_writestring("[      ] Attempting to shutdown computer...");
	terminal_newline();
	terminal_writestring("[      ] Shutdown computer using old QEMU...");
	
	outw(0xB004, 0x2000);

	terminal_newline();
	terminal_writestring("[      ] Shutdown computer using new QEMU...");

	outw(0x604, 0x2000);

	terminal_newline();
	terminal_writestring("[      ] Shutdown computer using Virtualbox...");

	outw(0x4004, 0x3400);

	terminal_newline();
	terminal_writestring("[ FAIL ] Shutdown computer...");
	terminal_newline();
	terminal_writestring("[      ] Sending hlt...");

    while (1) {
        asm volatile ("hlt");
    }
}

void reboot() {
    // Disable interrupts
	terminal_newline();
	terminal_writestring("[      ] Reboot computer...");
    asm volatile ("cli");

    // Use the BIOS interrupt to reboot
    asm volatile (
        "movb $0xFE, %al\n"  // Set the reset command
        "outb %al, $0x64\n"  // Send the command to the keyboard controller
    );

    // Hang the CPU if the reboot command fails
	terminal_newline();
	terminal_writestring("[ FAIL ] Restart computer...");
	terminal_newline();
	terminal_writestring("[      ] Sending hlt...");
    while (1) {
        asm volatile ("hlt");
    }
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
				if (fsinit != true) {
					panic("FS NOT INIT!");
				} else {
					read_root_directory(3);
					//printf(mounted_drives[1].fs.root_directory.file_count);
					//for (size_t i = 0; i < mounted_drives[1].fs.root_directory.file_count; i++) {
					//	terminal_writestring("File or dir: ");
            		//	terminal_writestring(mounted_drives[1].fs.root_directory.files[i].name);
					//	terminal_newline();
        			//}
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
			} else if (strcmp(input_buffer, "help") == 0) {
				terminal_newline();
				printf("PotatoOS ");
				printf(version);
				printf(".");
				printf(subversion);
				terminal_newline();
				terminal_newline();
				printf("help        - shows this message.");
				terminal_newline();
				printf("clear       - clears the screen.");
				terminal_newline();
				printf("panic       - kernel panic.");
				terminal_newline();
				printf("reboot      - reboot.");
				terminal_newline();
				printf("g           - set some stuff up.");
				terminal_newline();
				printf("fatinit     - initialize FAT.");
				terminal_newline();
				printf("shutdown    - shutdown.");
				terminal_newline();
				printf("color       - shows colors.");
				terminal_newline();
				printf("ls          - list.");
				terminal_newline();
				printf("waitwrite   - Waits to write text.");
			} else if (strcmp(input_buffer, "clear") == 0) {
				terminal_buffer = (uint16_t*) 0xB8000;
				for (size_t y = 0; y < VGA_HEIGHT; y++) {
					for (size_t x = 0; x < VGA_WIDTH; x++) {
						const size_t index = y * VGA_WIDTH + x;
						terminal_buffer[index] = vga_entry(' ', terminal_color);
					}
				}
				terminal_row = 1;
				//terminal_column = 2;
			//} else if (strcmp(input_buffer, "fsinit") == 0) {
			//	filesystem_initialize();
			} else if (strcmp(input_buffer, "panic") == 0) {
				panic("User called panic.");
			} else if (strcmp(input_buffer, "reboot") == 0) {
				reboot();
            } else if (strcmp(input_buffer, "g") == 0) {
                outb(0x3D4, 0x0A);
                outb(0x3D5, 0x02);
                set_vga_mode();
            } else if (strcmp(input_buffer, "fatinit") == 0) {
                terminal_newline();
                terminal_writestring("[      ] Attempting to initialize FAT...");
                if (FATInitialize() == 0) {
                    fsinit = true;
                } else {
                    fsinit = false;
                }
            } else if (strcmp(input_buffer, "waitwrite") == 0) {
                waitwrite = true;
			} else if (strcmp(input_buffer, "shutdown") == 0) {
				shutdown();
            } else if (strcmp(input_buffer, "color") == 0) {
                outb(0x3D4, 0x0A);
                outb(0x3D5, 0x20);
                for (size_t y = 0; y < VGA_HEIGHT; y++) {
                    for (size_t x = 0; x < VGA_WIDTH; x++) {
                        const size_t index = y * VGA_WIDTH + x;
                        terminal_buffer[index] = vga_entry(' ', terminal_color);
                        for (volatile int i = 0; i < 60000; i++);
                        terminal_color = vga_entry_color(VGA_COLOR_BLACK, x); 
                    }
                }
                terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK); 
                outb(0x3D4, 0x0A);
                outb(0x3D5, 0x02);
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

#define RAM_START 0x00000000 
#define RAM_END   0x00003030
#define DUMP_SIZE 4

bool networkinit;

void panic(char* msg) 
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
	terminal_writestring("PANIC!!!");
	terminal_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE); 
    terminal_buffer = (uint16_t*) 0xB8000;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
			for (volatile int i = 0; i < 600000; i++);
        }
    }
    waitwrite = true;
    terminal_row = 1;
    terminal_newline();
	terminal_writestring("An error occurred and the system has been shutdown."); // ***The cake is a lie***
    terminal_newline();
	terminal_writestring("Contact me at ospotato3@gmail.com with the error message below, and anything you where doing before this happened.");
    terminal_newline();
    terminal_newline();
	terminal_writestring("Panic: ");
	terminal_writestring(msg);
	terminal_newline();
	terminal_writestring("Kernel: ");
	terminal_writestring(osname);
	terminal_writestring(" ");
	terminal_writestring(version);
	terminal_writestring(".");
	terminal_writestring(subversion);
	terminal_newline();
    terminal_newline();
    terminal_newline();
	terminal_writestring("The system will automatically reboot.");
    for (volatile int i = 0; i < 600000000; i++);
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
    }
    return ~sum;
}

// Function to send data through the network controller
void network_send(uint8_t *packet, size_t length) {
    // Example I/O port addresses for the Intel PRO/100 VE
    uint16_t io_base = 0x1000; // This should be the actual I/O base address of your network card

    // Write the packet to the transmit buffer
    for (size_t i = 0; i < length; i++) {
        outb(io_base + i, packet[i]);
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

void set_vga_mode() {
    // Set VGA mode to 80x25 text mode
    //outb(0x3D4, 0x0A); outb(0x3D5, 0x20); // Cursor start
    //outb(0x3D4, 0x0B); outb(0x3D5, 0x0F); // Cursor end
    outb(0x3D4, 0x0C); outb(0x3D5, 0x00); // Start address high
    outb(0x3D4, 0x0D); outb(0x3D5, 0x00); // Start address low
    outb(0x3D4, 0x09); outb(0x3D5, 0x13); // Maximum scan line
    outb(0x3D4, 0x14); outb(0x3D5, 0x20); // Underline location
    outb(0x3D4, 0x07); outb(0x3D5, 0xFFFFF); // Vertical display end
    //outb(0x3D4, 0x12); outb(0x3D5, 0xFFFFF); // Vertical retrace start
    outb(0x3D4, 0x17); outb(0x3D5, 0xA3); // Mode control
}

void kernel_main(void) 
{
	/* Initialize terminal interface */
	terminal_initialize();
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x02);




	//terminal_row = 23;
	//terminal_color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE);
	//terminal_writestring(" help = help   ls = list   rm = delete   cd = change dir                    ");
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
	terminal_row = 1;
	//terminal_column = 2;
    terminal_writestring("Welcome to ");
    terminal_writestring(osname);
    terminal_writestring(" ");
    terminal_writestring(version);
    terminal_writestring(".");
    terminal_writestring(subversion);
    terminal_writestring("!");
	terminal_newline();
	terminal_color = vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK); 
	terminal_writestring("WARNING: PotatoOS is in alpha! I am NOT responsible for ANY data loss.");

	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
	terminal_newline();	
    terminal_newline();
    terminal_writestring("[      ] Attempting to initialize FAT...");
    //if (FATInitialize() == 0) {
    //    fsinit = true;
    //}
    
    fsinit = true; // FAT driver is broken. Set fsinit to true.
    
    terminal_newline();
    terminal_writestring("[      ] Attempting to ping ???.???.???.???...");
    uint32_t dest_ip = 0xC0A80002; // 192.168.0.2
    send_ping(dest_ip);
    receive_ping();
	

	terminal_newline();
	terminal_writestring(username);
	terminal_writestring("@");
	terminal_writestring(hostname);
	terminal_writestring(" /> ");	
	while (1) {
            if (!networkinit && !fsinit) {
                panic("Kernel couldn't load necessary modules!");
            }
        	handle_keyboard_input();
    	}

}