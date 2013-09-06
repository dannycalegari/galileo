/* interface.cc */

void world::user_interface(){
	bool finished;
	point p;
	int x,y;

	finished=false;
	
	while(finished==false){
		XNextEvent(display, &report);
		switch(report.type) {
			case ButtonPress:
				p=mouse_location();
				break;
			case KeyPress:
			
				// move RLUD
				
				if(XLookupKeysym(&report.xkey, 0) == XK_Right){
					last_command="move right";
					attempt_move(1,0);
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Left){
					last_command="move left";
					attempt_move(-1,0);
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Up){
					last_command="move up";
					attempt_move(0,1);
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Down){
					last_command="move down";
					attempt_move(0,-1);
					draw();
				};
				
				// toggle view/edit modes
				
				if(XLookupKeysym(&report.xkey, 0) == XK_v){	// toggle view mode
					if(view_mode==false){
						view_mode=true;
						last_command="over[v]iew mode on";
						erase_field();
					} else {
						view_mode=false;
						last_command="over[v]iew mode off";
						erase_field();
					};
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_e){ // toggle edit mode
					if(edit_mode==false){
						edit_mode=true;
						last_command="[e]dit mode on";
						erase_field();
					} else {
						edit_mode=false;
						last_command="[e]dit mode off";
						erase_field();
					};
					draw();				
				};
				
				if(edit_mode==true){	// EDIT MODE
				
				// in edit mode: raise/lower geography
				
					if(XLookupKeysym(&report.xkey, 0) == XK_equal){ // raise geography
						world_map[P.x][P.y]++;
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_minus){ // lower geography
						if(world_map[P.x][P.y]>0){
							world_map[P.x][P.y]--;
						};
						draw();
					};	
				
				// in edit mode: add to flora_fauna_map
				
					if(XLookupKeysym(&report.xkey, 0) == XK_i){	// add object to flora/fauna layer
						flora_fauna_map[P.x][P.y]=(flora_fauna_map[P.x][P.y]+1) % MAX_SPRITE_NUM;
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_o){	// clear flora/fauna layer
						flora_fauna_map[P.x][P.y]=-1;
						draw();
					};
					
				} else {				// GAME MODE
					if(XLookupKeysym(&report.xkey, 0) == XK_b){	// board boat toggle
						if(P.embarked==false){
							if(flora_fauna_map[P.x][P.y]==13 && world_map[P.x][P.y]==0){	// if on a boat on water
								P.embarked=true;
								flora_fauna_map[P.x][P.y]=-1;	// remove boat from map
								last_command="[b]oard boat";
							};
						} else {
							P.embarked=false;
							flora_fauna_map[P.x][P.y]=13;	// put boat back on map
							last_command="exit [b]oat";
						};
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_u){
						last_command="[u]se object; which direction? ";
						draw_info();
						select_direction_interface(x,y);
						use_object(x,y);
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_t){
						last_command="[t]alk; which direction? ";
						draw_info();
						select_direction_interface(x,y);
						talk(x,y);
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_a){
						last_command="[a]ttack; which direction? ";
						draw_info();
						select_direction_interface(x,y);
						attack(x,y);
						draw();
					};
				};

				if(XLookupKeysym(&report.xkey, 0) == XK_space){ // update state
					last_command="pass";
					update_map();
					draw();
				};
				
				if(XLookupKeysym(&report.xkey, 0) == XK_s){	// save state
					last_command="[s]ave game";
					draw();
					save_state();
				};
				
				
				if(XLookupKeysym(&report.xkey, 0) == XK_q){ // quit       
					last_command="[q]uit";
                    finished=true;
                    XCloseDisplay(display);
                    exit(0);
                    break;
                };
            default:
            	break;
        };
    };
};

void world::select_direction_interface(int &select_direction_x, int &select_direction_y){
	bool finished;
	point p;

	select_direction_x=0;	// default
	select_direction_y=0;	// default
	
	finished=false;
	
	while(finished==false){
		XNextEvent(display, &report);
		switch(report.type) {
			case ButtonPress:
				p=mouse_location();
				break;
			case KeyPress:
			
				// use RLUD
				
				if(XLookupKeysym(&report.xkey, 0) == XK_Right){
					select_direction_x=1;
					select_direction_y=0;
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Left){
					select_direction_x=-1;
					select_direction_y=0;
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Up){
					select_direction_x=0;
					select_direction_y=1;
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Down){
					select_direction_x=0;
					select_direction_y=-1;
				};
				finished=true;
				break;
				
            default:
            	break;
        };
    };
};
