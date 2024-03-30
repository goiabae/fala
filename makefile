CC = cc
CFLAGS += -std=c11
CFLAGS += -O0 -ggdb
CFLAGS += -Wall -Wextra -pedantic -Wmissing-declarations
# CFLAGS += -D FALA_WITH_REPL
LIBS =
# LIBS += -lreadline -lhistory

all: prepare fala

fala: build/parser.o build/main.o build/lexer2.o build/ast.o build/eval.o build/compiler.o build/env.o
	$(CC) $(CFLAGS) $(LIBS) -o build/$@ $^

build/main.o: src/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

build/%.o: src/%.c src/%.h
	$(CC) $(CFLAGS)  -c -o $@ $<

build/%.o: src/%.c
	$(CC) $(CFLAGS)  -c -o $@ $<

src/parser.c src/parser.h: src/parser.y
	bison --header=src/parser.h --output=$@ $<

src/lexer.c: src/lexer.l
	flex --outfile=src/lexer.c $<

prepare:
	mkdir -p build

clean:
	rm -rf build
	rm -f src/lexer.c
	rm -f src/parser.c
	rm -f src/parser.h

.PHONY: prepare all
