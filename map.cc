/* map.cc map commands */

int world::count_geography(int x, int y, int range, int type){
	int i,j,count;
	
	count=0;
	for(i=x-range;i<=x+range;i++){
		for(j=y-range;j<=y+range;j++){
			if(i>0 && i< (int) world_map.size() && j>0 && j< (int) world_map[0].size()){
				if(world_map[i][j]==type){
					count++;
				};
			};
		};
	};
	return(count);
};

int world::count_flora_fauna(int x, int y, int range, int type){
	int i,j,count;
	
	count=0;
	for(i=x-range;i<=x+range;i++){
		for(j=y-range;j<=y+range;j++){
			if(i>0 && i< (int) flora_fauna_map.size() && j>0 && j< (int) flora_fauna_map[0].size()){
				if(flora_fauna_map[i][j]==type){
					count++;
				};
			};
		};
	};
	return(count);
};

void world::plant_trees(){
	int i,j,k,ii,jj,kk;
	
	for(i=0;i<1000;i++){
		for(j=0;j<800;j++){
			// trees; must be in grass or scrub, but prefer grass
			if(world_map[i][j]==1 || world_map[i][j]==2){
				k=rand()%20;	
				if(k==0){
					flora_fauna_map[i][j]=0;
				};
			};
			if(world_map[i][j]==3){
				k=rand()%50;
				if(k==0){
					flora_fauna_map[i][j]=0;
				};
			};
			
			// fruit trees; must be in low-lying grass
			if(world_map[i][j]==1){
				k=rand()%35;
				if(k==0){
					flora_fauna_map[i][j]=1;
				};
			};
			
			// deer; must be in high grass or scrub
			if(world_map[i][j]==2 || world_map[i][j]==3){
				k=rand()%100;
				if(k==0){
					flora_fauna_map[i][j]=2;
				};
			};
			
			// fish; must be in water near land
			if(world_map[i][j]==0){
				kk=0;
				for(ii=i-3;ii<=i+3;ii++){
					for(jj=j-3;jj<=j+3;jj++){
						kk=kk+world_map[ii][jj];
					};
				};
				k=rand()%50;
				if(k==0 && kk>0){
					flora_fauna_map[i][j]=3;
				};
			};
			
			// bear; must be in scrub
			if(world_map[i][j]==3){
				k=rand()%250;
				if(k==0){
					flora_fauna_map[i][j]=4;
				};
			};
		};
	};
	
	// location of specific cities (fixed)
	
	flora_fauna_map[289][457]=100;	// Cambridge
	flora_fauna_map[283][440]=101;	// London
	flora_fauna_map[342][441]=102;	// Amsterdam
	flora_fauna_map[296][383]=103;	// Paris
	flora_fauna_map[409][297]=104;	// Venice
	flora_fauna_map[408][231]=105;	// Rome
	flora_fauna_map[372][336]=106;	// Zurich
	flora_fauna_map[568][154]=107;	// Athens
	flora_fauna_map[645][218]=108;	// Constantinople
	flora_fauna_map[175][254]=109;	// Madrid
	flora_fauna_map[121][182]=110;	// Gibraltar

};

void world::adjust_mountain_heights(){
	int i,j,a,b,k;
	bool all_high;
	for(i=10;i<990;i++){
		for(j=10;j<790;j++){
			all_high=true;
			for(a=-1;a<=1;a++){
				for(b=-1;b<=1;b++){
					if(world_map[i+a][j+b]<4){
						all_high=false;
					};
				};
			};
			if(all_high==true){
				k=rand()%11;
				if(k==8 || k==9){
					world_map[i][j]=5;
				};
				if(k==10){
					world_map[i][j]=6;
				};
			};
		};
	};
};

void world::clear_flora_fauna(){
	int i,j;
	for(i=0;i<(int) flora_fauna_map.size();i++){
		for(j=0;j<(int) flora_fauna_map[0].size();j++){
			flora_fauna_map[i][j]=-1;
		};
	};
};

void world::clear_wall(){
	int i,j;
	if(wall_map.size()!=0){	// is there a wall layer?
		for(i=0;i<(int) wall_map.size();i++){
			for(j=0;j<(int) wall_map[0].size();j++){
				flora_fauna_map[i][j]=-1;
			};
		};	
	};
};

