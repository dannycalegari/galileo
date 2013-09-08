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

void world::update_map(){	// only update region centered on avatar, for speed
	int i,j,k;
	int x,y;
	point p;
	
	moves++;
	
	/* 
		Since each animal can only move once per turn, if it moves, it is given
		code index-100 on the flora/fauna map; thus it won't be moved again.
		
		After all moves have taken place, 100 is added back to the index.
	*/
	
	for(i=P.x-9;i<=P.x+9;i++){		// could we update a bigger window?
		for(j=P.y-9;j<=P.y+9;j++){
			if(0<i && i<999 && 0<j && j<799){	// if in range
			
			switch(flora_fauna_map[i][j])	{
				case -1:		// empty
					break;
				case 0:		// tree
					break;
				case 1:		// fruit tree
					break;		
				case 2:		// deer
					k=rand()%100;
					if(k<50){	// move away from avatar
						p=towards_avatar(i,j);	// direction of avatar
						x=i-p.x;
						y=j-p.y;
					} else {	// random move
						x=i+(rand()%3-1);
						y=j+(rand()%3-1);
					};
					if(world_map[x][y]>0 && world_map[x][y]<4 && flora_fauna_map[x][y]==-1){	// move
						flora_fauna_map[x][y]=2-100;	// code for moved deer
						flora_fauna_map[i][j]=-1;
					};
					break;
				case 3:		// fish
					k=rand()%100;
					if(k>50){
						x=i+(rand()%3-1);
						y=j+(rand()%3-1);
						if(world_map[x][y]==0 && flora_fauna_map[x][y]==-1){
							flora_fauna_map[x][y]=3-100;	// code for moved fish
							flora_fauna_map[i][j]=-1;
							// random move
						};
					};
					break;
				case 4:		// bear
						k=rand()%100;
					if(k<50){	// move towards avatar
						p=towards_avatar(i,j);	// direction of avatar
						x=i+p.x;
						y=j+p.y;
					} else	{	// random move
						x=i+(rand()%3-1);
						y=j+(rand()%3-1);
					};
					if(world_map[x][y]>0 && world_map[x][y]<4 && flora_fauna_map[x][y]==-1){	// move
						flora_fauna_map[x][y]=4-100;	// code for moved bear
						flora_fauna_map[i][j]=-1;
					};
					break;
				default:
					break;
			};
			
			};
		};
	};
	
	for(i=P.x-9-3;i<=P.x+9+3;i++){	// forget move status by adding back 100; 
		// examine wider square since something might have moved into it
		for(j=P.y-9-3;j<=P.y+9+3;j++){
			if(0<i && i<999 && 0<j && j<799){	// if in range
				if(flora_fauna_map[i][j]<-90){
					flora_fauna_map[i][j]=flora_fauna_map[i][j]+100;
				};
			};
		};
	};

};