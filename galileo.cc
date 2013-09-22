/*	galileo.cc

	Version 0.1

	Game with interface loosely based on Ultima IV
	
	version 0.1 August 18 2013; started work
	
	Copyright Danny Calegari

	released under the terms of the GNU GPL, version 3.0
	see attached licence for more details
*/

// standard libraries to include

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h> 
#include <string>
#include <math.h>
#include <vector>
#include <complex>

// X11 libraries to include

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/xpm.h>

using namespace std;

// preprocessor definitions

#define PI 		3.14159265358979
#define TWOPI	6.28318530717959
double theta;	// global function

// global constants

#define MAX_SPRITE_NUM 100
#define MAX_SKILL_NUM 6
#define MAX_WALL_NUM 35
#define MAX_MESSAGE_NUM 10
#define UPDATE_WINDOW 39
#define MAX_TEXT_WIDTH 30
#define SPAWN_RATE 10

#include "point.cc"
#include "graphics.cc"
#include "world.cc"
#include "info.cc"
#include "load_save.cc"
#include "move.cc"
#include "use.cc"
#include "talk.cc"
#include "combat.cc"
#include "sprite.cc"
#include "draw.cc"
#include "map.cc"
#include "interface.cc"
#include "update.cc"
#include "npc.cc"
#include "goal.cc"
#include "direction.cc"

int main(int argc, char *argv[]){ 

	setup_graphics();
	XFlush(display);
  	XSetInputFocus(display, win, RevertToNone, CurrentTime);	// doesn't seem to work?
	
	world W;
	
	W.initialize();
	W.load_sprites();

	W.draw();
	W.user_interface();
	
	return(0);
};
