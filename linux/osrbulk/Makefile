#------------------------------------------------------------------------------
# Copyright (C) Robin Callender
#
# Makefile for the osrbulk program.
#------------------------------------------------------------------------------

CC      = gcc
CFLAGS  = -g -O2 -Wall

OBJS    = osrbulk.o

all:    Makefile osrbulk

osrbulk:  $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
        
%.o: %.c 
	$(CC) -c $(CFLAGS) -o $@ $<

clean: 
	@rm -f osrbulk osrbulk.o
