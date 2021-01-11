.PHONY: test

OPT ?= -O3

CC = gcc
CFLAGS = -Iinclude -fPIC -Wall -Wextra -lc -lm -std=gnu99 -g $(OPT)
#LDFLAGS = -shared

# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

#On Linux:$ gcc -fPIC -std=gnu99 -c -o module.o module.c
#$ ld -o module.so module.o -shared -Bsymbolic -lc 
#OSX:$ gcc -dynamic -fno-common -std=gnu99 -c -o module.o module.c
#$ ld -o module.so module.o -bundle -undefined dynamic_lookup -lc
# Compile flags for linux / osx

ifeq ($(uname_S),Linux)
	LDFLAGS ?= -shared -Bsymbolic
else
	LDFLAGS ?= -bundle
endif

TARGET = tdigest.so
SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard include/*.h)
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $(LDFLAGS) -lm -lc $(OBJECTS)

noopt:
	$(MAKE) OPT="-O0"

clean:
	find . -type f -name '*.o' -delete
	find . -type f -name '*.so' -delete
	find . -type f -name '*.pyc' -delete

test: $(TARGET)
	py.test -v test