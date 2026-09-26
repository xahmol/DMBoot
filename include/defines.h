/*
DMBoot 128 v5
Device Manager Boot Menu for the Commodore 128

Written in 2020-2026 by Xander Mol
https://github.com/xahmol/DMBoot
https://www.idreamtin8bits.com/

Code and resources from others used:
-   Oscar64 cross compiler
    https://github.com/drmortalwombat/oscar64
-   Ultimate 64/II+ Command Library, Scott Hutter, Francesco Sblendorio
    https://github.com/xlar54/ultimateii-dos-lib
-   C128 Device Manager ROM and its extended API, Bart van Leeuwen
    https://www.bartsplace.net/content/publications/devicemanager128.shtml
-   Ultimate II+ cartridge, Gideon Zweijtzer
    https://ultimate64.com/

The code can be used freely as long as you retain a notice describing
original source and author.

THE PROGRAMS ARE DISTRIBUTED IN THE HOPE THAT THEY WILL BE USEFUL,
BUT WITHOUT ANY WARRANTY. USE THEM AT YOUR OWN RISK!
*/

#ifndef DEFINES_H
#define DEFINES_H

// ---------------------------------------------------------------------------
// Version (VERSION string is injected by the Makefile via -dVERSION)
// ---------------------------------------------------------------------------
#ifndef VERSION
#define VERSION "v5.0.0-dev"
#endif

// ---------------------------------------------------------------------------
// MMU $FF00 configuration register values (see docs/REBUILD_PLAN.md §3.1)
// ---------------------------------------------------------------------------
#define BNK_DEFAULT     0x0e    // Bank 0, I/O, RAM $4000-$BFFF, KERNAL ROM at $C000+
#define BNK_0_FULL      0x3f    // Bank 0, all RAM (including under ROM), no I/O
#define BNK_1_FULL      0x7f    // Bank 1, all RAM, no I/O
#define BNK_0_IO        0x3e    // Bank 0, all RAM, I/O visible
#define BNK_1_IO        0x7e    // Bank 1, all RAM, I/O visible
#define BNK_DM_FUNCROM  0x2a    // External function ROM (Device Manager ROM) visible
#define BNK_CHARROM     0x01    // Bank 0 with the character ROM at $D000 (no I/O)

// MMU RAM configuration register ($D505 area, xmmu.rcr) values
#define RCR_COMMON_8K_BOTTOM    0x06    // 8 KB common RAM at $0000-$1FFF
#define RCR_COMMON_DEFAULT      0x04    // 1 KB common RAM (C128 power-on default)

// ---------------------------------------------------------------------------
// C128 system locations
// ---------------------------------------------------------------------------
#define ZP_MODE_80COL       0xd7    // Bit 7 set: 80 column (VDC) screen active
#define ZP_CURRENT_DEVICE   0xba    // Last used device number
#define ZP_KEYBUF_COUNT     0xd0    // Number of keys in the keyboard buffer
#define KEYBUF_ADDRESS      0x034a  // KERNAL keyboard buffer
#define KEYBUF_SIZE         10      // Keyboard buffer length in bytes
#define VIC_CLOCK_REG       0xd030  // VIC-IIe clock register, bit 0 = 2 MHz
#define VIC_CLOCK_FAST      0x01
#define VIC_CTRL1_REG       0xd011  // VIC control register 1
#define VIC_CTRL1_DEN       0x10    // Display enable
#define VIC_CTRL1_RST8      0x80    // Raster compare bit 8: kept 0, as BASIC FAST does
#define MODE_80COL_FLAG     0x80

// ---------------------------------------------------------------------------
// Memory map (docs/REBUILD_PLAN.md §3.2 - §3.3)
// ---------------------------------------------------------------------------
#define LMC_START           0x1300  // Low-memory code overlay (common RAM)
#define LMC_END             0x1b00
#define RESIDENT_START      0x1c80  // Resident program region start
#define OVERLAYSIZE         0x2800  // Size of the one overlay load slot
#define OVERLAYLOAD         0x9800  // Overlay load slot, = 0xc000 - OVERLAYSIZE
#define OVERLAY_SLOT_END    0xc000

// Overlay storage locations
#define OVERLAY_STORE_BANK1_1   0x4000
#define OVERLAY_STORE_BANK1_2   0x6800
#define OVERLAY_STORE_BANK1_3   0x9000
#define OVERLAY_STORE_BANK1_4   0xb800
#define OVERLAY_STORE_BANK0_1   0xc000

// DualWin popup background storage (bank 1)
#define WINDOW_STORE_BASE       0x2000
#define WINDOW_STORE_SIZE       0x2000

// Number of overlays (see the overlay table in src/main.c)
#define OVERLAY_COUNT       5
#define OVERLAY_NONE        0       // Value of overlay_active when none is loaded
#define OVERLAY_NAME_MAX    17      // CBM filename (16) plus terminator

