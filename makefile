CC=gcc
DEPS = header.h
OBJ = built_in.o init.o shell.o execute_cmd.o parser.o redirect.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

shell: $(OBJ)
		gcc -o $@ $^ $(CFLAGS)
