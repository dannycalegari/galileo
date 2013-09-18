/* move.cc	movement routines */

bool world::can_move_into_square(int w, int x, int y){	 // can move into square; 
	// x,y must be in range; w is type of mover
	// w=0 avatar, w=1 animal/npc, w=2 fish/pirate ship, w=3 goat/mountaineer
	
	bool can_move;
	if(is_in_range(x,y,0)==false){	// out of range check
		return(false);
	};
	
//	can_move=true;	// default
	switch(w){
		case 0:		// avatar
			if(embarked){	// embarked?
				if (world_map[x][y]==0){	// moving onto water?
					can_move=true;
				} else {
					can_move=false;
				};
			} else if (world_map[x][y]==0){	// moving onto water?
				if(flora_fauna_map[x][y]==60){	// moving onto boat?
					can_move=true;
				} else {
					can_move=false;
				};
			} else if (world_map[x][y]>4){	// high mountain
				can_move=false;
			} else if (world_map[x][y]==4){	// low mountain
				if(have_object("grapple")){	// have grapple?
					can_move=true;
				} else {
					can_move=false;
				};
			} else if((int) wall_map.size()>0){	// is there a wall layer?
				if(wall_map[x][y]>-1){	// is there a wall?
					can_move=false;
				} else if(occupied_by_special(x,y)>-1){	// occupied by monster/npc
					can_move=false;
				} else {
					can_move=true;
				};
			} else if(occupied_by_special(x,y)>-1){	// occupied by monster/npc
				can_move=false;
			} else {
				can_move=true;
			};
			break;
		case 1:		// animal/npc
			if(world_map[x][y]==0 || world_map[x][y]>=4){	// water or mountain
				can_move=false;
			} else {
				if(x==P.x && y==P.y){	// move onto avatar
					can_move=false;
				} else if(flora_fauna_map[x][y]!=-1){	// occupied
					can_move=false;				
				} else if((int) wall_map.size()>0){	// is there a wall layer?
					if(wall_map[x][y]>-1){	// is there a wall?
						can_move=false;
					} else if(occupied_by_special(x,y)>-1){	// occupied by monster/npc
						can_move=false;
					} else {
						can_move=true;
					};
				} else if(occupied_by_special(x,y)>-1){	// occupied by monster/npc
					can_move=false;
				} else {
					can_move=true;
				};
			};
			break;
		case 2:		// fish/pirate ship
			if(world_map[x][y]!=0){	// land
				can_move=false;
			} else {
				if(x==P.x && y==P.y){	// move onto avatar
					can_move=false;
				} else if(flora_fauna_map[x][y]!=-1){	// occupied
					can_move=false;				
				} else {
					can_move=true;
				};
			};		
			break;
		case 3:		// goat/mountaineer
			if(world_map[x][y]==0 || world_map[x][y]>=5){	// water or high mountain
				can_move=false;
			} else {
				if(x==P.x && y==P.y){	// move onto avatar
					can_move=false;
				} else if(flora_fauna_map[x][y]!=-1){	// occupied
					can_move=false;				
				} else if(wall_map.size()>0){	// is there a wall layer?
					if(wall_map[x][y]>-1){	// is there a wall?
						can_move=false;
					} else if(occupied_by_special(x,y)>-1){	// occupied by monster/npc
						can_move=false;
					} else {
						can_move=true;
					};
				} else {
					can_move=true;
				};
			};
			break;		
		default:
			break;
	};
	return(can_move);
};

void world::attempt_move(int x, int y){	// avatar attempts to move to relative location x,y
	int X,Y;
	X=P.x+x;
	Y=P.y+y;
	bool can_move;

	if(is_in_range(X,Y,0)==false){	// exit city or combat map; or if in europe,
		if(in_city){
			exit_city();
		} else if(in_combat){
			exit_combat();
		} else {
			add_new_message("out of range!");
		};
	} else if(edit_mode){
		P.x=X;
		P.y=Y;
	} else {
		can_move=can_move_into_square(0,X,Y);
		cout.flush();
		if(can_move){
			if(world_map[X][Y]==0){	// moving onto water
				if(embarked==false){	// not embarked
					P.x=X;
					P.y=Y;
				} else if(rand()%20<P.dexterity){
					P.x=X;
					P.y=Y;				
				} else {
					add_new_message("capsize");
				};
			} else if(world_map[X][Y]==4){ // mountaineering
				if(rand()%60<P.dexterity){
					P.x=X;
					P.y=Y;
				} else {
					add_new_message("stumble");
				};
			} else {
				P.x=X;
				P.y=Y;
			};
		} else {
			add_new_message("blocked");
		};
	};
};

