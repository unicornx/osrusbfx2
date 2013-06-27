#------------------------------------------------------------------------------
# Makefile for the osrfx2 test program.
#------------------------------------------------------------------------------
PWD    := $(shell pwd)
INCLUDE_DIR=$(PWD)/../include
CC      = gcc
CFLAGS  = -g -O2 -Wall -I$(INCLUDE_DIR)

OBJS    = osrfx2.o

all:    Makefile osrfx2

osrfx2:  $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
        
%.o: %.c 
	$(CC) -c $(CFLAGS) -o $@ $<

clean: 
	@rm -f osrfx2 osrfx2.o
