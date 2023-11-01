FLAGS=-pedantic -Wall -Wextra -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization -Wformat=2 -Wuninitialized -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Wold-style-cast -Woverloaded-virtual -Wredundant-decls -Wsign-conversion -Wsign-promo -Wstrict-overflow=5 -Wundef -Werror -Wno-unused -Wno-unused-parameter
CXX ?= gcc

all: quash
	./quash

quash: quash.cpp
	g++ -g -o $@ $<

quashFlags: quash.cpp
	g++ -g $(FLAGS) -o $@ $<