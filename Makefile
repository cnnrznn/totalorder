BIN= prj1

all: prj1

prj1: main.c
	gcc main.c \
		-o prj1

clean:
	rm $(BIN)
