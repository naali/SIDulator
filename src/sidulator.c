#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#define FAKE6502_USE_STDINT
#include "../3rdparty/fake6502/fake6502.h"

#define VERSION "0.1.0"

#define MAXFRAMES 100000000

#define MEMSIZE 65536
static uint8 memory[MEMSIZE];
static uint8 memory_changes[MEMSIZE];

uint8 read6502(ushort addr) {
    return memory[addr];
}

void write6502(ushort addr, uint8 val) {
    memory_changes[addr] = 1;
    memory[addr] = val;
}

static int flag_verbose = 0;
static int flag_overwrite = 0;
static int flag_ignoresidregs = 0;

static struct option long_options[] = {
    {"sidfile", required_argument, 0, 'f'},
    {"skipbytes", optional_argument, 0, 's'},
    {"loadaddr", required_argument, 0, 'l'},
    {"difffile", required_argument, 0, 'd'},
    {"framecount", required_argument, 0, 'c'},
    {"playerprg", required_argument, 0, 'p'},
    {"flipflopaddr", required_argument, 0, 'i'},
    {"framecounteraddr", required_argument, 0, 't'},
    {"includeregions", optional_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"ignoresidregs", no_argument, &flag_ignoresidregs, 'r'},
    {"overwrite", no_argument, &flag_overwrite, 'o'},
    {"verbose", no_argument, &flag_verbose, 'v'},
    {0, 0, 0, 0}
};

int verbose(const char * restrict format, ...) {
    if (!flag_verbose) { return 0; }

    va_list args;
    va_start(args, format);
    int retval = vprintf(format, args);
    va_end(args);

    return retval;
}

void printHelp() {
    printf("OPTIONS:\n");
    for (size_t i = 0; i < sizeof(long_options)/sizeof(long_options[0]); i++) {
        if (long_options[i].val == 0) { continue; }

        printf("\t-%c, --%s\n", long_options[i].val, long_options[i].name);
    }
}

void loadFile(const char* filename, const ushort loadAddress, const ushort skipBytes) {
    FILE * fp = fopen(filename, "r");

    if (!fp) { 
        printf("Couldn't open file `%s`. Exiting...\n", filename);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);

    if ((filesize + loadAddress - skipBytes) > (MEMSIZE - 1)) {
        printf("Filesize of `%s` too long (%ld bytes) or load address (0x%04x) too high.\n", filename, filesize, loadAddress);
        exit(1);
    }

    fseek(fp, skipBytes, SEEK_SET);

    int bytesRead = fread((memory + loadAddress), 1, filesize, fp);
    printf("Bytes read: %d\n", bytesRead);

    fclose(fp);
}

uint16_t loadPrg(const char* filename) {
    FILE * fp = fopen(filename, "rb");

    if (!fp) {
        printf("Couldn't open player file `%s`. Exiting...\n", filename);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);

    fseek(fp, 0, SEEK_SET);

    uint8 lo = 0;
    uint8 hi = 0;

    fread(&lo, 1, 1, fp);
    fread(&hi, 1, 1, fp);

    int bytesRead = fread((memory + hi * 256 + lo), 1, filesize - 2, fp);
    printf("Player bytes read: %d\n", bytesRead);

    fclose(fp);

    return (hi * 256 + lo);
}

void clearMemory(uint8 value) {
    memset(memory, value, sizeof(memory)/sizeof(memory[0]));
    memset(memory_changes, 0, sizeof(memory)/sizeof(memory[0]));
}

void printChanges() {
    verbose("\nMemory changes:\n");

    int count = 0;
    for (int i = 0; i < MEMSIZE; i++) {
        if (memory_changes[i] != 0) {
            verbose("0x%04x: 0x%02x\n", i, memory[i]);
            count++;
        }
    }

    printf("Total changes: %d places\n", count);
}

