/* direction.cc		functions to determine location, direction, pathfinding */

int world::occupied_by_special(int x, int y){	
	// is there a monster/npc in location x,y, and if so, which monster/npc?
	int i;
	int occupied;
	occupied=-1;
	if(in_combat==true){
		for(i=0;i<(int) monsters.size();i++){
			if(monsters[i].x==x && monsters[i].y==y){
				occupied=i;
			};	
		};
	} else {
		for(i=0;i<(int) npcs.size();i++){
			if(npcs[i].x==x && npcs[i].y==y){
				occupied=i;
			};	
		};	
	};
	return(occupied);
};

int world::special_in_direction(int x, int y){ // is there a monster/npc in direction x,y, and if so, which one?
	int i,j;
	int occupied;
	occupied=-1;
	i=P.x;
	j=P.y;
	bool in_range;
	in_range=true;
	while(in_range){
		i=i+x;
		j=j+y;
		occupied=occupied_by_special(i,j);
		if(occupied>-1){
			in_range=false;
		};
		if(i<1 || i>(int) world_map.size() || j<1 || j>(int) world_map[0].size()){
			in_range=false;
		};
	};
	return(occupied);
};

point world::towards_object(int i, int j, int type){
	// returns relative coordinates of nearest type in flora/fauna map
	// or npc roster if there is one within UPDATE_WINDOW, or 0,0 if none
	point p;
	int closest,a,b,c;

	if(type==99){
		p=new_point(P.x,P.y)-new_point(i,j);
	} else {
		p=new_point(0,0);
		closest=(2*UPDATE_WINDOW)+1;
		for(a=-UPDATE_WINDOW;a<=UPDATE_WINDOW;a++){		// look for type in flora/fauna map
			for(b=-UPDATE_WINDOW;b<=UPDATE_WINDOW;b++){
				if(i+a>0 && i+a < (int) world_map.size() && j+b >0 && j+b < (int) world_map.size()){
					if(flora_fauna_map[i+a][j+b]==type && norm(a,b)<closest){
						p=new_point(a,b);
						closest = norm(a,b);
					};
				};
			};
		};
		for(c=0;c<(int) npcs.size();c++){		// look for type in npc roster
			if(npcs[c].id==type){	// is this who we're looking for?
				a=npcs[c].x-i;
				b=npcs[c].y-j;
				if(norm(a,b)<closest){
					p=new_point(a,b);
					closest = norm(a,b);
				};
			};
		};
	};
	return(p);
};

point world::towards_object(int i, int j, int type_lower, int type_upper){
	// returns relative coordinates of nearest type IN RANGE in flora/fauna map
	// or npc roster if there is one within UPDATE_WINDOW, or 0,0 if none
	point p;
	int closest,a,b,c;

	p=new_point(0,0);
	closest=(2*UPDATE_WINDOW)+1;
	for(a=-UPDATE_WINDOW;a<=UPDATE_WINDOW;a++){		// look for type in flora/fauna map
		for(b=-UPDATE_WINDOW;b<=UPDATE_WINDOW;b++){
			if(i+a>0 && i+a < (int) world_map.size()-1 && j+b >0 && j+b < (int) world_map.size()-1){
				if(type_lower <= flora_fauna_map[i+a][j+b] && flora_fauna_map[i+a][j+b]<=type_upper && norm(a,b)<closest){
					p=new_point(a,b);
					closest = norm(a,b);
				};
			};
		};
	};
	for(c=0;c<(int) npcs.size();c++){		// look for type in npc roster
		if(type_lower <= npcs[c].id && npcs[c].id <= type_upper){	// is this who we're looking for?
			a=npcs[c].x-i;
			b=npcs[c].y-j;
			if(norm(a,b)<closest){
				p=new_point(a,b);
				closest = norm(a,b);
			};
		};
	};
	
	return(p);
};

point world::best_free_direction(int i, int j, point desired_move, int type){
	// person at (i,j) of type wants to move towards (i,j)+desired_move; this function
	// returns the best 1-step move available, or (0,0) if none.
	point p[4];
	point q;
	point desired_direction;
	int a;
	
	if(rand()%2==0){	// randomize initial order in case of tiebrakers
		p[0]=new_point(1,0);
		p[1]=new_point(-1,0);
		p[2]=new_point(0,1);
		p[3]=new_point(0,-1);
	} else {
		p[0]=new_point(0,-1);	
		p[1]=new_point(0,1);
		p[2]=new_point(-1,0);
		p[3]=new_point(1,0);
	};
	
	desired_direction=sign(desired_move);	// desired direction
	// bubble sort p[0] to p[3] by how close they are to desired direction
	a=0;
	while(a<3){
		if(l2norm(p[a]-desired_direction)>l2norm(p[a+1]-desired_direction)){
			q=p[a];
			p[a]=p[a+1];
			p[a+1]=q;
			if(a>0){
				a--;
			};
		} else {
			a++;
		};
	};
	
	// test sorted points to find first free one
	a=0;
	while(a<4){
		if(can_move_into_square(type, i+p[a].x, j+p[a].y)){
			return(p[a]);
		} else {
			a++;
		};
	};
	return(new_point(0,0));
};

point world::fancy_best_free_direction(int i, int j, point desired_move, int type, int range){
	int dist[2*range+1][2*range+1];	
	int a,b;
	int x,y,z,w;
	int count;
	for(a=0;a<2*range+1;a++){	// initialize
		for(b=0;b<2*range+1;b++){
			if(can_move_into_square(type, i+a-range, j+b-range)){
				dist[a][b]=100;	
			} else {
				dist[a][b]=400;
			};
		};
	};
	if(norm(desired_move)<=1){	// adjacent or on top
		return(new_point(0,0));	// already there
	} else if(abs(desired_move.x)>range || abs(desired_move.y)>range){
		return(new_point(0,0));	// out of range
	} else {
		dist[range+desired_move.x][range+desired_move.y]=0;
		count=0;
		while(count<=range*2){
			count++;
			for(a=1;a<2*range;a++){
				for(b=1;b<2*range;b++){
					if((a==range && b==range) || dist[a][b]==100){
						x=dist[a+1][b];
						y=dist[a-1][b];
						z=dist[a][b-1];
						w=dist[a][b+1];
						if(x<=y && x<=z && x<=w && x<100){
							dist[a][b]=x+1;
						} else if(y<=z && y<=w && y<100) {
							dist[a][b]=y+1;
						} else if(z<=w && z<100){
							dist[a][b]=z+1;
						} else if(w<100){
							dist[a][b]=w+1;
						};
					};
				};
			};
		};
		if(dist[range][range]>=100){
			return(new_point(0,0));
		} else {
			x=dist[range+1][range];
			y=dist[range-1][range];
			z=dist[range][range-1];
			w=dist[range][range+1];
			if(x<=y && x<=z && x<=w){
				return(new_point(1,0));
			} else if(y<=z && y<=w) {
				return(new_point(-1,0));
			} else if(z<=w){
				return(new_point(0,-1));
			} else {
				return(new_point(0,1));
			};
		};
	};
};