void world::add_random_building(int i, int j, int size){
	// try to add building to wall layer centered at i,j with radius roughly size
	bool can_build, found_wall;
	int a,b,c,d;
	int ileft,iright,jleft,jright;
	int occupied_neighbors,occupied_up_down;
	
	can_build=true;
		
	if(size<3){	// too small to bother?
		can_build=false;
	};
	
	if(wall_map.size()==0){	// is there a wall layer?
		can_build=false;
	} else {
		if(i-size<=1 || i+size >= (int) wall_map.size()-1 || j-size<=1 || j+size >= (int) wall_map[0].size()-1){	
			// is wall layer big enough to accomodate building site?
			can_build=false;
		} else {	
			for(a=i-size;a<=i+size;a++){
				for(b=j-size;b<=j+size;b++){
					if(wall_map[a][b]!=-1){		// is building site empty?
						can_build=false;
					};
					if(world_map[a][b]<=0 || world_map[a][b]>=4){	// is there land and no mountain?
						can_build=false;
					};
				};
			};
			if(world_map[i][j]<2){		// center must be elevated
				can_build=false;
			};
		};
	};
	if(can_build==true){
		// build!
		for(c=0;c<2;c++){		// draw footprint
			ileft=1-size+(rand()% (size-2));
			iright=1+(rand()% (size-2));
			jleft=1-size+(rand()% (size-2));
			jright=1+(rand()% (size-2));
		
			for(a=i+ileft;a<=i+iright;a++){
				for(b=j+jleft;b<=j+jright;b++){
					wall_map[a][b]=0;
				};
			};
		};
		
		for(a=i-size;a<=i+size;a++){	// detect boundary squares in footprint
			for(b=j-size;b<=j+size;b++){
				occupied_neighbors=0;
				occupied_up_down=0;
				for(c=a-1;c<=a+1;c++){
					for(d=b-1;d<=b+1;d++){
						if(wall_map[c][d]>-1){
							occupied_neighbors=occupied_neighbors+1;
							if(c==a){
								occupied_up_down=occupied_up_down+1;
							};
						};
						
					};
				};
				if(wall_map[a][b]>-1){
					if(occupied_neighbors==6 || occupied_neighbors==7){
						if(occupied_up_down==3){
							wall_map[a][b]=1;
						} else {
							wall_map[a][b]=0;
						};
					} else if(occupied_neighbors>=4 && occupied_neighbors<=8){
						wall_map[a][b]=2;
					} else {
						wall_map[a][b]=3;
					};
				};
			};
		};
		for(a=i-size;a<=i+size;a++){	// erase interior squares
			for(b=j-size;b<=j+size;b++){
				if(wall_map[a][b]==3){
					wall_map[a][b]=-1;
				};
			};
		};
			// put in doorway
		found_wall=false;
		switch(rand()%4){	
			case 0:
				for(b=j-size;b<=j+size;b++){
					if(found_wall==false && wall_map[i][b]>-1){
						wall_map[i][b]=-1;
						found_wall=true;
					};
				};
				break;
			case 1:
				for(b=j+size;b>=j-size;b--){
					if(found_wall==false && wall_map[i][b]>-1){
						wall_map[i][b]=-1;
						found_wall=true;
					};
				};
				break;	
			case 2:
				for(a=i-size;a<=i+size;a++){
					if(found_wall==false && wall_map[a][j]>-1){
						wall_map[a][j]=-1;
						found_wall=true;
					};
				};
				break;
			case 3:
				for(a=i+size;a>=i-size;a--){
					if(found_wall==false && wall_map[a][j]>-1){
						wall_map[a][j]=-1;
						found_wall=true;
					};
				};
				break;		
		};
	};
};

void world::populate_city_with_random_buildings(){
	int count;
	int i,j,size;
	for(count=0;count<10000;count++){
		if(0< (int) wall_map.size()){
			i=rand()%wall_map.size();
			j=rand()%wall_map[0].size();
			size=3+(rand()%5);
			add_random_building(i,j,size);
		};
	};
};


