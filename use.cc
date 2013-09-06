/* use.cc	use objects in game mode */

void world::use_object(int x, int y){	
	// interact with object in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			last_command="[u]se nothing";
			break;
		case 0:	// tree
			last_command="chop down tree";
			break;
		case 1: // fruit tree
			last_command="pick fruit";
			break;
		case 2:	// hunt deer
			last_command="hunt deer";
			break;
		case 3: // fish
			last_command="fish";
			break;
		case 7: // cow
			last_command="milk cow";
			break;
		default:
			break;
	};
};
