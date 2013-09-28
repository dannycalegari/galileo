/* talk.cc	conversation functions */

string world::get_line_of_text(){
	string S;
	KeySym k;
	char c;
	S="> ";	// initialize string
	add_new_message(S);	
	draw_info();
	bool line_finished;
	line_finished=false;	// look for end of line character
	while(line_finished==false){
		XNextEvent(display, &report);
		switch(report.type) {
			case KeyPress:
				k=XLookupKeysym(&report.xkey, 0);
				if(k >= 0x041 && k <= 0x05a){	// upper case
					k=k+0x020;
				} else 	if(k >=  0x061 && k <= 0x07a){
					c=char('a'+(int) (k-97));
					S=S+c;
					modify_last_line(S);
					draw_info();
				} else if(k == XK_space){
					S=S+' ';
				} else if(k == XK_Delete || k == XK_BackSpace){
					if(S.size()>1){
						S.erase(S.size()-1);	// strip off last character;
						modify_last_line(S);
						draw_info();
					//	S.pop_back();	// alternate code; needs C++11 support
					};
				} else if(XLookupKeysym(&report.xkey, 0) == XK_Return){
					line_finished=true;
				};
				break;
			default:
				break;
		};
	};
	return(S);
};

int world::get_number(){
	int n,k;
	n=0;	// initial value
	string S;
	S="> "; // initialize string
	char c;
	add_new_message(S);
	draw_info();
	bool line_finished;
	line_finished=false;	// look for end of line character
	while(line_finished==false){
		XNextEvent(display, &report);
		switch(report.type) {
			case KeyPress:
				k=XLookupKeysym(&report.xkey, 0);
				if(k >= 0x030 && k <= 0x039){	// number
					n=(10*n)+(k-48);
					c=char('0'+(int) (k-48));
					S=S+c;
					modify_last_line(S);
					draw_info();
				} else if(k == XK_Delete || k == XK_BackSpace){
					if(S.size()>1){
						n=n/10;
						S.erase(S.size()-1);	// strip off last character;
						modify_last_line(S);
						draw_info();
					//	S.pop_back();	// alternate code; needs C++11 support
					};
				} else if(XLookupKeysym(&report.xkey, 0) == XK_Return){
					line_finished=true;
				};
				break;
			default:
				break;
		};
	};
	return(n);
};

string world::get_line_of_text(string R){	// initial prompt
	string S;
	KeySym k;
	char c;
	S="> "+R;	// initialize string
	add_new_message(S);	
	draw_info();
	bool line_finished;
	line_finished=false;	// look for end of line character
	while(line_finished==false){
		XNextEvent(display, &report);
		switch(report.type) {
			case KeyPress:
				k=XLookupKeysym(&report.xkey, 0);
				if(k >= 0x041 && k <= 0x05a){	// upper case
					k=k+0x020;
				} else 	if(k >=  0x061 && k <= 0x07a){
					c=char('a'+(int) (k-97));
					S=S+c;
					modify_last_line(S);
					draw_info();
				} else if(k == XK_space){
					S=S+' ';
				} else if(k == XK_Delete || k == XK_BackSpace){
					if(S.size()>1){
						S.erase(S.size()-1);	// strip off last character;
						modify_last_line(S);
						draw_info();
					//	S.pop_back();	// alternate code; needs C++11 support
					};
				} else if(XLookupKeysym(&report.xkey, 0) == XK_Return){
					line_finished=true;
				};
			default:
				break;
		};
	};
	return(S);
};

void world::talk(int x, int y){
	// talk to npc in relative location x,y
	int c;
	string S;
	if(in_combat==true){
		add_new_message("not in combat");
	} else {
		c=occupied_by_special(P.x+x,P.y+y);
		if(c>-1){	// real npc with scripted (canned) responses
			conversation_with_npc(c);
		} else {	// generic npc type
			c=flora_fauna_map[P.x+x][P.y+y];
			switch(c){
				case 50:
					add_new_message(": Nice weather we're having");	// milkmaid
					break;
				case 51:
					add_new_message(": I'm too busy to talk to you"); // merchant
					break;
				case 52:
					add_new_message(": Lovely trees"); // woodcutter
					break;
				case 53:
					add_new_message(": Your money or your life!");	// robber
					break;
				case 54:
					add_new_message(": Too busy to talk");	// generic npc
				default:
					add_new_message("not here");
					break;
			};
		};
	};
};

