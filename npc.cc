/* npc.cc	code to generate random npc or read npc from file	*/

/*
struct conversation_item{
	string prompt;
	string reply;	
	// a reply has two components: a verbal part, and a nonverbal part; they are
	// separated by a semicolon. So for example, a response could be: hello!;smile
};

struct npc{
	int x,y;	// coordinates 
	int id;		//
	int cx,cy;	// center of gravity
		// skills; out of 100
	int sword;
	int bow;
	int net;
	int farming;
	int axe;
		// attributes; out of 100
	int health;
	int food;
	int wood;
	
	vector<conversation_item > talk_list;	// canned responses for conversation
};
*/

npc world::make_new_npc(int type, int x, int y){
	npc N;
	conversation_item C;

	N.x=x;	// initial location
	N.y=y;
	N.cx=x;	// center of gravity
	N.cy=y;
	N.hx=1;	// initial heading
	N.hy=0;
	N.d=20;	// maximum distance from center of gravity
	N.talk_list.clear();	// initialize
			
	switch(type){
		case 50:	// generic farmer npc
			N.id=50;
			N.goal=51;	// goal is merchant
			C.prompt="initial";
			C.reply="hello!";
			N.talk_list.push_back(C);
			C.prompt="weather";
			C.reply="looks like rain";
			N.talk_list.push_back(C);
			C.prompt="rain";
			C.reply="good for the farmers";
			N.talk_list.push_back(C);
			C.prompt="farm";
			C.reply="I sow and harvest wheat";
			N.talk_list.push_back(C);
			C.prompt="wheat";
			C.reply="used to make bread";
			N.talk_list.push_back(C);
			C.prompt="bread";
			C.reply="they love it in Paris";
			N.talk_list.push_back(C);
			C.prompt="name";
			C.reply="Jane";
			N.talk_list.push_back(C);
			C.prompt="job";
			C.reply="I'm a farmer";
			N.talk_list.push_back(C);
			C.prompt="bye";
			C.reply="bye!;exit";
			N.talk_list.push_back(C);			
			break;
		case 51: // generic merchant npc
			N.id=51;
			N.goal=-1;	//	goal is avatar
			C.prompt="initial";
			C.reply="hello!";
			N.talk_list.push_back(C);
			C.prompt="name";
			C.reply="Antonio";
			N.talk_list.push_back(C);
			C.prompt="job";
			C.reply="merchant";
			N.talk_list.push_back(C);
			C.prompt="merchant";
			C.reply="there's lots of commerce in   Venice";
			N.talk_list.push_back(C);
			C.prompt="money";
			C.reply="it makes the world go round";
			N.talk_list.push_back(C);
			C.prompt="buy";
			C.reply="I don't have my wares with me";
			N.talk_list.push_back(C);
			C.prompt="bye";
			C.reply="bye!";
			N.talk_list.push_back(C);		
			break;
		case 52: // generic woodcutter npc
			N.id=52;
			N.goal=0;	//	goal is tree
			C.prompt="initial";
			C.reply="hello!";
			N.talk_list.push_back(C);
			C.prompt="name";
			C.reply="Lisa";
			N.talk_list.push_back(C);
			C.prompt="job";
			C.reply="woodcutter";
			N.talk_list.push_back(C);
			C.prompt="wood";
			C.reply="grows on trees";
			N.talk_list.push_back(C);
			C.prompt="bye";
			C.reply="bye!";
			N.talk_list.push_back(C);		
			break;
		default:
			break;
	};
	
	return(N);
};

int world::update_goal(int type, int goal){
	// when npc of type achieves goal, value of new goal
	int new_goal;
	
	switch(type){
		case 50:	// farmer
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
				case 50:	// reach farmer
					new_goal=52;	// new goal is woodcutter
					break;
				case 52:	// reach woodcutter
					new_goal=99;	// new goal is avatar
					break;
				case 99:	// reach avatar
					new_goal=50;	// new goal is farmer
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
					new_goal=51;	// new goal is merchant
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
				flora_fauna_map[npcs[l].x][npcs[l].y]=4;	// plant sapling
			};
			break;
		default:
			break;
	};
};
