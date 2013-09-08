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
#include <string>
#include <math.h>
#include <vector>
#include <complex>

using namespace std;

// preprocessor definitions

#define PI 		3.14159265358979
#define TWOPI	6.28318530717959

// global constants

struct point{ int x,y; };

#include "graphics.cc"
#include "world.cc"
#include "load_save.cc"
#include "skill.cc"
#include "use.cc"
#include "talk.cc"
#include "combat.cc"
#include "sprite.cc"
#include "draw.cc"
#include "map.cc"
#include "interface.cc"
#include "update.cc"



int main(int argc, char *argv[]){ 

	setup_graphics();
//	usleep(100000);
	XFlush(display);

	
	world W;
	
	W.initialize();
	W.load_sprites();

	W.draw();
	W.user_interface();
	
	return(0);
};
