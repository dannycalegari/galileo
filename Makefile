CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter

# macOS or Linux: the system's OpenGL and sound libraries, and how to install what is missing
SYSTEM := $(shell uname -s)
ifeq ($(SYSTEM),Darwin)
SYS_CFLAGS :=
SYS_LIBS   := -framework OpenGL -framework CoreAudio -framework AudioToolbox -framework CoreFoundation
GET_PKGCONFIG := brew install pkg-config
GET_LIBS      := brew install glfw cairo
else
SYS_CFLAGS := -DGL_GLEXT_PROTOTYPES
SYS_LIBS   := -lGL -ldl -lpthread -lm
GET_PKGCONFIG := install pkg-config (Debian or Ubuntu: sudo apt install pkg-config)
GET_LIBS      := install glfw and cairo with their headers (Debian or Ubuntu: sudo apt install libglfw3-dev libcairo2-dev)
endif

# pkg-config gives the compiler and linker the flags of glfw and cairo; without it, or without them, the link would
# fail with a long list of undefined symbols, so stop first and say what is missing
ifneq ($(MAKECMDGOALS),clean)
ifeq ($(shell command -v pkg-config 2>/dev/null),)
$(error pkg-config is not installed: $(GET_PKGCONFIG) (then make again))
endif
ifneq ($(shell pkg-config --exists glfw3 cairo && echo ok),ok)
$(error pkg-config cannot find glfw3 and cairo: $(GET_LIBS) (then make again))
endif
endif
GUI_CFLAGS := $(shell pkg-config --cflags glfw3 cairo) $(SYS_CFLAGS)
GUI_LIBS   := $(shell pkg-config --libs glfw3 cairo) $(SYS_LIBS)

SRC := src/world.cc src/city.cc src/under.cc src/people.cc src/items.cc src/animals.cc src/mesh.cc src/render.cc src/hud.cc src/main.cc \
       src/audio.cc src/sound.cc
OBJ := $(SRC:src/%.cc=build/%.o) build/miniaudio.o
HDR := $(wildcard src/*.h)

all: galileo

build/%.o: src/%.cc $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(GUI_CFLAGS) -c $< -o $@

# the music and the sounds play through miniaudio, with stb_vorbis for the Ogg files (src/ext: other people's code,
# compiled once, as C, without our warnings)
build/miniaudio.o: src/ext/miniaudio_impl.c src/ext/miniaudio.h src/ext/stb_vorbis.c
	@mkdir -p build
	$(CC) -O2 -w -c $< -o $@

galileo: $(OBJ)
	$(CXX) $(OBJ) $(GUI_LIBS) -o $@

clean:
	rm -rf build galileo

.PHONY: all clean
