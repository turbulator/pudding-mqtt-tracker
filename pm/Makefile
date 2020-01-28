MCU=attiny13a
CC=avr-gcc
OBJCOPY=avr-objcopy
CFLAGS=-g -mmcu=$(MCU) -Os

all: pm.hex

pm.hex : main.o
	$(OBJCOPY) -R .eeprom -O ihex main.o pm.hex 

main.o : main.c
	$(CC) $(CFLAGS) -Os -o main.o main.c

clean:
	rm -f *.hex *.o
