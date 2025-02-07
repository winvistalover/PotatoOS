#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define NO_ERROR 0
#define FILE_OPEN_ERROR -1
#define DATA_READ_ERROR -2
#define DATA_WRITE_ERROR -3
#define DATA_MISMATCH_ERROR -4



#include <kernel/tty.h>

#include "vga.h"


#define KBD_CTRL_PORT 0x64
#define KBD_DATA_PORT 0x60

#define PCI_CONFIG_ADDRESS 0x0CF8  // PCI Configuration Address Register
uint16_t PCI_CONFIG_DATA = 0x0CFC;     // PCI Configuration Data Register




static size_t VGA_WIDTH = 80;
static size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

static inline unsigned int inl(unsigned short port) {
    unsigned int result;
    __asm__ __volatile__("inl %1, %0" : "=a"(result) : "d"(port));
    return result;
}

static inline void outl(unsigned int value, unsigned short port) {
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "d"(port));
}

unsigned int read_pci_config(unsigned int address) {
    outl(0x80000000 | (address & 0xFFFFFFFC), PCI_CONFIG_ADDRESS);
    return inl(PCI_CONFIG_DATA);
}
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
bool noprint;

#include <stdint.h>
#define UART0_BASE 0x3F8

volatile uint32_t * const UART0DR = (uint32_t *)(UART0_BASE + 0x00);
volatile uint32_t * const UART0FR = (uint32_t *)(UART0_BASE + 0x18);
volatile uint32_t * const UART0IBRD = (uint32_t *)(UART0_BASE + 0x24);
volatile uint32_t * const UART0FBRD = (uint32_t *)(UART0_BASE + 0x28);
volatile uint32_t * const UART0LCRH = (uint32_t *)(UART0_BASE + 0x2C);
volatile uint32_t * const UART0CR = (uint32_t *)(UART0_BASE + 0x30);
void uart_init() { 
    *UART0CR = 0x00000000;
    *UART0IBRD = 1;   
    *UART0FBRD = 40;   
    *UART0LCRH = (1 << 5) | (1 << 6); 
    *UART0CR = (1 << 0) | (1 << 8) | (1 << 9);
}
bool isuava = true;
bool isuart = false;
void uart_putc(char c) {
    if (isuava == true) {
        int timeout = 0;
        while (*UART0FR & (1 << 5) == 0) {
            if (timeout == 5) {
                isuava = false;
                return;
            }
            timeout++;
            terminal_color = 7;
            terminal_writestring(".");
            wait(600000000000);
        }
        outb(UART0DR, c);
    } else {
        isuart = false;
    }
}

void uart_puts(const char *str) {
    while (*str) {
        uart_putc(*str++);
    }
}

bool havebeeninitbefore;

int detect_pci(unsigned int address) {
    unsigned int vendor_id;
    unsigned int pci_class_code;
    unsigned int pci_subclass_code;
    char venstr[16]; 
    char adstr[16]; 
    char classstr[16]; 

    PCI_CONFIG_DATA = 0x0CFC;
    vendor_id = read_pci_config(address);
    
    if (vendor_id == 0) {
        return 1; 
    }
    
    PCI_CONFIG_DATA = 0x08;
    pci_class_code = read_pci_config(address);
    
    PCI_CONFIG_DATA = 0x09;
    pci_subclass_code = read_pci_config(address);

    terminal_newline();
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_writestring("PCI: Get PCI ");
    intToStr(address, adstr);
    terminal_writestring(adstr);
    terminal_writestring(": ");
    intToStr(vendor_id, venstr);
    terminal_writestring(venstr);
    terminal_newline();
    terminal_writestring(" => Device Type: ");
    if (pci_class_code == 0x02) { 
        switch (pci_subclass_code) {
            case 0x00:
                terminal_writestring("Ethernet Controller");
                break;
            case 0x01:
                terminal_writestring("Token Ring Controller");
                break;
            case 0x02:
                terminal_writestring("FDDI Controller");
                break;
            case 0x03:
                terminal_writestring("ATM Controller");
                break;
            default:
                terminal_writestring("Network Controller (Unknown subclass)");
                break;
        }
    } else if (pci_class_code == 0x01) { 
        switch (pci_subclass_code) {
            case 0x01:
                terminal_writestring("IDE Controller");
                break;
            case 0x02:
                terminal_writestring("SCSI Controller");
                break;
            case 0x04:
                terminal_writestring("RAID Controller");
                break;
            case 0x06:
                terminal_writestring("Non-Volatile Memory Controller");
                break;
            default:
                terminal_writestring("Mass Storage Controller (Unknown subclass)");
                break;
        }
    } else if (pci_class_code == 0x03) { 
        terminal_writestring("VGA-Compatible Controller");
    } else if (pci_class_code == 0x0C) { 
        switch (pci_subclass_code) {
            case 0x03:
                terminal_writestring("USB Controller");
                break;
            case 0x01:
                terminal_writestring("FireWire (IEEE 1394) Controller");
                break;
            default:
                terminal_writestring("Serial Bus Controller (Unknown subclass)");
                break;
        }
    } else {
        terminal_writestring("Unknown Device Type");
    }

    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    terminal_column = 75;
    terminal_writestring("OKAY");
    
    return 0;
}

