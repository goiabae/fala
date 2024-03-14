CFLAGS += -ggdb -Wall -Wextra

all: prepare fala

fala: build/parser.o build/main.o build/lexer.o
	$(CC) $(CFLAGS) -o build/$@ $^

build/main.o: src/main.c src/lexer.h
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.o: src/%.c
	$(CC) $(CFLAGS)  -c -o $@ $<

src/parser.c: src/parser.y src/lexer.h
	bison --header=src/parser.h --output=$@ $<

src/lexer.c src/lexer.h: src/lexer.l
	flex --header-file=src/lexer.h --outfile=src/lexer.c $<

prepare:
	mkdir -p build

clean:
	rm -rf build
	rm -f src/lexer.c
	rm -f src/lexer.h
	rm -f src/parser.c
	rm -f src/parser.h

.PHONY: prepare all
