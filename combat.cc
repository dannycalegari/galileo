/* combat.cc	routines for combat */

void world::attack(int x, int y){
	// attack monster/person in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			add_new_message("[a]ttack nothing");
			break;
		case 4:
			add_new_message("attack bear");
			break;
		case 8:
			last_command="attack farmer";
			break;
		case 9:
			last_command="attack robber";
			break;
		case 10:
			last_command="attack merchant";
			break;
		case 11:
			last_command="attack lumberjack";
			break;
		case 12:
			last_command="attack npc";
			break;
		default:
			break;
	};
};

void world::exit_combat(){
	ifstream input_file;

	P.x=saved_coordinates.x;
	P.y=saved_coordinates.y;
	
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
	
	map_name="europe";
	in_combat=false;
};