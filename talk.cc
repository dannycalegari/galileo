/* talk.cc	conversation functions */

void world::talk(int x, int y){
	// talk to npc in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			add_new_message("[t]alk to nothing");
			break;
		case 8:
			add_new_message("talk to farmer");
			break;
		case 9:
			add_new_message("talk to robber");
			break;
		case 10:
			add_new_message("talk to merchant");
			break;
		case 11:
			add_new_message("talk to lumberjack");
			break;
		case 12:
			add_new_message("talk to npc");
			break;
		default:
			break;
	};
};