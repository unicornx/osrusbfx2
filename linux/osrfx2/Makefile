#------------------------------------------------------------------------------
# Copyright (C) Robin Callender
#
# Makefile for the osrfx2 program.
#------------------------------------------------------------------------------

CC      = gcc
CFLAGS  = -g -O2 -Wall

OBJS    = osrfx2.o

all:    Makefile osrfx2

osrbulk:  $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
        
%.o: %.c 
	$(CC) -c $(CFLAGS) -o $@ $<

clean: 
	@rm -f osrfx2 osrfx2.o
