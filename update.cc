/* update.cc 	animals, predators, fish and npcs move	*/

void world::update_map(){	// only update region centered on avatar, for speed
	int i,j,k,l,m;
	int x,y;
	point p;
	point desired_move;
	stringstream T;
	bool accessible_goal;
	
	moves++;
	
	/* 
		Since each animal can only move once per turn, if it moves, it is given
		code index-100 on the flora/fauna map; thus it won't be moved again.
		
		After all moves have taken place, 100 is added back to the index.
	*/
	
	if(rand()%SPAWN_RATE==0 && in_combat==false && in_city==false){	
		spawn_random_flora_fauna();
	};
	
	for(i=P.x-UPDATE_WINDOW;i<=P.x+UPDATE_WINDOW;i++){		// could we update a bigger window?
		for(j=P.y-UPDATE_WINDOW;j<=P.y+UPDATE_WINDOW;j++){
			if(is_in_range(i,j,0)){		// if in range
			
				x=i;	// default is to stay still
				y=j;
				
				switch(flora_fauna_map[i][j])	{
					case -1:		// empty
						break;					
					case 4:		// sapling
						k=rand()%1000;
						if(k==0){
							flora_fauna_map[i][j]=0;	// grows into tree
						};
						break;
					case 11:	// boar
					case 12:	// goat
					case 13:	// deer
						if(flora_fauna_map[i][j]==12){
							l=3;	// move type of goat
						} else {
							l=1;	// move type of boar/deer
						};
						desired_move=-towards_object(i,j,99);	// move away from avatar
						p=best_free_direction(i, j, desired_move, l);
						if(norm(p)==1){
							assert(can_move_into_square(l,i+p.x,j+p.y));
							flora_fauna_map[i+p.x][j+p.y]=flora_fauna_map[i][j]-100;	// code for moved animal
							flora_fauna_map[i][j]=-1;
						} else {
							assert(norm(p)==0);
						};
						break;
					case 10:		// cow
					case 14:		// fish
					case 50:		// generic farmer
					case 51:		// generic merchant
						if(flora_fauna_map[i][j]==14){
							l=2;	// move type of fish
						} else {
							l=1;	// move type of cow
						};
						k=rand()%100;
						if(k>50){
							p=rand_point();	// random move half the time
						} else {
							p=new_point(0,0);	// stay still otherwise
						};
						if(can_move_into_square(l,i+p.x,j+p.y)==true){		// can we move?
							flora_fauna_map[i+p.x][j+p.y]=flora_fauna_map[i][j]-100;	// code for moved fish
							flora_fauna_map[i][j]=-1;
						};
						break;
					case 15:		// dog
						desired_move=towards_object(i,j,99);	// move towards avatar
				//		p=best_free_direction(i, j, desired_move, 1);
						p=fancy_best_free_direction(i, j, desired_move, 1, 20);	// rudimentary pathfinding

						if(norm(p)==1){
							assert(can_move_into_square(1,i+p.x,j+p.y));
							flora_fauna_map[i+p.x][j+p.y]=flora_fauna_map[i][j]-100;	// code for moved animal
							flora_fauna_map[i][j]=-1;
						} else {
							assert(norm(p)==0);
						};
						if(rand()%10==0){
							add_popup_message(i+p.x-P.x,j+p.y-P.y,"woof!");
						};
						break;
					case 20:	// bear
					case 21:	// wolf
					case 53:	// robber
						desired_move=towards_object(i,j,99);	// move towards avatar
						if(norm(desired_move)==1){	// if adjacent
							enter_combat(flora_fauna_map[i][j]);
						} else {
							p=best_free_direction(i,j,desired_move,1);
							if(norm(p)==1){
								assert(can_move_into_square(1,i+p.x,j+p.y));
								flora_fauna_map[i+p.x][j+p.y]=flora_fauna_map[i][j]-100;	// code for moved animal
								flora_fauna_map[i][j]=-1;
							} else {
								assert(norm(p)==0);
							};				
							if(rand()%10==0){
								add_popup_message(i+p.x-P.x,j+p.y-P.y,"grrr!");
							};
						};
					default:
						break;
				};
			};
		};
	};
	
	for(i=P.x-UPDATE_WINDOW-3;i<=P.x+UPDATE_WINDOW+3;i++){	// forget move status by adding back 100; 
		// examine wider square since something might have moved into it
		for(j=P.y-UPDATE_WINDOW-3;j<=P.y+UPDATE_WINDOW+3;j++){
			if(0<=i && i<(int) world_map.size() && 0<=j && j<(int) world_map[0].size()){	// if in range
				if(flora_fauna_map[i][j]<-1){	// code of moved animal
					flora_fauna_map[i][j]=flora_fauna_map[i][j]+100;
				};
			};
		};
	};
	
	if(in_combat){	// move monsters!
		for(l=0;l<(int) monsters.size();l++){
			if(monsters[l].ranged_attack==true || norm(towards_object(monsters[l].x,monsters[l].y,99))==1){
				// ranged attack, or adjacent?
				// attack!
				add_new_message("monster swipes!");
			} else {	// move towards avatar!		
				desired_move=towards_object(monsters[l].x,monsters[l].y,99);
				p=best_free_direction(monsters[l].x, monsters[l].y, desired_move, 1);
				if(norm(p)==1){
					assert(can_move_into_square(1,monsters[l].x+p.x,monsters[l].y+p.y));
					monsters[l].x=monsters[l].x+p.x;	// move!
					monsters[l].y=monsters[l].y+p.y;
				} else {
					assert(norm(p)==0);
				};	
			};
		};
	} else {	// move npcs; goalless npcs have a "heading" which makes their movement look purposeful
		l=0;
		while(l<(int) npcs.size()){
			desired_move=new_point(0,0);
			accessible_goal=false;

			if(npcs[l].goal==100){	// goal 100 special case
				desired_move=towards_object(npcs[l].x,npcs[l].y,100,110);
			} else if (npcs[l].goal>-1){
				desired_move=towards_object(npcs[l].x,npcs[l].y,npcs[l].goal);
			};
			if(norm(desired_move+new_point(npcs[l].x,npcs[l].y)-new_point(npcs[l].cx,npcs[l].cy))>npcs[l].d){	// is goal too far away?
				accessible_goal=false;
			} else if(norm(desired_move)>0){	// nearby goal
				accessible_goal=true;
				if(norm(desired_move)==1){	// adjacent to goal
					achieve_goal(l, npcs[l].goal, desired_move);	// achieve goal
					npcs[l].goal=update_goal(npcs[l].id, npcs[l].goal);	// update goal
				} else {	// move towards goal
					p=fancy_best_free_direction(npcs[l].x,npcs[l].y,desired_move,1,20);	// pathfinding
					if(norm(p)==1){
						assert(can_move_into_square(1,npcs[l].x+p.x,npcs[l].y+p.y));
						npcs[l].x=npcs[l].x+p.x;	// move!
						npcs[l].y=npcs[l].y+p.y;
					} else {
						assert(norm(p)==0);
						accessible_goal=false;
					};
				};
			};
			
			if(accessible_goal==false){	// no goal or no accessible goal
				if(rand()%5==0){
					m=npcs[l].hx;				// turn heading left
					npcs[l].hx=npcs[l].hy;
					npcs[l].hy=-1*m;
				};
				x=npcs[l].x+npcs[l].hx;	// move in heading direction
				y=npcs[l].y+npcs[l].hy;	// move in heading direction
				if(can_move_into_square(1,x,y)==true){	// square is free
					if(norm(new_point(x,y)-new_point(npcs[l].cx,npcs[l].cy))>npcs[l].d){
						// too far from center of gravity
						npcs[l].hx=-1*npcs[l].hx;	// about face
						npcs[l].hy=-1*npcs[l].hy;
					} else {
						npcs[l].x=x;
						npcs[l].y=y;
					};
				} else {
					m=npcs[l].hx;				// turn heading right
					npcs[l].hx=-1*npcs[l].hy;
					npcs[l].hy=m;
				};
			};
			T.str("");
			T << npcs[l].goal;
			add_popup_message(npcs[l].x-P.x,npcs[l].y-P.y,T.str());
			if(npcs[l].x==0 && npcs[l].y==0){	// npc "dead"
				remove_npc(l);
				l--;
			};
			l++;
		};
	};
};