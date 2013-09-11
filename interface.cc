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
					add_new_message("east");
					attempt_move(1,0);
					if(edit_mode==false){
						update_map();
					};
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Left){
					add_new_message("west");					
					attempt_move(-1,0);
					if(edit_mode==false){
						update_map();
					};
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Up){
					add_new_message("north");
					attempt_move(0,1);
					if(edit_mode==false){
						update_map();
					};
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Down){
					add_new_message("south");
					attempt_move(0,-1);
					if(edit_mode==false){
						update_map();
					};
					draw();
				};
				
				// toggle view/edit modes
				
				if(XLookupKeysym(&report.xkey, 0) == XK_v){	// toggle view mode
					if(view_mode==false){
						view_mode=true;
						add_new_message("[v]iew mode on");
						erase_field();
					} else {
						view_mode=false;
						add_new_message("[v]iew mode off");
						erase_field();
					};
					draw();
				};
				if(XLookupKeysym(&report.xkey, 0) == XK_Escape){ // toggle edit mode
					if(edit_mode==false){
						edit_mode=true;
						add_new_message("[e]dit mode on");
						erase_field();
					} else {
						edit_mode=false;
						add_new_message("[e]dit mode off");
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
					
					if(XLookupKeysym(&report.xkey, 0) == XK_2){	// add object to wall layer
						wall_map[P.x][P.y]=(wall_map[P.x][P.y]+1) % MAX_WALL_NUM;
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_1){	// clear wall layer
						wall_map[P.x][P.y]=-1;
						draw();
					};	
					
					if(XLookupKeysym(&report.xkey, 0) == XK_s){	// save current map
						add_new_message("[s]ave current map: "+map_name);
						draw();
						save_current_map();
					};
					
				} else {				// GAME MODE
					if(XLookupKeysym(&report.xkey, 0) == XK_b){	// board boat toggle
						if(P.skill_item[0]==false){		// in not embarked
							if(flora_fauna_map[P.x][P.y]==13 && world_map[P.x][P.y]==0){	// if on a boat on water
								P.skill_item[0]=true;	// embark
								flora_fauna_map[P.x][P.y]=-1;	// remove boat from map
								add_new_message("[b]oard boat");
								update_map();
							};
						} else {
							P.skill_item[0]=false;	// disembark
							flora_fauna_map[P.x][P.y]=13;	// put boat back on map
							add_new_message("exit [b]oat");
							update_map();
						};
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_u){
						add_new_message("[u]se; which direction? ");
						draw_info();
						select_direction_interface(x,y);
						use_object(x,y);
						update_map();
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_t){
						add_new_message("[t]alk; which direction? ");
						draw_info();
						select_direction_interface(x,y);
						talk(x,y);
						update_map();
						draw();
					};
					if(XLookupKeysym(&report.xkey, 0) == XK_a){
						add_new_message("[a]ttack; which direction? ");
						draw_info();
						select_direction_interface(x,y);
						attack(x,y);
						update_map();
						draw();
					};
				};

				if(XLookupKeysym(&report.xkey, 0) == XK_space){ // update state
					add_new_message("pass");
					update_map();
					draw();
				};

				
				if(XLookupKeysym(&report.xkey, 0) == XK_e){	// enter city
					add_new_message("[e]nter city");
					if(flora_fauna_map[P.x][P.y]>=100){	// if on a city
						switch(flora_fauna_map[P.x][P.y]){
							case 100:	// Cambridge
								break;
							case 101:	// London
								enter_city("london");
								break;
							case 102:	// Amsterdam
								enter_city("amsterdam");
								break;
							case 103:	// Paris
								enter_city("paris");
								break;
							case 104:
								break;
							case 105:
								break;
							case 106:
								break;
							case 107:
								break;
							case 108:
								break;
							case 109:
								break;
							case 110:
								break;
							default:
								break;
						};
					};
					draw();
				};
				
				if(XLookupKeysym(&report.xkey, 0) == XK_q){ // quit       
					add_new_message("[q]uit");
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
