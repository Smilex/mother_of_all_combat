.PHONY: all

CC ?= g++
LIBS = -Wl,-rpath,lib64 -Llib64 -lraylib

all:
	$(CC) -o moac_linux main.cpp $(LIBS)
