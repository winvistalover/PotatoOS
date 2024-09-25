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




#define DISK_PORT 0x1F1
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

    // Read the root directory entries starting from the root cluster
    for (size_t i = 0; i < MAX_FILES; i++) {
        FileEntry entry;
        // Read the cluster into the entry (you will need to implement the logic to read the directory entries)
        // For example, read the first cluster of the root directory
        read_cluster(cluster, &entry, drive);
        
        // Check if the entry is valid (e.g., not deleted)
        if (entry.size > 0) {
            drive->root_directory.files[drive->root_directory.file_count++] = entry;
        }
        
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


void filesystem_initialize(uint8_t drive_number) {
    terminal_newline();
    terminal_writestring("[      ] Initialize disk support...");

    // Ensure the drive number is valid
    if (drive_number >= MAX_MOUNTED_DRIVES) {
        terminal_writestring("[ERROR] Invalid drive number.");
        return;
    }

    mount_drive(1);

    terminal_newline();
    terminal_writestring("[  OK  ] Initialize disk support...");
	
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
			//} else if (strcmp(input_buffer, "fsinit") == 0) {
			//	filesystem_initialize();
			} else if (strcmp(input_buffer, "panic") == 0) {
				panic("User called panic.");
			} else if (strcmp(input_buffer, "reboot") == 0) {
				reboot();
			} else if (strcmp(input_buffer, "shutdown") == 0) {
				shutdown();
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
	if (mount_drive(0)) {
		read_root_directory(0);
        terminal_writestring("Drive 0 mounted.");
		fsinit = true;
    }

    if (mount_drive(1)) {
        terminal_writestring("Drive 1 mounted.");
		read_root_directory(1);
		fsinit = true;
    }

    if (mount_drive(2)) {
        terminal_writestring("Drive 2 mounted.");
		read_root_directory(2);
		fsinit = true;
    }

    if (mount_drive(3)) {
        terminal_writestring("Drive 3 mounted.");
		read_root_directory(3);
		fsinit = true;
    }


	if (fsinit != true) {
		panic("FS NOT INIT!");
	}

	
	terminal_newline();
	terminal_writestring(username);
	terminal_writestring("@");
	terminal_writestring(hostname);
	terminal_writestring(" /> ");	
	while (1) {
        	handle_keyboard_input();
    	}

}