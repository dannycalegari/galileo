/* npc.cc	code to generate random npc or read npc from file	*/

/*
struct conversation_item{
	string prompt;
	string reply;
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

	N.x=x;
	N.y=y;
	N.cx=x;
	N.cy=y;
	N.talk_list.clear();	// initialize
			
	switch(type){
		case 8:	// generic farmer npc
			N.id=8;
			C.prompt="initial";
			C.reply="hello!";
			N.talk_list.push_back(C);
			C.prompt="weather";
			C.reply="looks like rain";
			N.talk_list.push_back(C);
			C.prompt="rain";
			C.reply="good for the farmers";
			N.talk_list.push_back(C);
			C.prompt="farmer";
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
			C.reply="bye!";
			N.talk_list.push_back(C);			
			break;
		case 10: // generic merchant npc
			N.id=10;
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
			C.reply="there's lots of commerce in Venice";
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