// Filename prefix for files in the Device Manager partition 11
#define DM_PARTITION_PREFIX "11:"

// ---------------------------------------------------------------------------
// REU (docs/REBUILD_PLAN.md §3.5)
// ---------------------------------------------------------------------------
#define REU_MIN_PAGES       2       // Minimum REU size in 64 KB pages (128 KB)
#define REU_PAGE_BYTES      0x10000UL

// ---------------------------------------------------------------------------
// Slots and configuration (docs/REBUILD_PLAN.md §8)
// ---------------------------------------------------------------------------
#define CFGVERSION          0x05    // Version of the slot and config file format
#define SLOTS               36      // Number of boot menu slots (keys 0-9, a-z)
#define SLOTSIZE            1360    // sizeof(struct SlotStruct), checked below
#define SLOT_REU_START      0x00000UL   // REU address of slot 0
#define SLOT_REU_BACKUP     0x10000UL   // Slot backup while re-ordering (directory area: browser not active then)
#define DIR_REU_START       0x10000UL   // File browser directory list, up to the top of the REU
#define SAVE_BUF_SIZE       500     // Bytes per UCI write (data queue is 512)

// String buffer sizes, including the terminator
#define MAXPATHLEN          256
#define MAXFILENAME         51
#define MAXMENUNAME         31
#define MAXCOMMAND          81
#define MAXHOSTLENGTH       81
#define STORAGE_PATH_MAX    16      // e.g. "/usb*/11/"

// IEC bus scan: IDs 8-30 plus the printer ID 4
#define IEC_ID_FIRST        8
#define IEC_ID_COUNT        23      // indexes 0-21 = IDs 8-29, index 22 = ID 4
#define IEC_ID_PRINTER      4
#define UII_DEVINFO_COUNT   4       // uii_devinfo[]: drive A, drive B, SoftIEC, printer
#define UII_DEVINFO_DRIVES  2       // uii_devinfo[0..1]: drives A and B

// Slot command flags (SlotStruct.command)
#define COMMAND_CMD         0x01    // Run the user command
#define COMMAND_REU         0x02    // Load an REU image
#define COMMAND_IMGA        0x04    // Mount an image on drive A
#define COMMAND_IMGB        0x08    // Mount an image on drive B

// Slot execute flags (SlotStruct.runboot); v4 values kept, extended
#define EXEC_MOUNT          0x01    // Run from the image mounted on drive A
#define EXEC_FRC8           0x02    // Force device ID 8
#define EXEC_RUN64          0x04    // Run in C64 mode (Device Manager API)
#define EXEC_FAST           0x08    // Switch to FAST (2 MHz) before running
#define EXEC_BOOT           0x10    // BOOT instead of RUN
#define EXEC_COMMA1         0x20    // LOAD with ,1 (absolute address)
#define EXEC_DEMO           0x40    // Demo mode: power down drives not on ID 8

// One boot menu slot. Field order and sizes follow UBoot64-v2's SlotStruct
// (DMBoot-specific options live in the runboot bits). Stored in the REU.
struct SlotStruct
{
    char cfgvs;                     // CFGVERSION
    char path[MAXPATHLEN];          // IEC directory path of the program
    char menu[MAXMENUNAME];         // Name shown in the menu
    char file[MAXFILENAME];         // Program file name (empty: mount/command only)
    char cmd[MAXCOMMAND];           // User command
    char reu_image[MAXFILENAME];    // REU image file name
    char reu_path[MAXPATHLEN];      // REU image path (Ultimate file system)
    char reusize;                   // REU size index 0-7 (128 KB - 16 MB)
    char runboot;                   // EXEC_* flags
    char device;                    // IEC device ID to run from
    char command;                   // COMMAND_* flags
    char image_a_path[MAXPATHLEN];  // Drive A image path
    char image_a_file[MAXFILENAME]; // Drive A image file
    char image_a_id;                // Drive A device ID
    char image_b_path[MAXPATHLEN];  // Drive B image path
    char image_b_file[MAXFILENAME]; // Drive B image file
    char image_b_id;                // Drive B device ID
    char isdefault;                 // 1 = auto-boot default slot
    char partition;                 // Firmware 3.15 SoftIEC partition, 0 = none
    char padding[11];               // Zero, pads the struct to SLOTSIZE
};
typedef char slot_size_check[(sizeof(struct SlotStruct) == SLOTSIZE) ? 1 : -1];

// Colour scheme (logical C64/VIC colour numbers, see DUALWINMANUAL.md)
struct ColorPalette
{
    char background;
    char border;
    char header1;
    char header2;
    char text;
    char text_input;
    char key;
    char diritem_normal;
    char diritem_select;
    char error;
    char ok;
};

