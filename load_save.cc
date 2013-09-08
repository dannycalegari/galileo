/* load_save.cc commands to load and save state of world	*/


void world::initialize(){
	ifstream input_file;
	
	input_file.open("europe_geo.map");
	world_map.clear();	// initialize
	read_map(input_file, 0);
	input_file.close();
	input_file.open("europe_ff.map");
	flora_fauna_map.clear();	// initialize
	read_map(input_file, 1);
	input_file.close();
	count_flora_fauna();
	wall_map.clear();	// initialize

	input_file.open("party.txt");
	read_party(input_file);
	input_file.close();
};

void world::test_map(){
	ifstream input_file;
	
	input_file.open("test_geo.map");
	world_map.clear();	// initialize
	read_map(input_file, 0);
	input_file.close();
	input_file.open("test_ff.map");
	flora_fauna_map.clear();	// initialize
	read_map(input_file, 1);
	input_file.close();
	count_flora_fauna();
	input_file.open("test_wall.map");
	wall_map.clear();	// initialize
	read_map(input_file, 2);
	input_file.close();
	P.x=6;
	P.y=6;
};

void world::save_state(){
	ofstream output_file;
	
	output_file.open("europe.map");
	write_map(output_file, 0);
	output_file.close();
	
	output_file.open("flora_fauna.map");
	write_map(output_file, 1);
	output_file.close();
};

void world::read_map(ifstream &input_file, int type){
	int i,j,k;
	int rows,cols;
	vector<int> map_row;
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

void world::read_party(ifstream &input_file){
	input_file >> last_command;
	input_file >> gender;
	input_file >> moves;
	input_file >> edit_mode;
	input_file >> view_mode;
	input_file >> P.x;
	input_file >> P.y;
	input_file >> P.food;
	input_file >> P.health;
	input_file >> P.wood;
	input_file >> P.gold;
	
	int i;
	for(i=0;i<MAX_SKILL_NUM;i++){
		input_file >> P.skill_item[i];
		input_file >> P.skill[i];
		if(input_file.eof()) break;
	};
};

void world::write_party(ofstream &output_file){
	output_file << last_command << "\n";
	output_file << gender << "\n";
	output_file << moves << "\n";
	output_file << edit_mode << "\n";
	output_file << view_mode << "\n";
	output_file << P.x << "\n";
	output_file << P.y << "\n";
	output_file << P.food << "\n";
	output_file << P.health << "\n";
	output_file << P.wood << "\n";
	output_file << P.gold << "\n";
	
	int i;
	for(i=0;i<MAX_SKILL_NUM;i++){
		output_file << P.skill_item[i] << " " << P.skill[i] << "\n";
	};
};