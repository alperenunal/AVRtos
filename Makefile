CFLAGS = -Wall -Wextra -Wpedantic \
		 -DF_CPU=16000000UL -DBAUD=9600 \
		 -mmcu=atmega328p \
		 -Iinclude/

LDFLAGS = -Wl,--defsym=__heap_end=0x8007ff \
		  -lm -lprintf_flt -lscanf_flt

SRC = $(wildcard src/*.c)

build: build-opt

build-opt: $(SRC)
	avr-gcc -c -Os $^ $(CFLAGS) $(LDFLAGS)

build-debug: $(SRC)
	avr-gcc -c -O0 -gdwarf-2 -g3 $^ $(CFLAGS) $(LDFLAGS)

libavrtos.a: build
	avr-ar -crs libavrtos.a *.o

%.elf: %.c libavrtos.a
	avr-gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

%.hex: %.elf
	avr-objcopy -O ihex -R .eeprom $< $@

examples/%: examples/%.c libavrtos.a
	avr-gcc -o $*.elf $^ $(CFLAGS) $(LDFLAGS)

qemu-run-%: %.elf
	qemu-system-avr -M uno -bios $< -nographic

qemu-debug-%: %.elf
	qemu-system-avr -M uno -bios $< -nographic -S -s

flash-%: %.hex
	avrdude -v -c arduino -p m328p -P /dev/ttyUSB0 -b 115200 -U flash:w:$<

docs: Doxyfile
	doxygen

read-docs: docs
	python3 -m http.server -d docs/html

clean:
	rm -f *.o *.a *.elf *.hex *.out
