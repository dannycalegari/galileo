/* combat.cc	routines for combat */

/*
struct monster{
	int x,y;	// coordinates
	int id;
	int health;
	bool ranged_attack;
	int strength;
	int dexterity;
};
*/

monster make_new_monster(int type){
	monster M;
	switch(type){
		case 4:
			M.x = 6;
			M.y = 9;
			M.id = 4;
			M.health = 100;
			M.ranged_attack = false;
			M.strength = 40;
			M.dexterity = 20;
			break;
		case 9:
			M.x = 6;
			M.y = 9;
			M.id = 9;
			M.health = 100;
			M.ranged_attack = true;
			M.strength = 20;
			M.dexterity = 30;
			break;
		default:
			break;
	};
	return(M);
};

int world::occupied_by_enemy(int x, int y){	// is there a monster in location x,y, and if so, which monster?
	int i;
	int occupied;
	occupied=-1;
	for(i=0;i<(int) monsters.size();i++){
		if(monsters[i].x==x && monsters[i].y==y){
			occupied=i;
		};	
	};
	return(occupied);
};

int world::enemy_in_direction(int x, int y){ // is there a monster in direction x,y, and if so, which kind?
	int i,j;
	int occupied;
	occupied=-1;
	i=P.x;
	j=P.y;
	bool in_range;
	in_range=true;
	while(in_range){
		i=i+x;
		j=j+y;
		occupied=occupied_by_enemy(i,j);
		if(occupied>-1){
			in_range=false;
		};
		if(i<1 || i>(int) world_map.size() || j<1 || j>(int) world_map[0].size()){
			in_range=false;
		};
	};
	return(occupied);
};

void world::attack(int x, int y){
	// attack monster/person in flora/fauna layer in relative location x,y

	if(in_combat==true){
		if(occupied_by_enemy(P.x+x,P.y+y)>-1){	// is enemy there?
			// attack!
			add_new_message("swipe!");
		} else if(P.skill_item[2]==true && enemy_in_direction(x,y)>-1){	// bow attack?
			// bow attack!
			add_new_message("arrow fired!");
		} else {
			add_new_message("missed!");
		};
	} else {
		switch(flora_fauna_map[P.x+x][P.y+y]){
			case -1:
				add_new_message("[a]ttack nothing");
				break;
			case 4:
				add_new_message("attack bear");
				enter_combat(4);
				break;
			case 8:
				last_command="attack farmer";
				break;
			case 9:
				last_command="attack robber";
				enter_combat(9);
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
};

void world::enter_combat(int type){	// type is code of opponent
	ifstream input_file;
	monster M;
	
	saved_coordinates.x=P.x;	// save external coordinates
	saved_coordinates.y=P.y;
	map_name="combat";
	
	input_file.open("map_files/combat_geo.map");	//  could generate these maps randomly?
	world_map.clear();	// initialize
	read_map(input_file, 0);
	input_file.close();
	input_file.open("map_files/combat_ff.map");
	flora_fauna_map.clear();	// initialize
	read_map(input_file, 1);
	input_file.close();
	input_file.open("map_files/combat_wall.map");
	wall_map.clear();	// initialize
	read_map(input_file, 2);
	input_file.close();	
	P.x=6;	// should be specific to combat map
	P.y=3;
	in_combat=true;
	add_new_message("combat with "+type);	// should add name of opponent here
	M=make_new_monster(type);
	monsters.push_back(M);
	draw_info();
};

void world::exit_combat(){
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
	count_flora_fauna();
	wall_map.clear();	// initialize	
	
	map_name="europe";
	in_combat=false;
	monsters.clear();	// any unkilled monsters disappear
};