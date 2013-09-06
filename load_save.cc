/* load_save.cc commands to load and save state of world	*/


void world::initialize(){
	ifstream input_file;
	
	input_file.open("europe.map");
	read_map(input_file, 0);
	input_file.close();
	input_file.open("flora_fauna.map");
	read_map(input_file, 1);
	input_file.close();
	count_flora_fauna();

	input_file.open("party.txt");
	read_party(input_file);
	input_file.close();
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

void world::read_party(ifstream &input_file){
	input_file >> last_command;
	cout << last_command << "\n";
	input_file >> gender;
	cout << gender << "\n";
	input_file >> moves;
	cout << moves << "\n";
	input_file >> edit_mode;
	cout << edit_mode << "\n";
	input_file >> view_mode;
	cout << view_mode << "\n";
	input_file >> P.x;
	cout << P.x << "\n";
	input_file >> P.y;
	cout << P.y << "\n";
	input_file >> P.embarked;
	input_file >> P.has_axe;
	input_file >> P.has_bow;
	input_file >> P.has_net;
	input_file >> P.has_sword;
	input_file >> P.has_grapple;
	input_file >> P.has_bucket;
	input_file >> P.food;
	input_file >> P.health;
};