string world::get_response(int c, string S){
	int i;
	size_t found;
	string Q,R;
	bool found_prompt;
	found_prompt=false;
	for(i=0;i< (int) npcs[c].talk_list.size();i++){
		Q=npcs[c].talk_list[i].prompt;
		found=S.find(npcs[c].talk_list[i].prompt);	// look for prompt in string S
		if(found!=std::string::npos){	// found it!
			found_prompt=true;
			R=npcs[c].talk_list[i].reply;	// get canned reply
		};
	};
	if(found_prompt==false){
		R="what?";	// default reply
	};
	return(R);
};

void world::conversation_with_npc(int c){
	string S,T,U;
	size_t pos;
	bool conversation_over;
	S="initial";
	conversation_over=false;
	while(conversation_over==false){
		T=get_response(c,S);	// response of npc[c] to string S
		pos = T.find(';',0);	// does response have verbal and nonverbal component?
		if(pos!=string::npos){
			U=T.substr(pos+1,T.size()-pos-1);	// U is nonverbal response
			T=T.substr(0,pos);					// T is verbal response
		} else {
			U="";								// no nonverbal response
		};
		add_new_message(": "+T);
		if(U.size()>0){
			nonverbal_response(c,U);
			// analyze nonverbal response
		};
		draw_info();
		if(U=="exit"){
			conversation_over=true;
		} else {
			S=get_line_of_text();
		};
	};
};

void world::nonverbal_response(int c,string U){	// npc[c] has response U
	int i;
	char d;
	if(U[0]=='b'){	// buy
		d=U[1];
		i=(int) d - (int) 'a';
		commerce_routine(i);
	};
};

struct sale_item{
	string item;
	int price;
};

void world::commerce_routine(int i){
	string S;
	char c;
	stringstream T;
	vector<sale_item> wares;
	sale_item I;
	int j,k;
	switch(i){
		case 0: // blacksmith
			wares.clear();
			I.item="horseshoes";
			I.price=10;
			wares.push_back(I);
			I.item="sword";
			I.price=70;
			wares.push_back(I);
			break;
		case 1:	// baker
			wares.clear();
			I.item="bread";
			I.price=5;
			wares.push_back(I);
			I.item="pie";
			I.price=10;
			wares.push_back(I);
			I.item="cake";
			I.price=20;
			wares.push_back(I);
			break;
		case 2:	// dairy
			wares.clear();
			I.item="milk";
			I.price=5;
			wares.push_back(I);
			I.item="cheese";
			I.price=8;
			wares.push_back(I);
			break;		
		case 3:	// clothes
			wares.clear();
			I.item="dress";
			I.price=100;
			wares.push_back(I);
			I.item="cloak";
			I.price=45;
			wares.push_back(I);
			break;
		case 9: // provisioner
			add_new_message(": how many rations (1 each)?");
			j=get_number();
			if(j>0 && j <=P.gold){
				P.gold=P.gold-j;
				P.food=P.food+j;
			} else if (j>P.gold){
				add_new_message(": not enough gold!");
			};
			break;
		default:
			break;
	};
	if(i<9){
		for(j=0;j<(int) wares.size();j++){
			T.str("");
			T << ": [" << (char) ((int) 'a'+j) << "] " << wares[j].item << " " << wares[j].price;
			add_new_message(T.str());
		};
		T.str("");
		T << ": [" << (char) ((int) 'a'+(int) wares.size()) << "] nothing";
		add_new_message(T.str());
		S=get_line_of_text();
		c=S[2];	// strip off initial "> "
		k=(int) c - (int) 'a';
		if(0<=k && k<(int) wares.size()){	// if it is on the list
			if(P.gold>=wares[k].price){	// if P has enough money
				P.gold=P.gold-wares[k].price;
				add_new_object(wares[k].item);
				add_new_message(": here you are");
			} else {
				add_new_message(": not enough gold!");
			};
		};
	};
};