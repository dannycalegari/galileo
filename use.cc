/* use.cc	use objects in game mode */

bool world::have_object(string S){	// test if object is in inventory
	int i;
	bool have_object;
	have_object=false;
	for(i=0;i<(int) P.item_list.size();i++){
		if(P.item_list[i]==S){	// need exact match
			have_object=true;
		};
	};
	return(have_object);
};

bool world::directional_use(string S){	// test if object is used in a direction
	bool directional;
	directional=false;
	if(S=="bow" || S=="axe" || S=="bucket" || S=="telescope" || S=="net" || S=="saddle" || S=="flower"){
		directional=true;
	};
	return(directional);
};

void world::use_dialog(){	// interface to use object
	int x,y;
	string S,R;
	char c;
	KeySym k;
	
	add_new_message("use what?");
	draw_info();
	k=select_direction_interface(x,y);
	if(norm(new_point(x,y))==0){
		R="";
		if(k >= 0x041 && k <= 0x05a){	// upper case
			k=k+0x020;
		} else if(k >=  0x061 && k <= 0x07a){
			c=char('a'+(int) (k-97));
			R=R+c;
		};
		S=get_line_of_text(R);	// need to strip off initial "> "
		S=S.substr(2,S.length()-2);
		if(have_object(S)){
			if(directional_use(S)){
				add_new_message("use in what direction?");
				draw_info();
				select_direction_interface(x,y);
				// use object S in direction x,y
				use_object(S,x,y);
			} else {
				// use object S
				use_object(S);
			};
		} else {
			add_new_message("you don't have "+S);
		};
	} else {
		// use object in direction x,y
		use_object(x,y);
	};
};

void world::use_object(string S){
	if(S=="seed"){	// plant sapling
		if(flora_fauna_map[P.x][P.y]==-1 && (world_map[P.x][P.y]==1 || world_map[P.x][P.y]==2)){
			flora_fauna_map[P.x][P.y]=4;	// sapling planted
			add_new_message("seed planted");
		};
	};
};

void world::use_object(string S, int x, int y){
	if(S=="bow"){	// use bow to hunt
		if(flora_fauna_map[P.x+x][P.y+y]>=11 && flora_fauna_map[P.x+x][P.y+y]<=13){
			// boar, goat, deer
			if(rand()%100<P.dexterity){
				add_new_message("animal killed");
				flora_fauna_map[P.x+x][P.y+y]=-1;
				P.food=P.food+100;
			} else {
				add_new_message("missed");
			};
		} else {
			add_new_message("not here");
		};
	} else if(S=="axe"){	// use axe for firewood
		if(flora_fauna_map[P.x+x][P.y+y]==0){	// tree
			P.wood=P.wood+10;
			add_new_message("collected firewood");
			if(rand()%100<10){
				flora_fauna_map[P.x+x][P.y+y]=-1;	// tree felled
				add_new_message("tree felled");
			};	
		} else {
			add_new_message("not here");
		};
	} else if(S=="bucket"){		// use bucket to milk cow
		if(flora_fauna_map[P.x+x][P.y+y]==10){	// cow
			if(rand()%50<P.dexterity){
				add_new_message("cow milked");
				P.food=P.food+10;
			};
		};
	} else if(S=="net"){	// use net to catch fish
		if(flora_fauna_map[P.x+x][P.y+y]==14){	// fish
			if(rand()%75<P.dexterity){
				add_new_message("fish caught");
				flora_fauna_map[P.x+x][P.y+y]=-1;
				P.food=P.food+40;
			};
		};
	} else if(S=="telescope"){
	
	} else if(S=="saddle"){		// use saddle on horse
		if(flora_fauna_map[P.x+x][P.y+y]==16){	// horse
			if(rand()%100<P.dexterity){
				add_new_message("horse saddled");
				flora_fauna_map[P.x+x][P.y+y]=-1;
				riding=true;
				P.x=P.x+x;
				P.y=P.y+y;
			};
		};
	} else if(S=="flower"){		// use flower on cow
		if(flora_fauna_map[P.x+x][P.y+y]==10){	// cow
			add_new_message("cow eats flower");
			// do something
		};
	};
};

void world::use_object(int x, int y){	
	// interact with object in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			add_new_message("[u]se nothing");
			break;
		case 1:
			add_new_message("fruit picked");
			P.food++;
			break;
		case 2:
			add_new_message("coconut picked");
			P.food=P.food+3;
			break;
		case 5:
			add_new_message("flower picked");
			// add flower as item
			break;
		default:
			break;
	};
};

void world::add_new_object(string S){
	int i;
	bool already_have;
	already_have=false;
	for(i=0;i<(int) P.item_list.size();i++){	// do I already have the object?
		if(S==P.item_list[i]){
			already_have=true;
		};
	};
	if(already_have==false){	// if not, then add it to item list
		P.item_list.push_back(S);
	};
};

void world::remove_object(string S){
	int i,j;
	bool have_object;
	have_object=false;
	for(i=0;i<(int) P.item_list.size();i++){
		if(S==P.item_list[i]){
			have_object=true;
			j=i;
		};
	};
	if(have_object){
		P.item_list.erase(P.item_list.begin()+j);
	};
};