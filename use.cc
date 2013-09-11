/* use.cc	use objects in game mode */

void world::use_object(int x, int y){	
	// interact with object in flora/fauna layer in relative location x,y
	int c;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			add_new_message("[u]se nothing");
			break;
		case 0:	// tree
			if(test_of_skill(1)==true){	// have axe and pass woodcutting test
				flora_fauna_map[P.x+x][P.y+y]=-1;	// tree is chopped down
				add_new_message("tree felled");
				P.wood=P.wood+100;
			};
			break;
		case 1: // fruit tree
			add_new_message("fruit picked");
			P.food++;
			break;
		case 2:	// hunt deer
			if(test_of_skill(2)==true){	// have bow and pass hunting test
				flora_fauna_map[P.x+x][P.y+y]=-1;	// deer is killed
				add_new_message("deer killed");
				P.food=P.food+100;
			} else {
			};
			break;
		case 3: // fish
			if(test_of_skill(3)==true){ // have net and pass fishing test
				flora_fauna_map[P.x+x][P.y+y]=-1;	// fish is killed
				add_new_message("fish caught");
				P.food=P.food+100;
			} else {
			};
			break;
		case 7: // cow
			if(test_of_skill(5)==true){	// have bucket
				add_new_message("cow milked");
				P.food=P.food+10;
			} else {
			};
			break;
		default:
			break;
	};
};
