#------------------------------------------------------------------------------
# Copyright (C) Robin Callender
#
# Makefile for the switch_events program.
#------------------------------------------------------------------------------

CC      = gcc
CFLAGS  = -g -O2 -Wall

OBJS    = switch_events.o

all:    Makefile switch_events

switch_events:  $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
        
%.o: %.c 
	$(CC) -c $(CFLAGS) -o $@ $<

clean: 
	rm -f *.o *~ switch_events
        
install:
	chmod 777 get_events
