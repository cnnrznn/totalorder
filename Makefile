BIN= prj1
OBJS= channel.o
INCS= channel.h messages.h

all: prj1

prj1: main.c $(OBJS) $(INCS)
	gcc main.c \
		$(OBJS) \
		-o prj1

channel.o: channel.c $(INCS)
	gcc -c channel.c

clean:
	rm -f $(BIN) $(OBJS)
