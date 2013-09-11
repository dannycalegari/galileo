/* update.cc 	animals, predators, fish and npcs move	*/

void world::count_flora_fauna(){
	int i,j,c;
	for(i=0;i<5;i++){	// initialize count
		flora_fauna_count[i]=0;
	};
	
	for(i=10;i<990;i++){
		for(j=10;j<790;j++){
			c=flora_fauna_map[i][j];
			if(c>=0 && c<5){
				flora_fauna_count[c]++;
			};
		};
	};
};

point world::towards_avatar(int i, int j){
	point p;
	int x,y;
	
	x=P.x-i;
	y=P.y-j;
	
	if(x>0){
		p.x=1;
	} else if(x<0){
		p.x=-1;
	} else {
		p.x=0;
	};
	
	if(y>0){
		p.y=1;
	} else if(y<0){
		p.y=-1;
	} else {
		p.y=0;
	};
	return(p);
};

bool world::is_adjacent_to_avatar(int i, int j){
	int x,y;
	x=P.x-i;
	y=P.y-j;
	if(x<0){
		x=-x;
	};
	if(y<0){
		y=-y;
	};
	if(x+y<2){
		return(true);
	} else {
		return(false);
	};
};

void world::update_map(){	// only update region centered on avatar, for speed
	int i,j,k,l;
	int x,y;
	point p;
	
	moves++;
	
	/* 
		Since each animal can only move once per turn, if it moves, it is given
		code index-100 on the flora/fauna map; thus it won't be moved again.
		
		After all moves have taken place, 100 is added back to the index.
	*/
	
	for(i=P.x-UPDATE_WINDOW;i<=P.x+UPDATE_WINDOW;i++){		// could we update a bigger window?
		for(j=P.y-UPDATE_WINDOW;j<=P.y+UPDATE_WINDOW;j++){
			if(0<i && i<(int) world_map.size()-1 && 0<j && j<(int) world_map[0].size()-1){	// if in range
			
				x=i;	// default is to stay still
				y=j;
				
				switch(flora_fauna_map[i][j])	{
					case -1:		// empty
						break;
					case 0:		// tree
						break;
					case 1:		// fruit tree
						break;		
					case 2:		// deer
						p=towards_avatar(i,j);	// direction of avatar
						if(p.x!=0){
							x=i-p.x;
						} else if(p.y!=0){
							y=j-p.y;
						};
						if(can_move_into_square(1,x,y)==true){	// can we move?
							flora_fauna_map[x][y]=2-100;	// code for moved deer
							flora_fauna_map[i][j]=-1;
						} else {	// random move if trapped
							if(rand()%2==0){
								x=i+(rand()%3)-1;
								y=j;
							} else {
								x=i;
								y=j+(rand()%3)-1;
							};
							if(can_move_into_square(1,x,y)==true){	// can we move?
								flora_fauna_map[x][y]=2-100;	// code for moved deer
								flora_fauna_map[i][j]=-1;
							};
						};
						break;
					case 3:		// fish
						k=rand()%100;
						if(k>50){
							p.x=(rand()%3-1);
							p.y=(rand()%3-1);
							if(p.x!=0){
								x=i+p.x;
							} else if(p.y!=0){
								y=j+p.y;
							};
							if(can_move_into_square(2,x,y)==true){		// can we move?
								flora_fauna_map[x][y]=3-100;	// code for moved fish
								flora_fauna_map[i][j]=-1;
							};
						};
						break;
					case 4:		// bear
						if(is_adjacent_to_avatar(i,j)){
							enter_combat(4);
						} else {
							k=rand()%100;
							if(k<90){	// move towards avatar
								p=towards_avatar(i,j);	// direction of avatar
							} else	{	// random move
								p.x=(rand()%3-1);
								p.y=(rand()%3-1);
							};
							if(p.x!=0){
								x=i+p.x;
							} else if(p.y!=0){
								y=j+p.y;
							};
							if(can_move_into_square(1,x,y)==true){
								flora_fauna_map[x][y]=4-100;	// code for moved bear
								flora_fauna_map[i][j]=-1;				
							};
						};
						break;
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
				if(flora_fauna_map[i][j]<-90){
					flora_fauna_map[i][j]=flora_fauna_map[i][j]+100;
				};
			};
		};
	};
	
	if(in_combat){	// move monsters!
		for(l=0;l<(int) monsters.size();l++){
			if(monsters[l].ranged_attack==true || is_adjacent_to_avatar(monsters[l].x,monsters[l].y)==true){	// ranged attack, or adjacent?
				// attack!
				add_new_message("monster swipes!");
			} else {	// move towards avatar!			
				p=towards_avatar(monsters[l].x,monsters[l].y);
				if(p.x!=0){
					x=monsters[l].x+p.x;
					y=monsters[l].y;
				} else if(p.y!=0){
					x=monsters[l].x;
					y=monsters[l].y+p.y;
				} else {
					x=monsters[l].x;
					y=monsters[l].y;				
				};
				if(can_move_into_square(1,x,y)==true){	// do move!
					monsters[l].x=x;
					monsters[l].y=y;
				};
			};
		};
	};
};