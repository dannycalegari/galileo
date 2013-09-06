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
			if(P.has_axe){
				last_command="chop down tree";
			} else {
				last_command="how?";
			};
			break;
		case 1: // fruit tree
			last_command="pick fruit";
			break;
		case 2:	// hunt deer
			if(P.has_bow){
				last_command="hunt deer";
			} else {
				last_command="how?";
			};
			break;
		case 3: // fish
			if(P.has_net){
				last_command="fish";
			} else {
				last_command="how?";
			};
			break;
		case 7: // cow
			if(P.has_bucket){
				last_command="milk cow";
			} else {
				last_command="how?";
			};
			break;
		default:
			break;
	};
};
