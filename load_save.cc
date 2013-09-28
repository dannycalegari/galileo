/* load_save.cc commands to load and save state of world	*/


void world::initialize(){
	ifstream input_file;
	
	input_file.open("map_files/europe_geo.map");
	world_map.clear();	// initialize
	read_map(input_file, 0);
	input_file.close();
	input_file.open("map_files/europe_ff.map");
	flora_fauna_map.clear();	// initialize
	read_map(input_file, 1);
	input_file.close();
//	count_flora_fauna();
	wall_map.clear();	// initialize
	
	input_file.open("npc_files/europe.npc");
	read_npc_file(input_file);
	input_file.close();

	input_file.open("party.txt");
	read_party(input_file);
	input_file.close();
	
	map_name="europe";
	in_city=false;
	in_combat=false;
	embarked=false;
	riding=false;
	monsters.clear();	// no monsters
//	npcs.clear();	// no npcs
	message.clear();
	popup_message.clear();
	theta=0.3;
};

void world::enter_city(string S){	// S is name of city
	ifstream input_file;
	string T;
	map_name=S;
	
	T="map_files/"+S+"_geo.map";
	input_file.open(T.c_str());
	world_map.clear();	// initialize
	read_map(input_file, 0);
	input_file.close();
	T="map_files/"+S+"_ff.map";
	input_file.open(T.c_str());
	flora_fauna_map.clear();	// initialize
	read_map(input_file, 1);
	input_file.close();
//	count_flora_fauna();
	T="map_files/"+S+"_wall.map";
	input_file.open(T.c_str());
	wall_map.clear();	// initialize
	read_map(input_file, 2);
	input_file.close();
	saved_coordinates.x=P.x;
	saved_coordinates.y=P.y;
	P.x=6;	// should be city-specific
	P.y=6;
	in_city=true;
	add_new_message("entering "+S);
	
	T="npc_files/"+S+".npc";
	input_file.open(T.c_str());
	read_npc_file(input_file);
	input_file.close();
};

void world::exit_city(){
	ifstream input_file;

	P.x=saved_coordinates.x;
	P.y=saved_coordinates.y;
	
	input_file.open("map_files/europe_geo.map");
	world_map.clear();	// initialize
	read_map(input_file, 0);
	input_file.close();
	input_file.open("map_files/europe_ff.map");
	flora_fauna_map.clear();	// initialize
	read_map(input_file, 1);
	input_file.close();
	wall_map.clear();	// initialize	
	add_new_message("leaving "+map_name);
	map_name="europe";
	in_city=false;

	input_file.open("npc_files/europe.npc");
	read_npc_file(input_file);
	input_file.close();
};

void world::save_current_map(){
	ofstream output_file;
	string T;
	
	T="map_files/"+map_name+"_geo.map";
	output_file.open(T.c_str());
	write_map(output_file, 0);
	output_file.close();
	
	T="map_files/"+map_name+"_ff.map";
	output_file.open(T.c_str());
	write_map(output_file, 1);
	output_file.close();
	
	T="map_files/"+map_name+"_wall.map";
	output_file.open(T.c_str());
	write_map(output_file, 2);
	output_file.close();
	
	T="npc_files/"+map_name+".npc";
	output_file.open(T.c_str());
	write_npc_file(output_file);
	output_file.close();
};

void world::write_npc_file(ofstream &output_file){
	int number_of_npcs,i,number_of_conversation_items,number_of_goal_items,j;
	number_of_npcs=(int) npcs.size();
	output_file << number_of_npcs << "\n";	// number of npcs
	for(i=0;i<number_of_npcs;i++){	// for each npc
		output_file << npcs[i].id << "\n";
		output_file << npcs[i].sprite << "\n";
		output_file <<  npcs[i].x << " " << npcs[i].y << "\n";
		output_file << npcs[i].goal << "\n";
		output_file << npcs[i].grumble << "\n";
		number_of_conversation_items=(int) npcs[i].talk_list.size();
		output_file << number_of_conversation_items << "\n";
		for(j=0;j<number_of_conversation_items;j++){
			output_file << npcs[i].talk_list[j].prompt << "\n";
			output_file << npcs[i].talk_list[j].reply << "\n";
		};
		number_of_goal_items=(int) npcs[i].goal_list.size();
		output_file << number_of_goal_items << "\n";
		for(j=0;j<number_of_goal_items;j++){
			output_file << npcs[i].goal_list[j] << "\n";
		};
	};
};