static inline char* osname = "Mango";
static inline char* version = "0";
static inline char* subversion = "2.6.0";

static inline char* username = "mango";
static inline char* hostname = "cd";

#define MAX_EVENTS 10000

char* eventlog[MAX_EVENTS];
int size = 1;

enum taskstate {
    task_waiting = 0,
    task_ok = 1,
    task_failed = 2,
};

void addevent(char* new_event, int state) {
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
    addevent(name, state);
    if (!noprint) {
        terminal_newline();
        //terminal_column = 10;
        if (state == 0) {
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            terminal_writestring(name);
            terminal_color = vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
            terminal_column = 75;
            terminal_writestring("WAIT");
            terminal_column = 0;
            uart_puts("\nKERNEL FUNCTION TASK: WAIT ");
            uart_puts(name);
        } else if (state == 1) {
            //terminal_writestring("[  ");
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            //terminal_writestring("  ] ");
            terminal_writestring(name);
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            terminal_column = 75;
            terminal_writestring("OKAY");
            uart_puts("\nKERNEL FUNCTION TASK: OKAY ");
            uart_puts(name);
        } else if (state == 2) {
            //terminal_writestring("[ ");
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
            //terminal_writestring(" ] ");
            terminal_writestring(name);
            terminal_color = vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
            terminal_column = 75;
            terminal_writestring("FAIL");
            uart_puts("\nKERNEL FUNCTION TASK: FAIL ");
            uart_puts(name);
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


    for (uint16_t i = 0; i < 0x2001; i++) {
        outw(0x230, i);
    }
    printf("Press the power button to shutdown");
    asm volatile ("cli");
}

void reboot() {
    task("Reboot computer...", 0);
    asm volatile ("cli");
    asm volatile (
        "movb $0xFE, %al\n"  
        "outb %al, $0x64\n" 
    );

    task("Reboot computer...", 2);
    task("Sending hlt...", 0);
    while (1) {
        asm volatile ("hlt");
    }
}

char input_buffer[1283256];
size_t input_buffer_index = 0;

bool waitwrite;


int mainfat() {
    task("No FAT FS support yet.", 2);
    return 1;
}


void terminal_initialize(void) 
{
    if (isuart) {
        uart_puts("\nRunning UART, terminal_initialize skipped.");
        return;
    }
	terminal_row = 0;
	terminal_column = 0;
    if (!havebeeninitbefore) {
        terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
	terminal_buffer = (uint16_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			
			terminal_buffer[index] = vga_entry(' ', terminal_color);
			if (waitwrite) {
				for (volatile int i = 0; i < 90000; i++);
			}
		}
	}


	terminal_buffer = (uint32_t*) 0xB8000;
	for (size_t y = 0; y < VGA_HEIGHT - 20; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
            		if (waitwrite) {
	             		for (volatile int i = 0; i < 90000; i++);
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
        if (isuart != true) 
		  terminal_putchar(data[i]);
        else
            uart_putc(data[i]);
        if (waitwrite) {
            for (volatile int i = 0; i < 600000; i++);
        }
}

void terminal_scroll() {
    if (!isuart) {
        for (int i = 0; i < VGA_HEIGHT - 1; i++) {
            for (int j = 0; j < VGA_WIDTH; j++) {
                terminal_buffer[i * VGA_WIDTH + j] = terminal_buffer[(i + 1) * VGA_WIDTH + j];
             }
        }
    }
    terminal_column = 0;
    terminal_row -= 1;
}


void terminal_writestring(const char* data) 
{
    if (!isuart) {
	   terminal_write(data, strlen(data));
    } else {
        uart_puts(data);
    }
}

void terminal_newline(void)
{
	terminal_row += 1;
	terminal_column = 0;
	if (terminal_row == VGA_HEIGHT - 1) {
		terminal_scroll();
	}
    if (isuart) 
        uart_putc('\n');
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

 //Play sound using built-in speaker
 static void play_sound(uint32_t nFrequence) {
    uint32_t Div;
    uint8_t tmp;
 
        //Set the PIT to the desired frequency
    Div = 1193180 / nFrequence;
    outb(0x43, 0xb6);
    outb(0x42, (uint8_t) (Div) );
    outb(0x42, (uint8_t) (Div >> 8));
 
        //And play the sound using the PC speaker
    tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
 }
 
 //make it shut up
 static void nosound() {
    uint8_t tmp = inb(0x61) & 0xFC;
     
    outb(0x61, tmp);
 }
 
 //Make a beep
 void beep() {
     play_sound(1000);
     wait(100000000000000);
     play_sound(1300);
     wait(300000000000000);
     nosound();
          //set_PIT_2(old_frequency);
 }


void wait2(unsigned int ms) {
    // Simple busy-wait function to create a delay in milliseconds
    for (unsigned int i = 0; i < ms * 1000; i++) {
        // Busy-wait loop
    }
}




#define MAX_FILES 128
#define MAX_FILENAME_LENGTH 32
#define BLOCK_SIZE 512
#define MAX_BLOCKS 1024
#define MAX_MOUNTED_DRIVES 25


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
        task("Invalid drive number", 2);
        return false; // Invalid drive number
    }

    MountedDrive* drive = &mounted_drives[drive_number];
    if (drive->is_mounted) {
        task("Drive already mounted", 2);
        return false; // Drive already mounted
    }


    // Read the boot sector
    read_boot_sector(drive_number);
    
    // Initialize the FAT32 structure
    initialize_fat32(drive_number);

    drive->is_mounted = true;

    task("Drive mounted successfully.", 1);
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
    //while (!(inb(DISK_PORT + 7) & 0x08)); // Wait for DRQ

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


char* drives[3];

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
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int shift_pressed = 0;

uint16_t read_keyboard() {
    uint16_t data;
    while ((inb(0x64) & 0x01) == 0) {
        // Wait for data to be available
    }
    data = inb(0x60);
    outb(0xED, 0);

    if (data == 0xED) { // 0xED is the prefix for some extended keys
        // Read the next byte
        while ((inb(0x64) & 0x01) == 0) {
            // Wait for data to be available
        }
        data = inb(0x60);
    }

    if (data == 0x45) { // Num Lock key
        task("num lock key pressed", 1);
    } else if (data == 0x36 || data == 0x2A || data == 0x3A) { // Right Shift key 
        if (data & 0x80) {
            shift_pressed = 0;
        } else {
            if (shift_pressed) {
                shift_pressed = 0;
            } else {
                shift_pressed = 1;
            }
        }
        return 0;
    }

    if (data & 0x80) {
        return 0;
    }
    
    if (data < sizeof(keyboard_layout)) {
        char c = keyboard_layout[data];
        if (shift_pressed && (c >= 'a' && c <= 'z')) {
            c -= 32; // Convert to uppercase
        }
        return c;
    }
    
    return 0;
}


char* fakeroot[];

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
      // task("Read RTC...", 0);
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
      // task("Read RTC...", 1);
      /* terminal_putchar(second);
      terminal_putchar(minute);
      terminal_putchar(hour);
      terminal_putchar(month);*/
}

extern void initTasking();

typedef struct {
    uint32_t eax, ebx, ecx, edx, esi, edi, esp, ebp, eip, eflags, cr3;
} Registers;

typedef struct Task {
    Registers regs;
    struct Task *next;
} Task;



    

void handasm() {
    read_keyboard();
    char* asmmsg = "*";
    terminal_newline();
    char input_buffer2[1283256];
    printf(asmmsg);
    while (1) {
        uint8_t data = read_keyboard();
        if (data == 0) {
            continue;
        }
        input_buffer_index = 0;
        input_buffer2[input_buffer_index] = '\0';
        if (data == '\b') {
            if (terminal_column > strlen(asmmsg)) {
                terminal_column--;
                input_buffer_index--;
                terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
                update_cursor(terminal_column, terminal_row);
            }
        } else {
            if (data == '\n') {
                if (strcmp(input_buffer2, "quit") == 0) {
                    printf("hasdasd");
                    break;
                }
                terminal_newline();
                printf(asmmsg);
                input_buffer_index = 0;
            } else {
                if (input_buffer_index < sizeof(input_buffer2) - 1) {
                    input_buffer2[input_buffer_index++] = data;    
                    if (data == '`') {
                        break;
                    }
                    terminal_putchar(data);
                }
            }
        }
    }
}

void handle_keyboard_input() {
    uint8_t data = read_keyboard();
    if (data == 0) {
        return;
    }
    input_buffer[input_buffer_index] = '\0';
    if (data == '\b') {
        if (terminal_column > strlen(username) + strlen(hostname) + 5) {
            terminal_column--;
            input_buffer_index--;
            terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
            update_cursor(terminal_column, terminal_row);
        
            if (isuart) {
                uart_puts("\b");
            }
        }
    } else {
        if (data == '%') {
            task("Set VGA mode", 1);
            set_vga_mode();
        } else if (data == '^') {
            terminal_initialize();
            terminal_row--;
            update_cursor(0,0);
            task("Reset terminal", 1);
        } else if (data == '&') {
            terminal_initialize();
            task("Waiting for command [F1: reboot() | F2: shutdown() | F3: ide_detect_drives() | F4: detect_pci()]", 1);
            read_keyboard();
            uint8_t data2 = read_keyboard();
            while (data2 != 0) {
                if (data2 == '%') {
                    reboot();
                    data2 = '\0';
                } else if (data2 == '^') {
                    shutdown();
                    data2 = '\0';
                } else if (data2 == '&') {
                    task("Detecting ATA devices...",0);
                    ide_detect_drives();
                    task("Detecting ATA devices...",1);
                    data2 = '\0';
                } else if (data2 == '*') {
                    task("Detecting PCI devices...",0);
                    for (int i; i < 255; i++) {
                        detect_pci(i);
                    }
                    task("Detecting PCI devices...",1);
                    data2 = '\0';
                } else if (data2 == '`') {
                    break;
                }
            }
        } else if (data == '\n') {
            if (strcmp(input_buffer, "ls") == 0) {
				terminal_newline();
				if (fsinit != true) {
                    for (int i = 0; i < sizeof(fakeroot[0]); i++) {
            			if (fakeroot[i] == 0) {
            				continue;
            			}
                        printf("%s ", fakeroot[i]);
                    }
				}
            } else if (strcmp(input_buffer, "vga2urt") == 0) {
                terminal_initialize();
                printf("Output redirected to UART!");
                printf("%nIf you don't see any output on COM1, enter the command 'urt2vga'.");
                isuart = true;
            } else if (strcmp(input_buffer, "urt2vga") == 0) {
                isuart = false;
           } else if (strcmp(input_buffer, "loop") == 0) {
    		uint16_t i;
    		while (i < 100) {
    			i++;
    			terminal_newline();
    			printf("hello, world!");
    		}
    	   } else if (strcmp(input_buffer, "le") == 0) {
                    for (int i = 0; i < size; i++) {
            		    if (eventlog[i] == 0) {
            		      continue;	
                        } 
                        terminal_newline();
                        printf("%s ", eventlog[i]);
                    }
            } else if (strcmp(input_buffer, "lsd") == 0) {
                printf("%n");
                for (int i = 0; i < sizeof(drives) / sizeof(drives[0]); i++) {
                    if (drives[i] == 0) {
        			     continue;
        		    }
                    printf("%n",drives[i]);
                }
            } else if (strncmp(input_buffer, "echo ", 5) == 0) {
                char* content = strtok(input_buffer + 5, " ");
                printf("%s",content);
            } else if (strcmp(input_buffer, "help") == 0) {
				terminal_newline();
				printf("PotatoOS %s.",version);
				printf("%s%n%n",subversion);
                printf("ls                  => Lists nodes in the current directory.%n");
                printf("vga2urt             => Redirects the output to COM1.%n");
                printf("urt2vga             => Redirects the output to VGA.%n");
                printf("loop                => Prints 'Hello, World!' 100 times.%n");
                printf("le                  => List kernel events.%n");
                printf("echo                => Print text to the terminal%n");
                printf("lsd                 => List drives.%n");
                printf("help                => Shows this message.%n");
                printf("clear               => Clears the screen.%n");
                printf("panic               => Kernel panics.%n");
                printf("lsd                 => List drives.%n");
                printf("reboot              => Reboots the computer.%n");
                printf("shutdown            => Turns off the computer.%n");
                printf("rk                  => Restarts the kernel.%n");
                printf("waitwrite           => Waits before outputting a char.%n");
                printf("datetime            => Prints the date and time.%n");
                printf("beep                => Uses the PC beeper.%n");
                
            } else if (strcmp(input_buffer, "beep") == 0) {
                beep();
			} else if (strcmp(input_buffer, "clear") == 0) {
	            terminal_initialize();
                terminal_row--;
			} else if (strcmp(input_buffer, "panic") == 0) {
				panic("User called panic.");
            } else if (strcmp(input_buffer, "reboot") == 0) {
				reboot();
                panic("Fail to reboot");
            } else if (strcmp(input_buffer, "rk") == 0) {
                havebeeninitbefore = false;
                kernel_main();
            } else if (strcmp(input_buffer, "waitwrite") == 0) {
                waitwrite = true;
			} else if (strcmp(input_buffer, "shutdown") == 0) {
				shutdown();
            } else if (strcmp(input_buffer, "datetime") == 0) {
                read_rtc();
                char yearstr;
                char monthstr;
                char hourstr;
                char minutestr;
                char secondstr;
                char daystr;
                printf("%n[m/d/y]: ");
                intToStr(month, monthstr);
                printf("/%s",monthstr);
                intToStr(day, daystr);
                printf(daystr);
                intToStr(year, yearstr);
                printf("/%s", yearstr);
                intToStr(hour, hourstr);
                printf(" at [h/m] %s",hourstr);
                intToStr(minute, minutestr);
                printf(":%s",minutestr);
            } else if (input_buffer_index != 0) {
				char* command = input_buffer;
                printf("%n'%s' unknown command or file name.", input_buffer); 
            }
            printf("%n%s",username);
            printf("@%s /> ",hostname);
            input_buffer_index = 0; // Reset input buffer index
        } else {
            if (input_buffer_index < sizeof(input_buffer) - 1) {
                input_buffer[input_buffer_index++] = data;
                if (isuart) {
                    uart_putc(data);
                } else {
                    terminal_putchar(data);
                }
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
    beep();
    outb(0xED, KBD_CTRL_PORT);
    outb(0x01, KBD_DATA_PORT);
    outb(0xED, KBD_CTRL_PORT);
    outb(0x00, KBD_DATA_PORT);
    outb(0xED, KBD_CTRL_PORT);
    outb(0x02, KBD_DATA_PORT);
    //terminal_initialize();
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK); 
    terminal_buffer = (uint16_t*) 0xB8000;
    input_buffer_index = 0;
    
    //waitwrite = true;
    terminal_newline();
	//terminal_writestring(""); // ***The cake is a lie***
    //terminal_newline();
	//terminal_writestring("Contact me at ospotato3@gmail.com with the error message below.");
    //terminal_newline();
	//terminal_writestring("Github: https://github.com/winvistalover/PotatoOS/tree/nightly");
    //terminal_newline();
    //terminal_newline();
    uart_puts("\nKERNEL FUNCTION PANIC:");
    terminal_writestring("Panic: ");
    terminal_writestring(msg);
    uart_puts("\nPanic: ");
    uart_puts(msg);
	terminal_newline();
    terminal_writestring("Kernel: ");
    terminal_writestring(osname);
    terminal_writestring(" ");
    terminal_writestring(version);
    terminal_writestring(".");
    terminal_writestring(subversion);
    uart_puts("\nKernel: ");
    uart_puts(osname);
    uart_puts(" ");
    uart_puts(version);
    uart_puts(".");
    uart_puts(subversion);
	terminal_newline();
    //terminal_newline();
    //terminal_newline();
    //terminal_writestring("The system will stop.");
    //terminal_newline();
    terminal_writestring("Press any key to reboot...");
    uint8_t data = read_keyboard();
    data = read_keyboard();
    while (data != 0) {
        reboot();
    }
    /*
    terminal_newline();
    terminal_row = VGA_HEIGHT - 3;
    terminal_column = 0;
    terminal_writestring("Timeout:");
    terminal_column = 0;
    terminal_row = VGA_HEIGHT - 2;
	terminal_color = vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_WHITE);  */   

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


// Host-to-network byte order conversion (htonl, htons)
uint32_t htonl(uint32_t hostlong) {
    return ((hostlong & 0xFF000000) >> 24) |
           ((hostlong & 0x00FF0000) >> 8)  |
           ((hostlong & 0x0000FF00) << 8)  |
           ((hostlong & 0x000000FF) << 24);
}

uint16_t htons(uint16_t hostshort) {
    return (hostshort >> 8) | (hostshort << 8);
}

// Network structures
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

uint16_t checksum(void *data, size_t len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;
    
    // Add 16-bit words to sum
    for (size_t i = 0; i < len / 2; i++) {
        sum += *ptr++;
    }

    // If odd byte, add the remaining byte
    if (len % 2) {
        sum += *(uint8_t *)ptr;
    }

    // Handle overflow by wrapping around and adding
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return ~sum; // Return the one's complement of the sum
}


// Function to send data through the network controller (example placeholder)
void network_send(uint8_t *packet, size_t length) {
    uint16_t io_base = 0xB8000;  // Adjust to your network card's I/O base address
    
    // Write the packet to the transmit buffer
    for (size_t i = 0; i < length; i++) {
        outb(io_base + i, packet[i]);
    }

    // Trigger transmission
    outb(io_base + 0x10, 0x01);  // Example command to initiate packet transmission
}

// Function to receive data from the network controller
size_t network_receive(uint8_t *buffer, size_t max_length) {
    uint16_t io_base = 0x1500;  // Adjust to your network card's I/O base address
    
    // Check if data is available
    if (inb(io_base + 0x20) & 0x01) {  // Example status register check
        size_t length = inb(io_base + 0x24);  // Example length register
        
        if (length > max_length) {
            length = max_length;
        }

        // Read the packet from the receive buffer
        for (size_t i = 0; i < length; i++) {
            buffer[i] = inb(io_base + i);
        }

        return length;
    }

    return 0;  // No data received
}

void send_ping(uint32_t dest_ip) {
    /*struct ip_header ip;
    struct icmp_header icmp;
    uint8_t packet[sizeof(ip) + sizeof(icmp)];

    // Fill IP header
    ip.version_ihl = 0x45; // IPv4, 5 32-bit words (no options)
    ip.type_of_service = 0;
    ip.total_length = htons(sizeof(ip) + sizeof(icmp)); // Total length of IP + ICMP header
    ip.identification = 0;
    ip.flags_fragment_offset = 0;
    ip.time_to_live = 64;
    ip.protocol = 0x01; // ICMP protocol
    ip.header_checksum = 0;
    ip.source_address = htonl(0xC0A80001); // 192.168.0.1
    ip.destination_address = honl(dest_ip);
    ip.header_checksum = checksum(&ip, sizeof(ip));  // Calculate checksum for IP header

    icmp.type = 8; // Echo request
    icmp.code = 0;
    icmp.checksum = 0;  // Will calculate this after filling
    icmp.identifier = htons(1);
    icmp.sequence_number = htons(1);

    // Calculate ICMP checksum
    icmp.checksum = checksum(&icmp, sizeof(icmp));

    // Copy headers to packet
    memcpy(packet, &ip, sizeof(ip));
    memcpy(packet + sizeof(ip), &icmp, sizeof(icmp));

    // Print to confirm protocol is set correctly
    terminal_writestring("[ DEBUG ] Sending Ping to: ");
    terminal_putchar((dest_ip >> 24) & 0xFF); // Print the destination IP
    terminal_putchar((dest_ip >> 16) & 0xFF);
    terminal_putchar((dest_ip >> 8) & 0xFF);
    terminal_putchar(dest_ip & 0xFF);
    terminal_newline();

    // Double-check total packet length
    uint32_t packet_size = sizeof(ip) + sizeof(icmp);
    terminal_writestring("[ DEBUG ] Packet Size: ");
    terminal_putchar((packet_size >> 24) & 0xFF); // High byte
    terminal_putchar((packet_size >> 16) & 0xFF);
    terminal_putchar((packet_size >> 8) & 0xFF);
    terminal_putchar(packet_size & 0xFF); // Low byte
    terminal_newline();

    // Copy the IP and ICMP headers into the packet
    memcpy(packet, &ip, sizeof(ip));
    memcpy(packet + sizeof(ip), &icmp, sizeof(icmp));

    // Send the packet via network
    network_send(packet, sizeof(packet));*/
}


uint32_t ntohl(uint32_t netlong) {
    return ((netlong & 0x000000FF) << 24) | 
           ((netlong & 0x0000FF00) << 8)  |
           ((netlong & 0x00FF0000) >> 8)  |
           ((netlong & 0xFF000000) >> 24);
}


// Function to receive ICMP echo reply (ping response)
void receive_ping() {
    uint8_t buffer[1500];
    size_t len = network_receive(buffer, sizeof(buffer));  // Receive data from network

    if (len > 0) {
        struct ip_header *ip = (struct ip_header *)buffer;
        struct icmp_header *icmp = (struct icmp_header *)(buffer + sizeof(struct ip_header));

        // Check if the received packet is at least large enough for the IP and ICMP headers
        if (len < sizeof(struct ip_header) + sizeof(struct icmp_header)) {
            terminal_newline();
            terminal_writestring("[ ERROR ] Packet too short for ICMP");
            return;
        }

        // Debugging output to see the protocol field
        terminal_newline();
        terminal_writestring("[ DEBUG ] Received packet: ");
        for (size_t i = 0; i < len && i < 16; i++) {  // Print up to 16 bytes for debugging
            terminal_putchar(buffer[i]);
            if (i < len - 1) terminal_putchar(' ');
        }

        terminal_newline();
        terminal_writestring("[ DEBUG ] IP Protocol: ");
        terminal_putchar((ip->protocol / 10) + '0');  // Print first digit
        terminal_putchar((ip->protocol % 10) + '0');  // Print second digit

        // Validate the IP header protocol (should be ICMP, protocol number 1)
        if (ip->protocol != 1) {
            terminal_newline();
            terminal_writestring("[ ERROR ] Invalid protocol in IP header");
            return;
        }

        // ICMP checksum validation
        uint16_t calculated_checksum = checksum(icmp, sizeof(struct icmp_header));
        if (icmp->checksum != calculated_checksum) {
            terminal_newline();
            terminal_writestring("[ ERROR ] ICMP checksum mismatch");
            return;
        }

        // Check for ICMP Echo Reply (type 0, code 0)
        if (icmp->type == 0 && icmp->code == 0) {  // Echo reply
            // Print success message and relevant info
            terminal_newline();
            terminal_writestring("[  OK  ] Ping successful: ");
            
            uint32_t source_ip = ntohl(ip->source_address);
            uint8_t byte1 = (source_ip >> 24) & 0xFF;
            uint8_t byte2 = (source_ip >> 16) & 0xFF;
            uint8_t byte3 = (source_ip >> 8) & 0xFF;
            uint8_t byte4 = source_ip & 0xFF;

            // Print the source IP address
            terminal_putchar(byte1 + '0');
            terminal_putchar('.');
            terminal_putchar(byte2 + '0');
            terminal_putchar('.');
            terminal_putchar(byte3 + '0');
            terminal_putchar('.');
            terminal_putchar(byte4 + '0');
            terminal_newline();
        } else {
            terminal_newline();
            terminal_writestring("[ FAIL ] Received invalid ICMP Echo reply");
        }
    } else {
        terminal_newline();
        terminal_writestring("[ ERROR ] No response received or timeout");
    }
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


#define PAGE_SIZE 4096
#define NUM_PAGES 1024

unsigned int page_directory[NUM_PAGES] __attribute__((aligned(PAGE_SIZE)));
unsigned int first_page_table[NUM_PAGES] __attribute__((aligned(PAGE_SIZE)));

void setup_paging() {
    for (int i = 0; i < NUM_PAGES; i++) {
        printf('.');
        first_page_table[i] = (i * PAGE_SIZE) | 3;
    }

    page_directory[0] = ((unsigned int)first_page_table) | 3;

    for (int i = 1; i < NUM_PAGES; i++) {
        page_directory[i] = 0; 
    }

    asm volatile("mov %0, %%cr3" :: "r"(page_directory));

    unsigned int cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; 
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
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
    uint8_t* font_data = font[c];
    for (int i = 0; i < 8; i++) {
        terminal_buffer[(x) + (y)] = font_data; // White
    }
}

void draw_string(uint8_t* framebuffer, int x, int y, char* str) {
    for (int i = 0; str[i] != '\0'; i++) {
        draw_char(terminal_buffer, x + i, y, str[i]);
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
    uart_puts("\nKERNEL FUNCTION SET_VGA_MODE: Setting VGA mode...");
    outb(0x3C4, 0x4F01);
    outb(0x3C5, 0x101);
}





bool fixed;




typedef struct {
    uint32_t base_addr;
    uint32_t length;
    uint32_t type;
    uint32_t acpi;
} memory_map_entry_t;

uint32_t get_rsdp_addr() {
    uint32_t rsdp_addr = 0;
    for (uint32_t i = 0x000E0000; i <= 0x000FFFFF; i += 16) {
        if (*((uint32_t*)i) == 0x20445352) {
            rsdp_addr = i;
            break;
        }
        // for (volatile int i = 0; i < 6000; i++);
    }
    return rsdp_addr;
}

uint32_t get_rsdt_addr(uint32_t rsdp_addr) {
    uint32_t rsdt_addr = 0;
    rsdt_addr = *((uint32_t*)(rsdp_addr + 16));
    return rsdt_addr;
}

void get_memory_map(memory_map_entry_t *entries, uint32_t *num_entries) {
    uint32_t rsdp_addr = get_rsdp_addr();
    uint32_t rsdt_addr = get_rsdt_addr(rsdp_addr);
    uint32_t entry_size = 20; 

    if (rsdt_addr == 0) {
        panic("Invalid RSDT address");
        return;
    }

    uint32_t num = 0;
    uint32_t entry_addr = rsdt_addr + 36;
    uint32_t total_size = rsdt_addr + 36 + entry_size; 


    printf("BOOT: Total RAM size: ");
    uart_puts("\nKERNEL FUNCTION GET_MEMORY_MAP: BOOT: Total RAM size: ");
    char size_str;
    MenintToStr(total_size, size_str);
    printf(size_str);
    uart_puts(size_str);
    printf(" bytes");
    uart_puts(" bytes");

    while (entry_addr < total_size) {
        entries[num].base_addr = *((uint32_t*)entry_addr);
        entries[num].length = *((uint32_t*)(entry_addr + 4));
        entries[num].type = *((uint32_t*)(entry_addr + 8));
        entries[num].acpi = *((uint32_t*)(entry_addr + 12));
        num++;
        entry_addr += entry_size;
    }

    
    *num_entries = num;
    printf("%nBOOT: Num Entries: ");
    uart_puts("\nKERNEL FUNCTION GET_MEMORY_MAP: BOOT: Num Entries: ");
    char num_str[10];
    MenintToStr(num, num_str);
    printf(num_str);
    uart_puts(num_str);
}




void MenintToStr(uint32_t N, char *str) {
    uint32_t i = 0;
    uint32_t sign = N;
    if (N < 0)
        N = -N;
    while (N > 0) {
        str[i++] = N % 10 + '0';
        N /= 10;
    }
    if (i == 0) {
        str[i++] = '0';
    }
    str[i] = '\0';
    for (uint32_t j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

void intToStr(uint32_t N, char *str) {
    uint32_t i = 0;
    uint32_t sign = N;
    if (N < 0)
        N = -N;
    while (N > 0) {
        str[i++] = N % 10 + '0';
        N /= 10;
    }
    if (sign < 0) {
        str[i++] = '-';
    }
    if (i == 0) {
        str[i++] = '0';
    }
    str[i] = '\0';
    for (uint32_t j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

void print_memory_size() {
    memory_map_entry_t entries[100];
    uint32_t num_entries = 0;
    get_memory_map(entries, &num_entries);
    int total_memory_size = 0;
    
    for (uint32_t i = 0; i < num_entries; i++) {
        if (entries[i].type == 1) {
            total_memory_size += entries[i].length / (1024 * 1024);
        }
    }
}

int countmem() {
    print_memory_size();
    return 0; 
}

void ide_detect_drives() {
    outb(0x1F6, 0xA0); 
    outb(0x1F7, 0x00);

    outb(0x1F6, 0xA0);
    uint8_t status = inb(0x1F7);
    if (status & 0x01) {
        task("Primary IDE master drive detected", 1);
        drives[0] = "idep0";
    }

    outb(0x1F6, 0xB0);
    status = inb(0x1F7); 
    if (status & 0x01) {
        task("Primary IDE slave drive detected", 1);
        drives[1] = "idep1";
    }

    outb(0x1F6, 0xA8);
    status = inb(0x1F7);
    if (status & 0x01) {
        task("Secondary IDE master drive detected", 1);
        drives[2] = "ides0";
    }

    outb(0x1F6, 0xB8);
    status = inb(0x1F7);
    if (status & 0x01) {
        task("Secondary IDE slave drive detected", 1);
        drives[3] = "ides1";
    }
    task("Add drive files...", 0);
    for (int i = 0; i < sizeof(drives) / sizeof(drives[0]); i++) {
  	 fakeroot[i] = drives[i];
    }
    task("Add drive files...", 1);
}

#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_SEQ_INDEX 0x3C4
#define VGA_SEQ_DATA 0x3C5
#define VGA_DAC_INDEX 0x3C8
#define VGA_DAC_DATA 0x3C9



void set_mode(uint16_t mode) {
    uint8_t crtc_index = 0x0F;
    uint8_t crtc_data = (mode & 0xFF);
    outb(VGA_CRTC_INDEX, crtc_index);
    outb(VGA_CRTC_DATA, crtc_data);

    crtc_index = 0x10;
    crtc_data = (mode >> 8);
    outb(VGA_CRTC_INDEX, crtc_index);
    outb(VGA_CRTC_DATA, crtc_data);
}


void kernel_main(void) 
{
    /*uint16_t mode = 0x51;
    outb(0x3C0, mode);*/
    VGA_HEIGHT++;
    terminal_initialize();
    terminal_writestring("Waiting for UART");
    uart_init();
    uart_puts("\nKERNEL FUNCTION MAIN: Loading kernel...");
    //waitwrite = true;
    uart_puts("\nKERNEL FUNCTION MAIN: Set cursor...");
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x02);
    //send_ping(0xC0A80002);
    //receive_ping();
    //wait(600000000000000);
    countmem();
    task("Detecting PCI devices...",0);
    for (int i; i < 255; i++) {
    	detect_pci(i);
    }
    task("Detecting PCI devices...",1);
    task("Detecting ATA devices...",0);
    ide_detect_drives();
    task("Detecting ATA devices...",1);
    
    //task("Setup paging...", 0);
    //setup_paging();
    //task("Setup paging...", 1);
    read_rtc();

    //waitwrite = false;
    if (!networkinit && !fsinit) {
        uart_puts("\nKERNEL FUNCTION MAIN: Failed to init network and fs!");
        shell();
        reboot();
    }
}

void shell() {
    //waitwrite = false;
    char yearstr;
    char monthstr;
    char hourstr;
    char minutestr;
    char secondstr;
    char daystr;
    task("Enter recovery shell.", task_ok);
    terminal_newline();
terminal_color = vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    terminal_writestring("New user login at [m/d/y]: ");
    uart_puts("\nKERNEL FUNCTION SHELL: New user login at [m/d/y]: ");
    intToStr(month, monthstr);
    terminal_writestring(monthstr);
    uart_puts(monthstr);
    terminal_writestring("/");
    uart_puts("/");
    intToStr(day, daystr);
    terminal_writestring(daystr);
    uart_puts(daystr);
    intToStr(year, yearstr);
    terminal_writestring("/");
    terminal_writestring(yearstr);
    uart_puts("/");
    uart_puts(yearstr);
    intToStr(hour, hourstr);
    terminal_writestring(" at [h/m] ");
    terminal_writestring(hourstr);
    uart_puts(" at [h/m] ");
    uart_puts(hourstr);
    intToStr(minute, minutestr);
    terminal_writestring(":");
    terminal_writestring(minutestr);
    uart_puts(":");
    uart_puts(minutestr);
    uart_puts("\n");
    terminal_newline();
    terminal_writestring("User: ");
    terminal_writestring(username);
    terminal_writestring("@");
    terminal_writestring(hostname);
    uart_puts("User: ");
    uart_puts(username);
    uart_puts("@");
    uart_puts(hostname);
    uart_puts("\n");
    uart_puts("KERNEL FUNCTION SHELL: To switch from VGA to UART enter the command 'vga2urt'");
    terminal_newline();
	terminal_writestring(username);
	terminal_writestring("@");
	terminal_writestring(hostname);
	terminal_writestring(" /> ");	

	while (1) {
        if (exitHKIloop == true) {
            break;
        	}
    
		terminal_color = vga_entry_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
	    handle_keyboard_input();
    }
}
