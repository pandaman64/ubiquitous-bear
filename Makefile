CC=gcc
CFLAGS=-Wall -Werror -g  
LDFLAGS=
LDLIBS= -lcurl -loauth -lcrypto -luv -ljansson 
SRCS=stream.c main.c
OBJS=$(SRCS:%.c=%.o)
DEPS=$(SRCS:%.c=%.d)
PROG=ubiquitous-bear

all: $(PROG)

-include $(DEPS)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS) 

.c.o:
	$(CC) $(CFLAGS) -c -MMD -MP -o $@ $< 

clean:
	rm -f $(PROG) $(OBJS) $(DEPS)

run: $(PROG)
	./$<
