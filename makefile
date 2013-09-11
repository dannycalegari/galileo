CC=g++
CFLAGS=-g -Wall -O3 -fast
IFLAGS=-I/usr/X11R6/include
LFLAGS=-L/usr/X11R6/lib -lX11 -lXpm
all: galileo

galileo: galileo.cc graphics.cc world.cc info.cc sprite.cc interface.cc map.cc draw.cc update.cc load_save.cc use.cc talk.cc combat.cc skill.cc
	$(CC) $(CFLAGS) $(IFLAGS) -o galileo galileo.cc $(LFLAGS) -lm

clean: rm galileo