void world::read_npc_file(ifstream &input_file){
	int number_of_npcs,i,number_of_conversation_items,number_of_goal_items,j,k;
	npc N;
	conversation_item C;
	string S,R;
	npcs.clear();	// initialize npc roster
	
	input_file >> number_of_npcs;	// number of npcs
	for(i=0;i<number_of_npcs;i++){	// for each npc
		input_file >> N.id;
		input_file >> N.sprite;
		input_file >> N.x >> N.y;
		input_file >> N.goal;
		getline(input_file,S);	// clear buffer
		getline(input_file,S);	// 
		N.grumble=S;
		input_file >> number_of_conversation_items;
		getline(input_file,S);	// clear buffer
		N.talk_list.clear();
		for(j=0;j<number_of_conversation_items;j++){
			getline(input_file,S);
			getline(input_file,R);
			C=new_conversation_item(S,R);
			N.talk_list.push_back(C);
		};
		input_file >> number_of_goal_items;
		N.goal_list.clear();
		for(j=0;j<number_of_goal_items;j++){
			input_file >> k;
			N.goal_list.push_back(k);
		};
		npcs.push_back(N);
	};
};


void world::read_map(ifstream &input_file, int type){
	int i,j,k;
	int r,g,b;
	int rows,cols;
	vector<int> map_row;
	if(type==2){
		input_file >> r >> g >> b;
		wall_color = (long) (256*256*r)+(256*g)+b;
		input_file >> r >> g >> b;
		trim_color = (long) (256*256*r)+(256*g)+b;
		input_file >> battlement_style;
	};
	input_file >> cols;
	input_file >> rows;
	for(i=0;i<cols;i++){
		for(j=0;j<rows;j++){
			input_file >> k;
			map_row.push_back(k);
		};
		switch(type){
			case 0:
				world_map.push_back(map_row);
				break;
			case 1:
				flora_fauna_map.push_back(map_row);
				break;
			case 2:
				wall_map.push_back(map_row);
				break;
			default:
				break;
		};
		map_row.clear();
	};
};

void world::write_map(ofstream &output_file, int type){
	int i,j,k;
	int rows, cols;
	switch(type){
		case 0:	
			rows=world_map[0].size();
			cols=world_map.size();
			break;
		case 1:
			rows=flora_fauna_map[0].size();
			cols=flora_fauna_map.size();
			break;
		case 2:
			cols=wall_map.size();
			if(cols==0){	// special case: might have no wall map (eg europe)
				rows=0;
			} else {
				rows=wall_map[0].size();
			};
			output_file << wall_color/(256*256) << " " << (wall_color/256)%256 << " " << wall_color % 256 << "\n";
			output_file << trim_color/(256*256) << " " << (trim_color/256)%256 << " " << trim_color % 256 << "\n";
			output_file << battlement_style << "\n";
			break;			
		};
	output_file << cols << " " << rows << "\n";
	for(i=0;i<cols;i++){
		for(j=0;j<rows;j++){
			switch(type){
				case 0:
					k=world_map[i][j];
					break;
				case 1:
					k=flora_fauna_map[i][j];
					break;
				case 2:
					k=wall_map[i][j];
					break;
				default:
					break;
			};
			if(k<0){
				output_file << k << "  ";
			} else if(k<10){
				output_file << k << "   ";
			} else if(k<100){
				output_file << k << "  ";
			} else {
				output_file << k << " ";
			};
		};
		output_file << "\n";
	};
};

void world::read_party(ifstream &input_file){
	int number_of_items,i;
	string S;
	
	input_file >> P.gender;
	input_file >> moves;
	input_file >> edit_mode;
	input_file >> view_mode;
	input_file >> P.x;
	input_file >> P.y;
	input_file >> P.food;
	input_file >> P.health;
	input_file >> P.wood;
	input_file >> P.gold;
	input_file >> P.dexterity;
	input_file >> P.strength;
	input_file >> P.intelligence;
	
	P.item_list.clear();	// initialize
	input_file >> number_of_items;
	
	for(i=0;i<number_of_items;i++){
		input_file >> S;
		P.item_list.push_back(S);
	};
};

void world::write_party(ofstream &output_file){
	int number_of_items,i;

	output_file << P.gender << "\n";
	output_file << moves << "\n";
	output_file << edit_mode << "\n";
	output_file << view_mode << "\n";
	output_file << P.x << "\n";
	output_file << P.y << "\n";
	output_file << P.food << "\n";
	output_file << P.health << "\n";
	output_file << P.wood << "\n";
	output_file << P.gold << "\n";
	output_file << P.dexterity << "\n";
	output_file << P.strength << "\n";
	output_file << P.intelligence << "\n";
	
	number_of_items=P.item_list.size();
	output_file << number_of_items << "\n";
	for(i=0;i<number_of_items;i++){
		output_file << P.item_list[i] << "\n";
	};
};