void world::add_random_flora_fauna_city(int i, int j){
	bool can_add, can_add_person;
	int k;
	int a,b;
	can_add=true;
	can_add_person=true;
	if(i<=0 || i>=(int) flora_fauna_map.size()-1 || j<=0 || j>=(int) flora_fauna_map[0].size()-1){
		can_add=false;
		can_add_person=false;
	};
	if(0< (int) wall_map.size()){	// not too close to building!
		for(a=-2;a<=2;a++){
			for(b=-2;b<=2;b++){
				if(i+a>0 && i+a<(int) flora_fauna_map.size()-1 && j+b>0 && j+b<(int) flora_fauna_map[0].size()-1){
					if(wall_map[i+a][j+b]!=-1){
						can_add=false;
						if(a==0 && b==0){
							can_add_person=false;
						};
					};
				};
			};
		};
	};
	if(flora_fauna_map[i][j]!=-1){	// occupied
		can_add=false;
		can_add_person=false;
	};
	if(can_add==true){
		switch(world_map[i][j]){
			case 0:		// water
				break;
			case 1:
				k=rand()%7;
				if(k<5){
					flora_fauna_map[i][j]=0;	// tree
				} else if(k==5){
					flora_fauna_map[i][j]=1;	// fruit tree
				} else if(k==6){
					flora_fauna_map[i][j]=10;	// cow
				} else if(k==7){
					npcs.push_back(make_new_npc(50,i,j));	// farmer
			//		flora_fauna_map[i][j]=50;	// farmer
				};
				break;
			case 2:
				k=rand()%10;
				if(k<5){
					flora_fauna_map[i][j]=0;	// tree
				} else if (k<7) {
					flora_fauna_map[i][j]=10;	// cow
				} else if (k==7) {
					npcs.push_back(make_new_npc(50,i,j));	// farmer
				};
				break;
			case 3:
				k=rand()%10;
				if(k<3){
					flora_fauna_map[i][j]=12;	// goat
				};
				break;
			case 4:
				break;
		};
	} else if(can_add_person==true){
		k=rand()%10;
		if(k<2){
			npcs.push_back(make_new_npc(50,i,j));	// farmer
		} else if(k<4){
			npcs.push_back(make_new_npc(55,i,j));	// cook
		} else if(k<6){
			npcs.push_back(make_new_npc(56,i,j));	// monk
		} else if(k<8){
			npcs.push_back(make_new_npc(57,i,j));	// musician
		} else if(k<10){
			npcs.push_back(make_new_npc(51,i,j));	// merchant
		};
	};

};

void world::populate_city_with_random_flora_fauna(){
	int count;
	int i,j;
	for(count=0;count<100;count++){
		i=1+(rand()% (flora_fauna_map.size()-2));
		j=1+(rand()% (flora_fauna_map.size()-2));
		add_random_flora_fauna_city(i,j);
	};
};

void world::spawn_random_flora_fauna(){
	int i,j,k;
	i=P.x+(rand()%21)-10;
	j=P.y+(rand()%21)-10;
	cout.flush();
	if(i>0 && i<(int) world_map.size()-1 && j>0 && j<(int) world_map.size() && 
			in_combat==false && in_city==false){	// in range, not in combat or city
		if(flora_fauna_map[i][j]==-1 && occupied_by_special(i,j)==-1 && norm(towards_object(i,j,99))>3){
			switch(world_map[i][j]){
				case 0:
					k=rand()%10;
					if(k<2){
						flora_fauna_map[i][j]=14;	// new fish
					};
					break;
				case 1:
					k=rand()%10;
					if(k<2){
						flora_fauna_map[i][j]=11;	// new boar
					} else if(k<6){
						flora_fauna_map[i][j]=13;	// new deer
					};
					break;
				case 2:
					k=rand()%20;
					if(k<6){
						flora_fauna_map[i][j]=11;	// new boar
					} else if(k<12){
						flora_fauna_map[i][j]=13;	// new deer
					} else if(k<13){
						flora_fauna_map[i][j]=21;	// new wolf
					} else if(k<14){
						flora_fauna_map[i][j]=53;	// new robber
					};
					break;
				case 3:
					k=rand()%20;
					if(k<1){
						flora_fauna_map[i][j]=11;	// new boar
					} else if(k<2){
						flora_fauna_map[i][j]=21;	// new wolf
					} else if(k<3){
						flora_fauna_map[i][j]=20;	// new bear
					} else if(k<4){
						flora_fauna_map[i][j]=53;	// new robber
					} else if(k<5){
						flora_fauna_map[i][j]=12;	// new goat
					};
					break;
				case 4:
					k=rand()%10;
					if(k==0){
						flora_fauna_map[i][j]=12;	// new goat
					};
					break;
			};
		};
	};
};


