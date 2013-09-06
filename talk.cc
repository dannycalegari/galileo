/* talk.cc	conversation functions */

void world::talk(int x, int y){
	// talk to npc in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			last_command="[t]alk to nothing";
			break;
		case 8:
			last_command="talk to farmer";
			break;
		case 9:
			last_command="talk to robber";
			break;
		case 10:
			last_command="talk to merchant";
			break;
		case 11:
			last_command="talk to lumberjack";
			break;
		case 12:
			last_command="talk to npc";
			break;
		default:
			break;
	};
};