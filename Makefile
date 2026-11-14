CXX      ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
GUI_CFLAGS := $(shell pkg-config --cflags glfw3 cairo)
GUI_LIBS   := $(shell pkg-config --libs glfw3 cairo) -framework OpenGL -framework AVFoundation -framework Foundation

SRC := src/world.cc src/city.cc src/under.cc src/people.cc src/items.cc src/animals.cc src/mesh.cc src/render.cc src/hud.cc src/main.cc
OBJ := $(SRC:src/%.cc=build/%.o) build/audio.o build/sound.o
HDR := $(wildcard src/*.h)

all: galileo

build/%.o: src/%.cc $(HDR)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(GUI_CFLAGS) -c $< -o $@

# the music player is Objective-C++ (Apple's AVFoundation)
build/audio.o: src/audio.mm src/audio.h
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -fobjc-arc -c $< -o $@

build/sound.o: src/sound.mm src/sound.h
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -fobjc-arc -c $< -o $@

galileo: $(OBJ)
	$(CXX) $(OBJ) $(GUI_LIBS) -o $@

clean:
	rm -rf build galileo

.PHONY: all clean
