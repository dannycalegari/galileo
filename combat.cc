/* combat.cc	routines for combat */

void world::attack(int x, int y){
	// attack monster/person in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			last_command="[a]ttack nothing";
			break;
		case 4:
			last_command="attack bear";
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