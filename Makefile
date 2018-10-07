BIN= prj1
OBJS= channel.o queue.o
INCS= channel.h messages.h

all: prj1

prj1: main.c $(OBJS) $(INCS)
	gcc -pg -g \
		main.c \
		$(OBJS) \
		-o prj1

channel.o: channel.c $(INCS)
	gcc -c -g -pg channel.c

queue.o: queue.c $(INCS)
	gcc -c -g -pg queue.c

clean:
	rm -f $(BIN) $(OBJS)