void saveChanges(const char* filename, bool overwrite) {
    FILE * fp = NULL;

    if (!overwrite) {
        if ((fp = fopen(filename, "rb")) != NULL) {
            printf("Diff file `%s` already exists. Exiting...\n", filename);
            fclose(fp);
            exit(1);
        }
    }

    fp = fopen(filename, "wb");

    if (!fp) {
        printf("Couldn't create diff file `%s`. Exiting...\n", filename);
        exit(1);
    }

    uint8_t ldx[] = { 0xa2, 0x00 };
    uint8_t inx[] = { 0xe8 };
    uint8_t stx[] = { 0x8e, 0x00, 0x00 };

    uint8_t previousChange = 0x00;

    for (uint32_t b = 0; b <= 255; b++) {
        bool ldxDone = false;
        ldx[1] = (b & 255);

        for (uint32_t i = 0; i < MEMSIZE; i++) {
            if (memory_changes[i] != 0 && memory[i] == ldx[1]) {
                stx[1] = (i & 255);
                stx[2] = (i >> 8) & 255;

                if (!ldxDone) {
                    if (previousChange + 1 == ldx[1]) {
                        fwrite(inx, 1, sizeof(inx), fp);
                    } else {
                        fwrite(ldx, 1, sizeof(ldx), fp);
                    }

                    ldxDone = true;
                }

                fwrite(stx, 1, sizeof(stx), fp);
                previousChange = ldx[1];
            }
        }
    }

    uint8_t rts[] = { 0x60 };
    fwrite(rts, 1, sizeof(rts), fp);

    fclose(fp);
}

void printMemory() {
    printf("\n");

    for (size_t y = 0; y < sizeof(memory)/16; y++) {
        printf("%04lx ", y * 16);
        printf(" %02x %02x %02x %02x %02x %02x %02x %02x ", memory[y * 16 + 0], memory[y * 16 + 1], memory[y * 16 + 2], memory[y * 16 + 3], memory[y * 16 + 4], memory[y * 16 + 5], memory[y * 16 + 6], memory[y * 16 + 7]);
        printf(" %02x %02x %02x %02x %02x %02x %02x %02x ", memory[y * 16 + 8], memory[y * 16 + 9], memory[y * 16 + 10], memory[y * 16 + 11], memory[y * 16 + 12], memory[y * 16 + 13], memory[y * 16 + 14], memory[y * 16 + 15]);
        printf(" |");

        for (size_t x = 0; x < 16; x++) {
            if (x == 8) { putchar(' '); }

            int c = memory[y * 16 + x];

            (c < 32 || c > 126) ? putchar('.') : putchar(c);
        }

        printf("|\n");
    }
}

void playMusic(uint16_t playerStartAddress, uint16_t frameCounterAddress, uint16_t frameChangedAddress, int maxFrames) {
    reset6502();

    pc = playerStartAddress;

    uint8_t flipflop = memory[frameChangedAddress];

    long long sanitycounter = MAXFRAMES;
    int frame = 0;

    verbose("Processing: ");

    while (--sanitycounter > 0 && frame < maxFrames) {
        step6502();

        if (flipflop != memory[frameChangedAddress]) {
            flipflop = memory[frameChangedAddress];
            
            frame = memory[frameCounterAddress] + (memory[frameCounterAddress + 1] << 8) + 
                    (memory[frameCounterAddress + 2] << 16) + (memory[frameCounterAddress + 3] << 24);

            if (flag_verbose) {
                if (frame % 3007 == 0) { putchar('.'); }
            }
        }
    }

    if (sanitycounter == 0) {
        printf("SANITY COUNTER OVERFLOWED! Exiting...\n");
        exit(1);
    }

    verbose(". DONE!\n");
}

void ignoreRegion(uint16_t startAddr, uint16_t endAddr) {
    int s = startAddr < endAddr ? startAddr : endAddr;
    int e = startAddr > endAddr ? startAddr : endAddr;

    if (s == e) {
        verbose("Ignoring address 0x%04x\n", s);
    } else {
        verbose("Ignoring region 0x%04x - 0x%04x\n", s, e);
    }

    for (int i = s; i <= e; i++) {
        memory_changes[i] = 0;
    }
}

void includeRegion(uint16_t startAddr, uint16_t endAddr) {
    int s = startAddr < endAddr ? startAddr : endAddr;
    int e = startAddr > endAddr ? startAddr : endAddr;

    if (s == e) {
        verbose("Including address 0x%04x\n", s);
    } else {
        verbose("Including region 0x%04x - 0x%04x\n", s, e);
    }

    for (int i = s; i <= e; i++) {
        memory_changes[i] = 1;
    }
}

