CC = gcc
CFLAGS = -g -pthread -std=c99 -lm
LFLAGS = 
LIBS   = 

SRCS = csr.c el.c 
OBJS = $(SRCS:.c=.o)

TARGETS = el2csr csr2el rand_graph

all: $(TARGETS) 

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

el2csr: $(OBJS) el2csr.c
	$(CC) $(CFLAGS)	$(INCLUDES) -o el2csr el2csr.c $(OBJS) $(LFLAGS) $(LIBS)

csr2el: $(OBJS) csr2el.c
	$(CC) $(CFLAGS)	$(INCLUDES) -o csr2el csr2el.c $(OBJS) $(LFLAGS) $(LIBS)

rand_graph: rand_graph.c graph.h 
	$(CC) $(CFLAGS) rand_graph.c -o rand_graph

clean:
	-rm rand_graph 
	-rm el_canon 
	-rm csr2el 
	-rm el2csr 
	-rm *.o
