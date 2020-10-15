.PHONY: all

CC ?= g++
LIBS = -Wl,-rpath,lib64 -Llib64 -lraylib -lm

all:
	$(CC) -Wno-enum-compare -Wno-narrowing -o moac_linux src/main.cpp $(LIBS)