int main(int argc, char** argv) {
    printf("SIDulator v%s - Pre-replays a sid file to the correct position and saves the diff\n", VERSION);

    int c = 0;

    char* sid_filename = NULL;
    char* diff_filename = NULL;
    char* playerprg_filename = NULL;
    char* loadaddr_str = NULL;
    char* framecount_str = NULL;
    char* skipbytes_str = NULL;
    char* flipflopaddr_str = NULL;
    char* framecounteraddr_str = NULL;
    char* includeregions_str = NULL;

    do {
        int option_index = 0;
        c = getopt_long(argc, argv, "f:s:l:d:c:p:i:t:g:hrov", long_options, &option_index);

        if (c < 0) { break; }

        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0) { break; }

                printf("option %s", long_options[option_index].name);

                if (optarg) {
                    printf(" with arg %s\n", optarg);
                }
                break;

            case 'v':
                verbose("Verbose mode\n");
                flag_verbose = 'v';
                break;

            case 'o':
                verbose("Overwrite existing .diff-file\n");
                flag_overwrite = 'o';
                break;
            
            case 'r':
                verbose("Ignore changes in SID registers\n");
                flag_ignoresidregs = 'r';
                break;

            case 'h':
                printHelp();
                exit(0);
                break;

            case 'l':
                verbose("loadaddr=`%s`\n", optarg);
                loadaddr_str = optarg;
                break;

            case 's':
                verbose("skipbytes=`%s`\n", optarg);
                skipbytes_str = optarg;
                break;
            
            case 'c':
                verbose("framecount=`%s`\n", optarg);
                framecount_str = optarg;
                break;

            case 'f':
                verbose("sidfile=`%s`\n", optarg);
                sid_filename = optarg;
                break;

            case 'd':
                verbose("difffile=`%s`\n", optarg);
                diff_filename = optarg;
                break;

            case 'p':
                verbose("playerprg=`%s`\n", optarg);
                playerprg_filename = optarg;
                break;

            case 'i':
                verbose("flipflopaddr=`%s\n`", optarg);
                flipflopaddr_str = optarg;
                break;

            case 't':
                verbose("framecounteraddr=`%s`\n", optarg);
                framecounteraddr_str = optarg;
                break;

            case 'g':
                verbose("includeregions=`%s`\n", optarg);
                includeregions_str = optarg;
                break;

            case '?':
                /* getopt_long already printed an error message. */
                break;

            default:
                abort();
        }
    } while (c >= 0);

    if (optind < argc) {
        printf("Extra arguments given (maybe you got something wrong?): ");

        while (optind < argc) {
            printf("%s ", argv[optind++]);
        }

        putchar('\n');
    }

    if (sid_filename == NULL || diff_filename == NULL || 
        loadaddr_str == NULL || playerprg_filename == NULL ||
        flipflopaddr_str == NULL || framecounteraddr_str == NULL) {

        printf("Mandatory parameter(s) missing!\n");
        exit(1);
    }

    int loadAddress = 0;
    if (loadaddr_str != NULL) { loadAddress = (int)strtol(loadaddr_str, NULL, 0); }

    int skipBytes = 0;
    if (skipbytes_str != NULL) { skipBytes = (int)strtol(skipbytes_str, NULL, 0); }

    int frameCount = 0;
    if (framecount_str != NULL) { frameCount = (int)strtol(framecount_str, NULL, 0); }


    clearMemory(0);
    loadFile(sid_filename, loadAddress, skipBytes);
    uint16_t playerStartAddress = loadPrg(playerprg_filename);

    int flipflopAddress = 0;
    if (flipflopaddr_str != NULL) { flipflopAddress = (int)strtol(flipflopaddr_str, NULL, 0); }

    int frameCounterAddress = 0;
    if (framecounteraddr_str != NULL) { frameCounterAddress = (int)strtol(framecounteraddr_str, NULL, 0); }

    playMusic(playerStartAddress, frameCounterAddress, flipflopAddress, frameCount);
//    printMemory();
    ignoreRegion(0x0000, 0x00ff); // ignore zp
    ignoreRegion(0x0100, 0x01ff); // ignore stack

    if (flag_ignoresidregs) { ignoreRegion(0xd400, 0xd7ff); }

    if (includeregions_str != NULL) {
        verbose("Include regions: `%s`\n", includeregions_str);
        int maxlen = strlen(includeregions_str);
        char *maxlenptr = includeregions_str + maxlen;

        char* tokens = ",\0";
        char* chrptr = NULL;

        chrptr = strtok(includeregions_str, tokens);

        while (chrptr != NULL) {
            char* dashptr = strstr(chrptr, "-");
            if (dashptr == NULL) {
                int addr = (int)strtol(chrptr, NULL, 0);
                includeRegion(addr, addr);
            } else {
                int addr_s = (int)strtol(chrptr, NULL, 0);
                int addr_e = (int)strtol(dashptr + 1, NULL, 0);
                includeRegion(addr_s, addr_e);
            }

            chrptr = strtok(NULL, tokens);
        }
    }

    printChanges();
    saveChanges(diff_filename, (flag_overwrite != 0));

    exit(0);
}
