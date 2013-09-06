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

void world::update_map(){
	int i,j,k;
	int x,y;
	int a,b,aa,bb;
	bool close_to_prey;
	bool ate_prey;
	
	moves++;
	
	for(i=10;i<990;i++){
		for(j=10;j<790;j++){
			
			switch(flora_fauna_map[i][j])	{
				case -1:		// empty
					break;
				case 0:		// tree
					break;
				case 1:		// fruit tree
					break;		
				case 2:		// deer
					k=rand()%100;
					if(k>50){
						x=i+(rand()%3-1);
						y=j+(rand()%3-1);
						if(world_map[x][y]>0 && world_map[x][y]<4 && flora_fauna_map[x][y]==-1){
							flora_fauna_map[x][y]=2;
							flora_fauna_map[i][j]=-1;
							// random move
						};
					};
					break;
				case 3:		// fish
					k=rand()%100;
					if(k>50){
						x=i+(rand()%3-1);
						y=j+(rand()%3-1);
						if(world_map[x][y]==0 && flora_fauna_map[x][y]==-1){
							flora_fauna_map[x][y]=3;
							flora_fauna_map[i][j]=-1;
							// random move
						};
					};
					break;
				case 4:		// bear
					close_to_prey=false;
					ate_prey=false;
					for(a=i-5;a<=i+5;a++){
						for(b=j-5;b<=j+5;b++){
							if(close_to_prey==false){
								if(flora_fauna_map[a][b]==2){
									close_to_prey=true;
									if(a<i){
										aa=-1;
									};
									if(a>i){
										aa=1;
									};
									if(a==i){
										aa=0;
									};
									if(b<j){
										bb=-1;
									};
									if(b>j){
										bb=1;
									};
									if(b==j){
										bb=0;
									};
								};
							};
						};
					};
					
					if(close_to_prey && rand()%3<2){
						x=i+aa;
						y=j+bb;
					} else {
						x=i+(rand()%3-1);
						y=j+(rand()%3-1);
					};
					if(world_map[x][y]>0 && flora_fauna_map[x][y]==2){
						flora_fauna_map[x][y]=-1;	// eat deer
						ate_prey=true;
					};
					if(ate_prey==true){
						if(rand()%5==0){
							x=i+(rand()%3-1);
							y=j+(rand()%3-1);
							if(world_map[x][y]>0 && world_map[x][y]<4 && flora_fauna_map[x][y]==-1){
								flora_fauna_map[x][y]=4;	// new bear
							};							
						};
					} else {
						if(world_map[x][y]>0 && world_map[x][y]<4 && flora_fauna_map[x][y]==-1){
							flora_fauna_map[i][j]=-1;
							flora_fauna_map[x][y]=4;	// move randomly or towards prey
						};
					};
				
				default:
					break;
			};
		};
	};

};