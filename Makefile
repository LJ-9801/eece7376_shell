CC=gcc
DEPS = $(wildcard src/*.h)
OBJ = $(patsubst %.c, %.o, $(wildcard *.c))
EXECUTABLE=shell

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(EXECUTABLE): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(OBJ) $(EXECUTABLE)