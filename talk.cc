/* talk.cc	conversation functions */

string world::get_line_of_text(){
	string S;
	KeySym k;
	char c;
	S=">";	// initialize string
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
	// talk to npc in flora/fauna layer in relative location x,y
	int c;
	string S;
	c=flora_fauna_map[P.x+x][P.y+y];
	switch(c){
		case -1:
			add_new_message("[t]alk to nothing");
			S=get_line_of_text();
			break;
		case 8:
			add_new_message("talk to farmer");
			break;
		case 9:
			add_new_message("talk to robber");
			break;
		case 10:
			add_new_message("talk to merchant");
			break;
		case 11:
			add_new_message("talk to lumberjack");
			break;
		case 12:
			add_new_message("talk to npc");
			break;
		default:
			break;
	};
};