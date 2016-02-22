#
# Simplest Makefile
# Created initially by Jack Waller on 2016-02-18
#
# Please install ENet from "https://github.com/lsalzman/enet" firstly
#

CC          = gcc
CFLAGS      = -I/usr/local/include -Wall -g -O0
LDFLAGS     = -L/usr/local/lib
DEPLIBS     = -lenet -lpthread

DEPS = \
       config.h


%.o:%.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@


SLICE_OBJS = \
        slice.o


PEER_OBJS = \
        peer.o


all: slice peer

slice: $(SLICE_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SLICE_OBJS) $(DEPLIBS)

peer: $(PEER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(PEER_OBJS) $(DEPLIBS)

clean:
	rm -f *.o slice peer

