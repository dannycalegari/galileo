/* map.cc map commands */

void world::read_map(ifstream &input_file, int type){
	int i,j;
	for(i=0;i<1000;i++){
		for(j=0;j<800;j++){
			if(type==0){
				input_file >> world_map[i][j];
			} else {
				input_file >> flora_fauna_map[i][j];
			};
		};
	};
};

void world::write_map(ofstream &output_file, int type){
	int i,j;
	for(i=0;i<1000;i++){
		for(j=0;j<800;j++){
			if(type==0){
				output_file << world_map[i][j] << "\n";
			} else {
				output_file << flora_fauna_map[i][j] << "\n";
			};
		};
	};
};

void world::clear_flora_fauna(){
	int i,j;
	for(i=0;i<1000;i++){
		for(j=0;j<800;j++){
			flora_fauna_map[i][j]=-1;
		};
	};
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

