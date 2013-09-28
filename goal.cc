/* goal.cc	goal routines for npcs	*/

void world::update_goal(int l){
	// when npc number l achieves goal, value of new goal
	int i;
	int number_of_goals;
	int next_goal;
	
	number_of_goals=(int) npcs[l].goal_list.size();
	if(number_of_goals<1){
		npcs[l].goal=-1;	// no goal
	} else {
		next_goal=-1;
		for(i=0;i<number_of_goals;i++){
			if(npcs[l].goal_list[i]==npcs[l].goal){
				next_goal=i+1;
			};
		};
	};
	if(next_goal==-1){
		npcs[l].goal=-1;
	} else {
		npcs[l].goal=npcs[l].goal_list[next_goal % number_of_goals];
	};
};
	

void world::achieve_goal(int l, point desired_move){
	// npc number l achieves goal in relative location desired_move
	int goal;
	goal=npcs[l].goal;
	switch(goal){
		case 99:	// reach avatar
			conversation_with_npc(l);
			break;
		case 0:		// reach tree
			flora_fauna_map[npcs[l].x+desired_move.x][npcs[l].y+desired_move.y]=-1;	// chop down tree 
			flora_fauna_map[npcs[l].x+desired_move.x][npcs[l].y+desired_move.y]=4;	// and replace with sapling
			break;
		case 20:	// bear or wolf
		case 21:
			flora_fauna_map[npcs[l].x+desired_move.x][npcs[l].y+desired_move.y]=-1;	// kill bear/wolf
			break;
		default:
			break;
	};
};

void world::pursue_special_goal(int type, int goal){	// special goals; usually plot related

};