// GEOS RAM boot settings (F6)
struct GeosConfig
{
    char reu_path[MAXPATHLEN];      // Path of the GEOS REU image
    char reu_image[MAXFILENAME];    // GEOS REU image file name
    char reusize;                   // REU size index 0-7
    char image_a_id;                // Drive A device ID (0 = no image)
    char image_a_path[MAXPATHLEN];
    char image_a_file[MAXFILENAME];
    char image_b_id;                // Drive B device ID (0 = no image)
    char image_b_path[MAXPATHLEN];
    char image_b_file[MAXFILENAME];
};

// Global configuration, file dmbconf.cfg. New fields go at the end: an older,
// shorter file is read with them zero (readconfigfile).
#define CONFIGSIZE          1203    // sizeof(struct ConfigStruct), checked below
#define NTP_SERVERS         3
// Start-up feedback (ConfigStruct.verbose). The names fit a menu line in
// both 40 and 80 columns (Phase 5 configuration screen).
#define VERBOSE_SILENT      0       // Spinner only
#define VERBOSE_ON          1       // Messages
#define VERBOSE_WAIT        2       // Messages, then wait for a key
#define VERBOSE_OPTIONS     3
#define VERBOSE_NAME_SILENT "Silent"
#define VERBOSE_NAME_ON     "Show messages"
#define VERBOSE_NAME_WAIT   "Show messages + wait"

struct ConfigStruct
{
    char version;                   // CFGVERSION
    char timeon;                    // 1 = set the time via NTP at start-up (firmware 3.14d+ does it itself)
    char host[MAXHOSTLENGTH];       // First NTP server
    long secondsfromutc;            // Time zone offset
    char verbose;                   // VERBOSE_SILENT / _ON / _WAIT
    struct ColorPalette colors;
    char timeoutidx;                // Auto-boot timeout index, 0 = off
    char iec_root_partition;        // Firmware 3.15 preparation, 0 = off
    struct GeosConfig geos;
    char reserved[16];              // Zero, room for later settings
    char host2[MAXHOSTLENGTH];      // Second NTP server (tried when the first fails)
    char host3[MAXHOSTLENGTH];      // Third NTP server
};
typedef char config_size_check[(sizeof(struct ConfigStruct) == CONFIGSIZE) ? 1 : -1];

// ---------------------------------------------------------------------------
// Key codes (raw PETSCII as returned by KERNAL GETIN)
// ---------------------------------------------------------------------------
#define KEY_NONE            0x00
#define KEY_RETURN          0x0d
#define KEY_F1              0x85
#define KEY_F3              0x86
#define KEY_F5              0x87
#define KEY_F7              0x88
#define KEY_F2              0x89
#define KEY_F4              0x8a
#define KEY_F6              0x8b
#define KEY_F8              0x8c

// ---------------------------------------------------------------------------
// Structures
// ---------------------------------------------------------------------------

// Where an overlay image is kept after the one-time load at startup
struct OverlayStore
{
    char mmucr;             // MMU $FF00 value to reach the store (BNK_1_FULL / BNK_0_FULL)
    unsigned address;       // Start address of the store in that bank
    char name[OVERLAY_NAME_MAX]; // Overlay file name (without partition prefix)
    const char *purpose;    // What the overlay contains (start-up messages)
};

// Machine / environment state detected at startup
struct SystemInfo
{
    char bootdevice;        // Device number DMBoot was loaded from
    char mode80;            // 1 = 80 column (VDC) mode, 0 = 40 column (VIC)
    char fast;              // 1 = running at 2 MHz
    unsigned reupages;      // Detected REU size in 64 KB pages (0 = none)
    unsigned diskloads;     // Number of overlay files loaded from disk
};

// Request from the file browser (overlay 3) to start a program directly.
// The browser cannot call the slot start overlay (5), so it leaves the
// request here and main() hands it over.
#define BROWSE_QUIT         0       // Back to the main menu
#define BROWSE_RUN          1       // Start browsereq.file on browsereq.device
struct BrowseRequest
{
    char action;                    // BROWSE_*
    char device;                    // IEC device ID
    char runboot;                   // EXEC_* flags
    char file[MAXFILENAME];         // Program name (PETSCII), empty for BOOT
};

// Device Manager ROM extended API information
struct DMApiInfo
{
    char present;           // 1 = API header found
    char version_major;     // API version high byte
    char version_minor;     // API version low byte
    char hyperspeed_id;     // Device ID of the hyperspeed drive
};

// Global variables (defined in src/main.c)
extern struct SystemInfo sysinfo;
extern struct SlotStruct Slot;          // Working copy of one slot
extern struct ConfigStruct cfg;         // Global configuration
extern struct DMApiInfo dminfo;
extern struct BrowseRequest browsereq;
extern char overlay_active;
struct DWin;
extern struct DWin screenwin;    // Full-screen window (src/main.c)
extern struct DWin console;      // Scrolling message window (src/main.c)

// Functions in src/main.c used by the overlays
void screen_swap(void);
void cpu_set_fast(bool fast);

#endif // DEFINES_H
