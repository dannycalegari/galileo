/* goal.cc	goal routines for npcs	*/


int world::update_goal(int type, int goal){
	// when npc of type achieves goal, value of new goal
	int new_goal;
	
	switch(type){
		case 50:	// milkmaid
			switch(goal){
				case 51:	// reach merchant
					new_goal=10;	// new goal is cow
					break;
				case 10:	// reach cow
					new_goal=51;	// new goal is merchant
					break;
				default:
					new_goal=-1;
					break;
			};
			break;
		case 51:	// merchant
			switch(goal){
				case 50:	// reach milkmaid
					new_goal=52;	// new goal is woodcutter
					break;
				case 52:	// reach woodcutter
					new_goal=99;	// new goal is avatar
					break;
				case 99:	// reach avatar
					new_goal=50;	// new goal is milkmaid
					break;
				default:
					new_goal=-1;
					break;
			};
			break;
		case 52:	// woodcutter
			switch(goal){
				case 51:	// reach merchant
					new_goal=0;	// new goal is tree
					break;
				case 0:	// reach tree
					new_goal=100;	// new goal is city
					break;
				case 100: // reach city
					new_goal=0;		// new goal is tree
					break;
				default:
					new_goal=-1;
					break;
			};
			break;
		default:
			new_goal=-1;	// no goal
			break;
	};
	return(new_goal);
};

void world::achieve_goal(int l, int goal, point desired_move){
	// npc with given id achieves goal in relative location desired_move
	switch(goal){
		case 99:	// reach avatar
			conversation_with_npc(l);
			break;
		case 0:		// reach tree
			if(npcs[l].id==52){	// if woodcutter
				flora_fauna_map[npcs[l].x+desired_move.x][npcs[l].y+desired_move.y]=-1;	// chop down tree and replace with sapling
				if(norm(new_point(npcs[l].x,npcs[l].y)-new_point(npcs[l].cx,npcs[l].cy))!=npcs[l].d-1){	// special case to fix bizarre sapling trap
					flora_fauna_map[npcs[l].x][npcs[l].y]=4;	// plant sapling
				} else {
					flora_fauna_map[npcs[l].x+desired_move.x][npcs[l].y+desired_move.y]=4;
				};
			};
			break;
		/*
		case 100:	// reach generic city
			if(npcs[l].id==52){	// if woodcutter
				// remove npc if tree density is too low
				cout << "reached city \n";
				cout << count_flora_fauna(npcs[l].x,npcs[l].y,20,0) << " nearby trees \n";
				cout << count_geography(npcs[l].x,npcs[l].y,20,1) << " nearby low grass \n";
				cout << count_geography(npcs[l].x,npcs[l].y,20,2) << "nearby grass \n";
				if(count_flora_fauna(npcs[l].x,npcs[l].y,20,0)*30 < count_geography(npcs[l].x,npcs[l].y,20,1)+count_geography(npcs[l].x,npcs[l].y,20,2)){
					npcs[l].x=0;
					npcs[l].y=0;	//	cheap way to "remove" npc
				};
			};
			break;
			*/
		default:
			break;
	};
};
