#include <stdio.h>
#include <string.h>

int usages[65536];

int main(int argc, char* argv[])
{
    memset(usages, 0, sizeof(int)*65536);
    while(!feof(stdin))
    {
        char rivi[256];
        fgets(rivi, 256, stdin);
        rivi[255] = 0;

        int addr = 0;
        int finalval = 0;
        int count = 0;
        sscanf(rivi, "%X: %X %d", &addr, &finalval, &count);
        //printf("%X %X %X\n", addr, finalval, count);

        usages[addr]++;
    }

    char chars[] = { '.', '1', '2', '3', '4', '5', '6', '7', '8', '9', '#' };
    int cc = sizeof(chars) / sizeof(chars[0]);

    for (int y = 0; y < 256; y++)
    {
        printf("%04X: ", y*256+0);
        for (int x = 0; x < 128; x++)
            fputc( chars [ usages[y*256+x] >= cc ? (cc - 1) : usages[y*256+x] ], stdout);
        //fputc('\n', stdout);

        //printf("%04X: ", y*256+128);
        for (int x = 128; x < 256; x++)
            fputc( chars [ usages[y*256+x] >= cc ? (cc - 1) : usages[y*256+x] ], stdout);
        fputc('\n', stdout);
    }
}