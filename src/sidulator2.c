#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <arpa/inet.h>

#define FAKE6502_USE_STDINT
#include "../3rdparty/fake6502/fake6502.h"

#define VERSION "0.1.1"

#define MAXFRAMES 100000000

static int flag_verbose = 0;
int verbose(const char * restrict format, ...) {
    if (!flag_verbose) { return 0; }

    va_list args;
    va_start(args, format);
    int retval = vfprintf(stderr, format, args);
    va_end(args);

    return retval;
}


static struct option long_options[] = {
    {"sidfile",  required_argument,             0, 'f'},
    {"emulate",  required_argument,             0, 'e'},
    {"verbose",  no_argument,       &flag_verbose, 'v'},
    {"subtune",  required_argument,             0, 't'},
    {"printmem", no_argument,                   0, 'm'},
    {"checkmem", required_argument,             0, 'r'},
    {"clear",    no_argument,                   0, 'c'},
    {"asm",      required_argument,             0, 'a'},
    {"help",     no_argument,                   0, 'h'},
    {0, 0, 0, 0}

};

void printHelp() {
    fprintf(stderr, "OPTIONS:\n");
    for (size_t i = 0; i < sizeof(long_options)/sizeof(long_options[0]); i++) {
        if (long_options[i].val == 0) { continue; }

        fprintf(stderr, "\t-%c, --%s\n", long_options[i].val, long_options[i].name);
    }
}



#define MEMSIZE 65536
static uint8 memory[MEMSIZE];
static uint8 memory_states[MEMSIZE];
static int memory_count[MEMSIZE];

#define MEMSTATE_READ (1)
#define MEMSTATE_WRITTEN (2)
#define MEMSTATE_INIT (4)

#define MEMSTATE_ALL (7)


uint8 read6502(ushort addr) {
    //verbose(" - read from %04X (%02x)\n", addr, memory[addr]);
    memory_states[addr] |= MEMSTATE_READ;
    memory_count[addr]++;
    return memory[addr];
}

void write6502(ushort addr, uint8 val) {
    //verbose(" - write to %04X (%02X)\n", addr, val);
    memory_states[addr] |= MEMSTATE_WRITTEN;
    memory_count[addr]++;
    memory[addr] = val;
}

void clearMemory(uint8 value) {
    memset(memory, value, sizeof(memory)/sizeof(memory[0]));
}

void clearMemoryTracking()
{
    memset(memory_states, 0, sizeof(memory_states)/sizeof(memory_states[0]));
    memset(memory_count, 0, sizeof(memory_count)/sizeof(memory_count[0]));
}





void loadSid(const char* filename, uint16_t* initAddr, uint16_t* playAddr) {
    verbose("Loading sid %s\n", filename);
    FILE *fp = fopen(filename, "r");
    if (!fp) { 
        fprintf(stderr, "Couldn't open file `%s`. Exiting...\n", filename);
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);

    fseek(fp, 0, SEEK_SET);

    uint8_t header[0x7C];
    fread(header,1, 0x7C, fp);

    uint16_t version = ntohs(*(uint16_t*)&(header[0x4]));
    uint16_t dataoffs = ntohs(*(uint16_t*)&(header[0x6]));


    verbose("SIDINFO: sid version %d\n", version);



    uint16_t loadaddr = ntohs(*(uint16_t*)&(header[0x8]));
    *initAddr = ntohs(*(uint16_t*)&(header[0xA]));
    *playAddr = ntohs(*(uint16_t*)&(header[0xC]));
    uint32_t ident = *(uint32_t*)&(header[0x0]);

    fseek(fp, dataoffs, SEEK_SET);

    if (loadaddr == 0)
        fread( &loadaddr, 2, 1, fp);

    long datalen = filesize - ftell(fp);


    verbose("SIDINFO: id %08X\n", ident);
    verbose("SIDINFO: load address: $%04X-$%04X\n", loadaddr, (int)(loadaddr+datalen-1));
    verbose("SIDINFO: init address: $%04X\n", *initAddr);
    verbose("SIDINFO: play address: $%04X\n", *playAddr);
    verbose("SIDINFO: name: %s\n", &(header[0x16]));
    verbose("SIDINFO: author: %s\n", &(header[0x36]));
    verbose("SIDINFO: copyright: %s\n", &(header[0x56]));

    fread(&(memory[loadaddr]), 1, datalen, fp);

    for (int i = 0; i < datalen; i++)
    {
        memory_states[ loadaddr + i ] |= MEMSTATE_INIT;
    }

    fclose(fp);
}

#define STACK_END (0x1FE)

void runUntilRTS(uint16_t newpc, uint8_t accu) {
    pc = newpc;
    a = accu;
    sp = 0xFD;
    status = 0;
    memory[STACK_END] = 0xFE;
    memory[STACK_END+1] = 0xFF;

    //while(sp <= 0xFD)
    while(pc != 0xFFFF)
    {
        //printf("pc %04X: sp %d\n", pc, sp);
        step6502();
    }

    // alles good if the stack end was accessed only once
    if (memory_count[STACK_END] == 1 && memory_count[STACK_END+1] == 1)
    {
        memory_states[STACK_END] = 0;
        memory_states[STACK_END+1] = 0;
        memory_count[STACK_END] = 0;
        memory_count[STACK_END+1] = 0;
    }

}

