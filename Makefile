.PHONY: all

CXX ?= g++
LIBS = -Wl,-rpath,lib64 -Llib64 -lraylib -lm

all:
	$(CXX) -g -Wno-enum-compare -Wno-narrowing -o moac_linux src/main.cpp $(LIBS)
