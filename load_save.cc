/* load_save.cc commands to load and save state of world	*/


void world::initialize(){
	P.x=300;
	P.y=400;	// initial party location
	
	ifstream input_file;
	input_file.open("europe.map");
	read_map(input_file, 0);
	input_file.close();
	input_file.open("flora_fauna.map");
	read_map(input_file, 1);
	input_file.close();
	count_flora_fauna();
	//	adjust_mountain_heights();	// random adjustment
	
	last_command="";	// initialize memory of last command
	gender="male";	// default gender; should make this loadable
	P.embarked=false;	// default embarked state
		
	moves=0;
	edit_mode=false;
	view_mode=false;
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