void callInit(int subTune, uint16_t initAddr)
{
    verbose("Begin emulation..");
    runUntilRTS(initAddr, subTune);
    verbose(".");
}

void RunSid(int frameCount, uint16_t playAddr) {
    //reset6502();

    for (int i = 0; i < frameCount; i++)
    {
        runUntilRTS(playAddr, 0);
        if (!(i%1000))
            verbose(".");
    }
    verbose("\n");
}

void printAllWithMask(int mask) {
    char* statestrs[] = { "", "read", "written", "read/written", "init", "init/read", "init/written", "init/read/written" };
    for (int i = 0; i <= 0xFFFF; i++)
    {
        if ( memory_states[i] & mask)
        {
            char* statestr = statestrs[ memory_states[ i ] ];
            printf("%04X: %02X %d (%s)\n", i, memory[ i ], memory_count[i], statestr);
        }
    }
}

void printRangesWithMask(int mask) {
    bool flag = false;
    int rangestart, rangeend;
    for (int i = 0; i <= 0xFFFF; i++)
    {
        if (!flag && (memory_states[i] & mask))
        {
            flag = true;
            rangestart = i;
        }
        else if (flag && !(memory_states[i] & mask))
        {
            flag = false;
            rangeend = i;

            printf("range: %04X-%04X\n", rangestart, rangeend-1);
            
        }
    }
}

void printAsmWithMask(int mask, const char* fn) {
    for (int i = 0; i <= 0xFFFF; i++)
    {
        if ( memory_states[i] & mask)
        {
            printf("LDA #$%02X\n", memory[i]);
            printf("STA $%04X\n", i);
        }
    }
}


int checkUsage(uint16_t startAddr, uint16_t endAddr) {
    int s = startAddr < endAddr ? startAddr : endAddr;
    int e = startAddr > endAddr ? startAddr : endAddr;

    if (s == e) {
        verbose("Checking usage of address 0x%04x\n", s);
    } else {
        verbose("Checking usage of region 0x%04x - 0x%04x\n", s, e);
    }

    for (int i = s; i <= e; i++) {
        if ( memory_states[i] )
        {
            fprintf(stderr, "Region $%04X-%04X used by tune!\n", s, e);
            return 1;
        }
    }

    return 0;

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
        memory_states[i] = 0;
        memory_count[i] = 0;
    }
}

void markRegion(uint16_t startAddr, uint16_t endAddr) {
    int s = startAddr < endAddr ? startAddr : endAddr;
    int e = startAddr > endAddr ? startAddr : endAddr;

    if (s == e) {
        verbose("Including address 0x%04x\n", s);
    } else {
        verbose("Including region 0x%04x - 0x%04x\n", s, e);
    }

    for (int i = s; i <= e; i++) {
        memory_states[i] |= MEMSTATE_ALL;
    }
}

int main(int argc, char** argv) {
    verbose("SIDulator v%s - Pre-replays a sid file to the correct position and saves the diff\n", VERSION);

    int c = 0;

    //char* diff_filename = NULL;
    //char* loadaddr_str = NULL;
    //char* skipbytes_str = NULL;
    //char* includeregions_str = NULL;


    clearMemory(0);
    clearMemoryTracking();

    uint16_t initAddr = 0;
    uint16_t playAddr = 0;

    int subTune = 0;
    int frameCount = 6*60*50;

    do {
        int option_index = 0;
        c = getopt_long(argc, argv, "f:e:vt:mhr:ca", long_options, &option_index);

        if (c < 0) { break; }

        switch (c) {
            case 0:
                /* If this option set a flag, do nothing else now. */
                if (long_options[option_index].flag != 0) { break; }

                fprintf(stderr, "option %s", long_options[option_index].name);

                if (optarg) {
                    fprintf(stderr, " with arg %s\n", optarg);
                }
                break;

            case 'h':
                printHelp();
                break;

            case 'v':
                verbose("Verbose mode\n");
                flag_verbose = 'v';
                break;

            case 'c':
                verbose("Clearing memory tracking\n");
                clearMemoryTracking();
                break;

            case 'e':
                if (initAddr == 0 || playAddr == 0)
                {
                    fprintf(stderr, "can't emulate without program\n");
                    exit(1);
                }
                frameCount = (int)strtol(optarg, NULL, 0);
                verbose("emulating %d frames\n", frameCount);
                RunSid(frameCount, playAddr);

                ignoreRegion(0x0100, 0x01ff); // ignore stack
                break;

            case 'f':
                verbose("loading sid `%s`\n", optarg);
                loadSid(optarg, &initAddr, &playAddr);
                break;

            case 't':
                subTune = (int)strtol(optarg, NULL, 0);
                verbose("using subtune %d\n", subTune);
                callInit(subTune, initAddr);
                break;

            case 'm':
                verbose("Printing used memory:\n");
                printAllWithMask(MEMSTATE_ALL);
                break;

            case 'a':
                verbose("Generating asm:\n");
                printAsmWithMask(MEMSTATE_WRITTEN, optarg);
                break;


            case 'r':
            {
                int beg,end;
                sscanf(optarg, "%X-%X", &beg, &end);
                if (checkUsage(beg,end))
                    exit(2);
                break;
            }

//            case 'r':
//                verbose("Check memory usage with rules='%s'", optarg);
//                rulefile_str = optarg;

            default:
                exit(1);
        }
    } while (c >= 0);

    return 0;
}

