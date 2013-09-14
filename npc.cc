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
		default:
			break;
	};
	
	return(N);
};
