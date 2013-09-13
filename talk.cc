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
	c=occupied_by_special(P.x+x,P.y+y);
	if(c>-1){
		conversation_with_npc(c);
	} else {
		c=flora_fauna_map[P.x+x][P.y+y];
		switch(c){
			case 8:
				add_new_message(": nice weather we're having");	// farmer
				break;
			case 9:
				add_new_message(": your money or your life");	// robber
				break;
			case 10:
				add_new_message("talk to merchant");
				break;
			case 11:
				add_new_message("talk to lumberjack");
				break;
			default:
				add_new_message("not here");
				break;
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
	string S,T;
	bool conversation_over;
	S="initial";
	conversation_over=false;
	while(conversation_over==false){
		T=get_response(c,S);	// response of npc[c] to string S
		add_new_message(": "+T);
		draw_info();
		if(T=="bye!"){
			conversation_over=true;
		} else {
			S=get_line_of_text();
		};
	};
};