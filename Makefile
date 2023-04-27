KICKASM_CMD=java -jar ~/bin/KickAssembler/KickAss.jar

CFLAGS=-std=c99 -O2
SOURCES=sidulator.c

.DEFAULT_GOAL:=all

player.prg: src/player.asm
	$(KICKASM_CMD) -o $@ $<

sidulator: src/$(SOURCES)
	$(CC) $(CFLAGS) $< -o $@

sidulator2: src/sidulator2.c
	$(CC) $(CFLAGS) $< -o $@

analyzeusage: src/analyzeusage.c
	$(CC) $(CFLAGS) $< -o $@

all: player.prg sidulator sidulator2 analyzeusage

run: all
	./sidulator -f testfiles/music_2_0800.sid -d music_2_0800.diff --playerprg player.prg -i 0x20 -t 0x10 -l 0x0800 -s 0x7e -c 100000 --overwrite --ignoresidregs -g 0xfe-0xff -v

clean:
	rm -f player.prg
	rm -f src/player.sym
	rm -f sidulator
	rm -f music_2_0800.diff

.PHONY: all clean
