/* npc.cc	code to generate random npc or read npc from file	*/

/*
struct conversation_item{
	string prompt;
	string reply;	
	// a reply has two components: a verbal part, and a nonverbal part; they are
	// separated by a semicolon. So for example, a response could be: hello!;smile
};

struct npc{
	int x,y;		// coordinates 
	int id;			// global id
	int sprite;		// sprite number of npc appearance
	int goal;		// flora/fauna number of object npc wants to move towards; -1=none
	string grumble;		// speech balloon said randomly
	vector<conversation_item > talk_list;	// canned responses for conversation
};
*/

npc world::make_new_npc(int type, int x, int y){
	npc N;
	conversation_item C;

	N.x=x;	// initial location
	N.y=y;
	N.talk_list.clear();	// initialize
	N.goal_list.clear();
			
	switch(type){
		case 49:	// generic scholar npc
			N.id=49;
			N.sprite=49;
			N.grumble="hmm";
			N.goal=51;	// initial goal is merchant
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Martin"));
			N.talk_list.push_back(new_conversation_item("job", "scholar"));
			N.talk_list.push_back(new_conversation_item("read", "I read many books"));
			N.talk_list.push_back(new_conversation_item("book", "I hear there is a new book by an Italian called Galileo"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));	
			N.goal_list.push_back(51);
			N.goal_list.push_back(55);
			break;			
		case 50:	// generic milkmaid npc
			N.id=50;
			N.sprite=50;
			N.grumble="la la";
			N.goal=10;	// initial goal is cow
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Jane"));
			N.talk_list.push_back(new_conversation_item("job", "milkmaid"));
			N.talk_list.push_back(new_conversation_item("rain", "good for the farmers"));
			N.talk_list.push_back(new_conversation_item("cow", "they like flowers"));
			N.talk_list.push_back(new_conversation_item("farm", "I milk cows"));
			N.talk_list.push_back(new_conversation_item("milk", "used for cheese"));
			N.talk_list.push_back(new_conversation_item("cheese", "try a Stilton in London"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));		
			N.goal_list.push_back(10);
			N.goal_list.push_back(51);
			break;
		case 51: // generic merchant npc
			N.id=51;
			N.sprite=51;
			N.grumble="";
			N.goal=-2;	// initial goal is none
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Antonio"));
			N.talk_list.push_back(new_conversation_item("job", "merchant"));
			N.talk_list.push_back(new_conversation_item("merchant", "I trained in Venice"));
			N.talk_list.push_back(new_conversation_item("money", "it makes the world go round"));
			N.talk_list.push_back(new_conversation_item("buy", "what would you like;ba"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));	
			break;
		case 52: // generic woodcutter npc
			N.id=52;
			N.sprite=52;
			N.grumble="timber!";
			N.goal=0;	// initial goal is tree
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Lisa"));
			N.talk_list.push_back(new_conversation_item("job", "woodcutter"));
			N.talk_list.push_back(new_conversation_item("wood", "for building"));
			N.talk_list.push_back(new_conversation_item("build", "the cities have buildings"));
			N.talk_list.push_back(new_conversation_item("city", "it's safer in the cities"));
			N.talk_list.push_back(new_conversation_item("tree", "nature seeks a balance"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));	
			N.goal_list.push_back(0);
			N.goal_list.push_back(111);
			break;
		case 53: // generic robber npc
			N.id=53;
			N.sprite=53;
			N.grumble="stand and deliver";
			N.goal=99;	// initial goal is avatar
			N.talk_list.push_back(new_conversation_item("initial", "what do you want?"));
			N.talk_list.push_back(new_conversation_item("name", "Iago"));
			N.talk_list.push_back(new_conversation_item("job", "none of your business!"));
			N.talk_list.push_back(new_conversation_item("rob", "I'm just getting around to it"));
			N.talk_list.push_back(new_conversation_item("bye", "stand and deliver!;exit"));
			N.goal_list.push_back(99);
			break;
		case 54: // generic knight npc
			N.id=54;
			N.sprite=54;
			N.grumble="";
			N.goal=20;	// initial goal is bear
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Sir Donald"));
			N.talk_list.push_back(new_conversation_item("job", "knight"));
			N.talk_list.push_back(new_conversation_item("cricket", "very noisy"));
			N.talk_list.push_back(new_conversation_item("horse", "just a couple of coconuts"));
			N.talk_list.push_back(new_conversation_item("silly", "yes, silly mid-on"));
			N.talk_list.push_back(new_conversation_item("bye", "forsooth!;exit"));
			N.goal_list.push_back(20);
			N.goal_list.push_back(21);
			break;
		case 55: // generic cook npc
			N.id=55;
			N.sprite=55;
			N.grumble="";
			N.goal=-2;	// initial goal is none
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Pierre"));
			N.talk_list.push_back(new_conversation_item("job", "cook"));
			N.talk_list.push_back(new_conversation_item("food", "I love to bake bread"));
			N.talk_list.push_back(new_conversation_item("bread", "the best bread is in Paris"));
			N.talk_list.push_back(new_conversation_item("buy", "what would you like;bb"));
			N.talk_list.push_back(new_conversation_item("eat", "I better not, I just ate"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));
			break;			
		case 56: // generic monk npc
			N.id=56;
			N.sprite=56;
			N.grumble="amen";
			N.goal=62;	// initial goal is crucifix
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Francis"));
			N.talk_list.push_back(new_conversation_item("job", "monk"));
			N.talk_list.push_back(new_conversation_item("wheat", "used for beer"));
			N.talk_list.push_back(new_conversation_item("beer", "nectar of god!"));
			N.talk_list.push_back(new_conversation_item("drink", "why not indeed!"));
			N.talk_list.push_back(new_conversation_item("pray", "give me chastity, but not yet"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));
			N.goal_list.push_back(62);
			N.goal_list.push_back(55);
			break;		
		case 57: // generic musician npc
			N.id=57;
			N.sprite=57;
			N.grumble="do re mi";
			N.goal=51;	// initial goal is merchant
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Lydia"));
			N.talk_list.push_back(new_conversation_item("job", "musician"));
			N.talk_list.push_back(new_conversation_item("lute", "broke an E string"));
			N.talk_list.push_back(new_conversation_item("fret", "why worry?"));
			N.talk_list.push_back(new_conversation_item("note", "let's take this allegro"));
			N.talk_list.push_back(new_conversation_item("concert", "I played for the pope in Rome"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));
			N.goal_list.push_back(51);
			N.goal_list.push_back(55);
			break;	
		case 58:	// generic farmer npc
			N.id=58;
			N.sprite=58;
			N.grumble="arrr";
			N.goal=-1;	// initial goal is none
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Watt"));
			N.talk_list.push_back(new_conversation_item("what", "wat ho!"));
			N.talk_list.push_back(new_conversation_item("rain", "good for the farmers"));
			N.talk_list.push_back(new_conversation_item("farm", "I harvest wheat"));
			N.talk_list.push_back(new_conversation_item("wheat", "used for bread"));
			N.talk_list.push_back(new_conversation_item("bread", "talk to a cook"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));	
			break;		
		case 59:	// generic shepherd npc
			N.id=59;
			N.sprite=59;
			N.grumble="";
			N.goal=17;	// initial goal is sheep
			N.talk_list.push_back(new_conversation_item("initial", "hello!"));
			N.talk_list.push_back(new_conversation_item("name", "Seana"));
			N.talk_list.push_back(new_conversation_item("job", "shepherd"));
			N.talk_list.push_back(new_conversation_item("weather", "looks like rain"));
			N.talk_list.push_back(new_conversation_item("rain", "good for the farmers"));
			N.talk_list.push_back(new_conversation_item("sheep", "we all like sheep"));
			N.talk_list.push_back(new_conversation_item("farm", "I tend sheep"));
			N.talk_list.push_back(new_conversation_item("wool", "used for clothes"));
			N.talk_list.push_back(new_conversation_item("clothes", "good for cold weather"));
			N.talk_list.push_back(new_conversation_item("bye", "bye!;exit"));
			N.goal_list.push_back(17);
			N.goal_list.push_back(51);
			break;
		default:
			break;
	};
	
	return(N);
};

void world::remove_npc(int i){	// removes npc from list
	// standard function to remove item from vector
	int j;
	cout << "npc list is: <start> \n";
	for(j=0;j<(int) npcs.size();j++){
		cout << "npc " << j << " type " << npcs[j].id << "\n";
	};
	cout << "<end>\n";
	if(i<(int) npcs.size()){	// range check
		npcs.erase(npcs.begin()+i);
	};
	cout << "erased npc " << i << "\n";
	cout << "now npc list is:  <start>" << "\n";
	for(j=0;j<(int) npcs.size();j++){
		cout << "npc " << j << " type " << npcs[j].id << "\n";
	};
	cout << "<end>\n";
};
