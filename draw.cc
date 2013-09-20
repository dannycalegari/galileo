/* draw.cc draws the screen */

int world::center_height(int i, int j){
	int h;
	if(is_in_range(i,j,0)==false){
		h=0;
	} else if(world_map[i][j]>=4){
		h=world_map[i][j]*20;
	} else {
		h=world_map[i][j]*16;
	};
	return(h);
};

void world::draw_wall(int i, int j){	
	// i,j in [-5,5] is location relative to P.x,P.y
	int k;
	int a,b;
	vector<int> r,s;
	vector<long> d;
	int x,y;
	int height[3][3];
	int wall_height;
	long c;
	XPoint q[8];
	if(in_combat==true){
		x=6;
		y=6;
	} else {
		x=P.x;
		y=P.y;
	};
	k=wall_map[x+i][y+j];	// k is type of wall
	for(a=-1;a<2;a++){
		for(b=-1;b<2;b++){
			height[a+1][b+1]=world_map[x+i][y+j]+world_map[x+i+a][y+j]+world_map[x+i][y+j+b]+world_map[x+i+a][y+j+b];
			if(a==0 && b==0 && world_map[x+i][y+j]>=4){	// mountain tile
				height[1][1]=world_map[x+i][y+j]*5;
			};
		};
	};

	r.clear();
	s.clear();	
	d.clear();
	
	switch(k){
		case 0:		// EW wall
			wall_height=50;
			r.push_back(2);
			s.push_back(1);
			d.push_back(0xAAAAFF);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xAAAAFF);
			r.push_back(0);
			s.push_back(1);
			break;
		case 1:		// NS wall
			wall_height=50;
			r.push_back(1);
			s.push_back(2);
			d.push_back(0x9999EE);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0x9999EE);
			r.push_back(1);
			s.push_back(0);
			break;
		case 2:		// corner
			wall_height=80;
			r.push_back(2);
			s.push_back(1);
			d.push_back(0x555599);
			r.push_back(1);
			s.push_back(2);
			d.push_back(0x222266);
			r.push_back(0);
			s.push_back(1);
			d.push_back(0xDDDDFF);
			r.push_back(1);
			s.push_back(0);
			d.push_back(0x7777CC);
			r.push_back(2);
			s.push_back(1);
			break;
		case 3:		// NE diagonal
			wall_height=50;
			r.push_back(2);
			s.push_back(2);
			d.push_back(0x7777CC);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0x7777CC);
			r.push_back(0);
			s.push_back(0);
			break;
		case 4:		// NW diagonal
			wall_height=50;
			r.push_back(2);
			s.push_back(0);
			d.push_back(0xDDDDFF);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xDDDDFF);
			r.push_back(0);
			s.push_back(2);
			break;
		case 5:		// NE W
			wall_height=50;
			r.push_back(2);
			s.push_back(2);
			d.push_back(0x7777CC);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xAAAAFF);
			r.push_back(0);
			s.push_back(1);
			break;		
		case 6:		// NE S
			wall_height=50;
			r.push_back(2);
			s.push_back(2);
			d.push_back(0x7777CC);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0x9999EE);
			r.push_back(1);
			s.push_back(0);
			break;			
		case 7:		// NW E
			wall_height=50;
			r.push_back(0);
			s.push_back(2);
			d.push_back(0xDDDDFF);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xAAAAFF);
			r.push_back(2);
			s.push_back(1);
			break;	
		case 8:		// NW S
			wall_height=50;
			r.push_back(0);
			s.push_back(2);
			d.push_back(0xDDDDFF);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0x9999EE);
			r.push_back(1);
			s.push_back(0);
			break;	
		case 9:		// SE W
			wall_height=50;
			r.push_back(2);
			s.push_back(0);
			d.push_back(0xDDDDFF);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xAAAAFF);
			r.push_back(0);
			s.push_back(1);
			break;
		case 10:	// SE N
			wall_height=50;
			r.push_back(1);
			s.push_back(2);
			d.push_back(0x9999EE);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xDDDDFF);
			r.push_back(2);
			s.push_back(0);
			break;
		case 11:	// SW E
			wall_height=50;
			r.push_back(0);
			s.push_back(0);
			d.push_back(0x7777CC);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0xAAAAFF);
			r.push_back(2);
			s.push_back(1);
			break;		
		case 12:	// SW N
			wall_height=50;
			r.push_back(1);
			s.push_back(2);	
			d.push_back(0x9999EE);
			r.push_back(1);
			s.push_back(1);
			d.push_back(0x7777CC);
			r.push_back(0);
			s.push_back(0);	
		default:
			break;
	};
	if(r.size()>=1){
		for(a=0;a<(int) r.size()-1;a++){
			q[0].x=365+(i*70)+r[a]*35;
			q[0].y=435-(j*70)-s[a]*35;
			q[0]=affine_transform(q[0]);
			q[0].y=q[0].y-height[r[a]][s[a]]*4;
			q[1].x=365+(i*70)+r[a]*35;
			q[1].y=435-(j*70)-s[a]*35;
			q[1]=affine_transform(q[1]);
			q[1].y=q[1].y-height[r[a]][s[a]]*4-wall_height;			
			q[6].x=365+(i*70)+r[a+1]*35;
			q[6].y=435-(j*70)-s[a+1]*35;
			q[6]=affine_transform(q[6]);
			q[6].y=q[6].y-height[r[a+1]][s[a+1]]*4-wall_height;
			q[2].x=(int) q[1].x*0.6666666+q[6].x*0.3333334;
			q[2].y=(int) q[1].y*0.6666666+q[6].y*0.3333334;	
			q[5].x=(int) q[1].x*0.3333334+q[6].x*0.6666666;
			q[5].y=(int) q[1].y*0.3333334+q[6].y*0.6666666;
			q[3].x=q[2].x;
			q[3].y=q[2].y+20;
			q[4].x=q[5].x;
			q[4].y=q[5].y+20;
			q[7].x=365+(i*70)+r[a+1]*35;
			q[7].y=435-(j*70)-s[a+1]*35;
			q[7]=affine_transform(q[7]);
			q[7].y=q[7].y-height[r[a+1]][s[a+1]]*4;				
			c=d[a];
   			XSetForeground(display, gc, c);
			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
			XFillPolygon(display, win, gc, q, 8, Nonconvex, CoordModeOrigin);
		};
	};
};


long determine_color(int k){	// k = sum of altitudes of vertices
	// interpolates colors for smoother graphics in geography layer
	long c;
	int r,g,b;
	int h;
	h=k;
	if(h==0){	
		h=h+rand()%3;	// random perturbation of color of flat water tile (waves)
	};
	if(h==0){
		r=0;
		g=40;
		b=155;
	} else {
		if(0<h && h<=12){
			r=0;
			g=(112*h)/12+(40*(12-h))/12;
			b=(155*(12-h))/12;
		};
		if(12<h && h<=24){
			r=0;
			g=((155*(h-12))+(112*(24-h)))/12;
			b=0;
		};
		if(24<h && h<=36){
			r=(160*(h-24))/12;
			g=((155*(36-h))+(128*(h-24)))/12;
			b=(48*(h-24))/12;
		};
		if(36<h && h<=48){
			r=((187*(h-36))+(160*(48-h)))/12;
			g=((187*(h-36))+(128*(48-h)))/12;
			b=((187*(h-36))+(48*(48-h)))/12;
		};
		if(48<h){
			r=255;
			g=255;
			b=255;
		};
	};
	c=(256*256*r)+(256*g)+b;
	return(c);
		/*
				0x00009B									  0   0 155
				0x007000									  0 112   0
				0x009B00									  0 155   0
				
				l=0x0000FF;	// water				0		  0   0 255
				l=0x00A000; // low-lying grass		3		  0	160   0
				l=0x00FF00; // grassland			6		  0 200   0
				l=0xA08030; // scrub				9		160 128  48
				l=0xBBBBBB; // mountain				12		187 187 187
		*/
};

long shadow(long c){
	int r,g,b;
	long d;
	b = c%256;
	g = (c/256)%256;
	r = (c/(256*256))%256;
	
	r=r*0.7;
	g=g*0.7;
	b=b*0.7;
	d=(256*256*r)+(256*g)+b;
	return(d);
};


void world::draw_geographical_square(int i, int j){	// i,j in [-5,5] is location relative to x,y
	int a,b;
	int x,y;
	int height[3][3];
	long c;
	XPoint q[3];
	
	if(in_combat==true){
		x=6;
		y=6;
	} else {
		x=P.x;
		y=P.y;
	};
	for(a=-1;a<2;a++){
		for(b=-1;b<2;b++){
			height[a+1][b+1]=world_map[x+i][y+j]+world_map[x+i+a][y+j]+world_map[x+i][y+j+b]+world_map[x+i+a][y+j+b];
			if(a==0 && b==0 && world_map[x+i][y+j]>=4){	// mountain tile
				height[1][1]=world_map[x+i][y+j]*5;
			};
		};
	};
	
	for(b=1;b>=0;b--){
		for(a=1;a>=0;a--){
			if((a+b)%2==0){
				q[0].x=365+(i*70)+(a*35);
				q[0].y=435-(j*70)-(b*35);
				q[0]=affine_transform(q[0]);
				q[0].y=q[0].y-height[a][b]*4;
				q[1].x=365+(i*70)+(a*35);
				q[1].y=435-35-(j*70)-(b*35);
				q[1]=affine_transform(q[1]);
				q[1].y=q[1].y-height[a][b+1]*4;
				q[2].x=365+35+(i*70)+(a*35);
				q[2].y=435-(j*70)-(b*35);
				q[2]=affine_transform(q[2]);
				q[2].y=q[2].y-height[a+1][b]*4;
				c=determine_color(height[a][b]+height[a][b+1]+height[a+1][b]);
   				if(b==1 && height[1][1]>=20){	// mountain
   					c=0xAAAAAA;
   				};
   				XSetForeground(display, gc, c);
    			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    			XFillPolygon(display, win, gc, q, 3, Convex, CoordModeOrigin);
    			
				q[0].x=365+35+(i*70)+(a*35);
				q[0].y=435-35-(j*70)-(b*35);
				q[0]=affine_transform(q[0]);
				q[0].y=q[0].y-height[a+1][b+1]*4;
				q[1].x=365+(i*70)+(a*35);
				q[1].y=435-35-(j*70)-(b*35);
				q[1]=affine_transform(q[1]);
				q[1].y=q[1].y-height[a][b+1]*4;
				q[2].x=365+35+(i*70)+(a*35);
				q[2].y=435-(j*70)-(b*35);
				q[2]=affine_transform(q[2]);
				q[2].y=q[2].y-height[a+1][b]*4;
				c=determine_color(height[a+1][b+1]+height[a][b+1]+height[a+1][b]);
   				XSetForeground(display, gc, c);
    			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    			XFillPolygon(display, win, gc, q, 3, Convex, CoordModeOrigin);
			} else {
				q[0].x=365+35+(i*70)+(a*35);
				q[0].y=435-(j*70)-(b*35);
				q[0]=affine_transform(q[0]);
				q[0].y=q[0].y-height[a+1][b]*4;
				q[1].x=365+35+(i*70)+(a*35);
				q[1].y=435-35-(j*70)-(b*35);
				q[1]=affine_transform(q[1]);
				q[1].y=q[1].y-height[a+1][b+1]*4;
				q[2].x=365+(i*70)+(a*35);
				q[2].y=435-(j*70)-(b*35);
				q[2]=affine_transform(q[2]);
				q[2].y=q[2].y-height[a][b]*4;
				c=determine_color(height[a+1][b]+height[a+1][b+1]+height[a][b]);
   				if(b==1 && height[1][1]>=20){	// mountain
   					c=0xDDDDDD;
   				};
   				XSetForeground(display, gc, c);
    			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    			XFillPolygon(display, win, gc, q, 3, Convex, CoordModeOrigin);
    			
				q[0].x=365+(i*70)+(a*35);
				q[0].y=435-35-(j*70)-(b*35);
				q[0]=affine_transform(q[0]);
				q[0].y=q[0].y-height[a][b+1]*4;
				q[1].x=365+35+(i*70)+(a*35);
				q[1].y=435-35-(j*70)-(b*35);
				q[1]=affine_transform(q[1]);
				q[1].y=q[1].y-height[a+1][b+1]*4;
				q[2].x=365+(i*70)+(a*35);
				q[2].y=435-(j*70)-(b*35);
				q[2]=affine_transform(q[2]);
				q[2].y=q[2].y-height[a][b]*4;
				c=determine_color(height[a][b+1]+height[a+1][b+1]+height[a][b]);
   				if(b==0 && height[1][1]>=20){	// mountain
   					c=0xCCCCCC;
   				};
   				XSetForeground(display, gc, c);
    			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    			XFillPolygon(display, win, gc, q, 3, Convex, CoordModeOrigin);			
			};
		};
	};
};

long color_code(int i){	// color scheme for overview map
	long l;
	switch(i) {
			case -1:	// empty space
				l=0x000000;
				break;
			case 0:		// water tile
				l=0x0000FF;
				break;
			case 1:		// low-lying grass
				l=0x00A000;
				break;
			case 2:		// grassland
				l=0x00FF00;
				break;
			case 3:		// scrub
				l=0xA08030;
				break;
			case 4:		// mountain
				l=0xBBBBBB;
				break;
            default:
            	l=0xFFFFFF;
            	break;
	};
	return(l);
};

void world::draw_graphics(){
	int i,j,k;
	int h;
	int x,y;
	XPoint p;
	point q;
	
	int tile_size;
	tile_size=70;	// should make this some global variable?

	erase_graphics_field();
	if(in_combat==true){
		x=6;
		y=6;
	} else {
		x=P.x;
		y=P.y;
	};
	
	if(view_mode==true){
		// view mode map
		// draw overview map with 5x5 pixels just colored squares
		for(j=-80;j<80;j++){
			for(i=-80;i<80;i++){
				if(is_in_range(x+i,j+y,0)){
					draw_square(5*(80+i),5*(80-j),5,color_code(world_map[x+i][y+j]));
					if(flora_fauna_map[x+i][y+j]>=100){	// city
						draw_square(5*(80+i),5*(80-j),5,0xFF0000);
					};
					if(flora_fauna_map[x+i][y+j]>9){	// ff
						draw_square(5*(80+i),5*(80-j),2,0x000000);	
					};
					if(wall_map.size()>0){	// if there is a wall map
						if(wall_map[x+i][y+j]>-1){	// wall
							draw_square(5*(80+i),5*(80-j),3,0x000000);
						};
					};
				};
			};
		};
		if(in_combat==true){
			// draw avatar square in combat
			draw_square(400+(P.x-6)*5,400-(P.y-6)*5,5,0xFFC0CB);
		} else {
			draw_square(400,400,5,0xFFC0CB);	// draw avatar square pink
		};
	} else {
		// local map

		
		for(j=5;j>=-5;j--){
			for(i=5;i>=-5;i--){
				if(is_in_range(x+i,y+j,0)){
					draw_geographical_square(i,j);	// draw geography layer
					if(0 < (int) wall_map.size()){	// if map has a wall layer
						if(wall_map[x+i][y+j]>-1){
							draw_wall(i,j);				// draw wall layer
						};
					};
					
				};
			};
			for(i=5;i>=-5;i--){
				if(is_in_range(x+i,y+j,0)){
					h=center_height(x+i,y+j);
					k=flora_fauna_map[x+i][y+j];
					if(k>=0 && k<100){
						draw_sprite(k,400+(i*tile_size),400-(j*tile_size),h);
					};
					if(k>=100 && k<200){
						switch(k){
							case 101:
								draw_sprite(31,400+(i*tile_size),400-(j*tile_size),h);	// London
								break;
							case 102:
								draw_sprite(32,400+(i*tile_size),400-(j*tile_size),h);	// Amsterdam
								break;
							case 103:
								draw_sprite(33,400+(i*tile_size),400-(j*tile_size),h);	// Paris
								break;
							case 104:
								draw_sprite(34,400+(i*tile_size),400-(j*tile_size),h);	// Venice
								break;							
							default:
								draw_sprite(41,400+(i*tile_size),400-(j*tile_size),h);	// generic city; need to make these specific
								break;
						};
					};
					if(k>=200 && k<300){
						draw_sprite(12,400+(i*tile_size),400-(j*tile_size),h);	// npc; need to make these specific			
					};
					if(in_combat==true){
						if(occupied_by_special(i+6,j+6)>-1){	// draw monsters
							k=monsters[occupied_by_special(i+6,j+6)].id;
							h=center_height(x+i,y+j);
							draw_sprite(k,400+(i*tile_size),400-(j*tile_size),h);
						};
					
						if(i==P.x-6 && j==P.y-6){	// draw avatar
							h=center_height(x+i,y+j);
							if(embarked==true){		// if not embarked
								draw_sprite(60,400+(i*tile_size),400-(j*tile_size),h);	// draw boat
							} else if(riding==true){
								draw_sprite(16,400+(i*tile_size),400-(j*tile_size),h);	// draw horse
							} else {
								draw_sprite(99,400+(i*tile_size),400-(j*tile_size),h);	// draw avatar
							};						
						};
					} else {
						if(occupied_by_special(i+x,j+y)>-1){	// draw npcs
							k=npcs[occupied_by_special(i+x,j+y)].id;
							h=center_height(x+i,y+j);
							draw_sprite(k,400+(i*tile_size),400-(j*tile_size),h);						
						};
					
						if(i==0 && j==0){	// draw avatar
							h=center_height(P.x,P.y);
							if(embarked==true){		// if not embarked
								draw_sprite(60,400,400,h);	// draw boat
							} else if(riding==true){
								draw_sprite(16,400,400,h);	// draw horse
							} else {
								draw_sprite(99,400,400,h);	// draw avatar
							};
						};
					};
				};			
			};	
		};		
		for(i=0;i<(int) popup_message.size();i++){
			h=center_height(popup_message[i].x+x,popup_message[i].y+y);
			p.x=400+(popup_message[i].x*tile_size)+50;
			p.y=400-(popup_message[i].y*tile_size)-90-h;
			p=affine_transform(p);
			q.x=p.x;	// translate XPoint to point
			q.y=p.y;
			draw_text(q,popup_message[i].S,0xFFFFFF);
		};
		popup_message.clear();	// popup messages only last one move
	};
};

void world::draw_info(){
	stringstream T;
	point p;
	int i;

	erase_writing_field();
	p.x=1150;

	T << "map: " << map_name;
	p.y=50;	
	draw_text(p,T.str(),0xAAFFAA);
	T.str("");
	T << "location " << P.x << " " << P.y;
	p.y=90;
	draw_text(p,T.str(),0xFFFFFF);
	T.str("");
	T << "view mode ";
	if(view_mode){
		T << "on";
	} else {
		T << "off";
	};
	p.y=110;
	draw_text(p,T.str(),0xFFFFFF);
	T.str("");
	T << "edit mode ";
	if(edit_mode){
		T << "on";
	} else {
		T << "off";
	};
	p.y=130;
	draw_text(p,T.str(),0xFFFFFF);
	T.str("");
	T << "moves " << moves;
	p.y=150;
	draw_text(p,T.str(),0xFFFFFF);	
	
	
	T.str("");
	T << "H: " << P.health << " F: " << P.food << " G: " << P.gold << " W: " << P.wood;
	p.y=180;
	draw_text(p,T.str(),0xFFAAAA);
	T.str("");
	T << "D:  " << P.dexterity << "  S:  " << P.strength << "  I:  " << P.intelligence;
	p.y=200;
	draw_text(p,T.str(),0xFFAAAA);
	
	
	draw_line(1150,220,1350,220,0x0000FF);
	
	p.y=240;
	for(i=0;i<(int) message.size();i++){
		T.str("");
		T << message[i];
		p.y=p.y+20;
		if(message[i][0]=='>'){
			draw_text(p,message[i].substr(2,message[i].size()-2),0xFFD700);
		} else if(message[i][0]==':'){
			draw_text(p,message[i].substr(2,message[i].size()-2),0xFFF9F0);
		} else {
			draw_text(p,message[i],0xAAAAFF);
		};
	};

};

void world::draw_inventory(){
	stringstream T;
	point p;
	int i;

	erase_writing_field();
	p.x=1150;

	T << "map: " << map_name;
	p.y=50;	
	draw_text(p,T.str(),0xAAFFAA);
	T.str("");
	T << "location " << P.x << " " << P.y;
	p.y=90;
	draw_text(p,T.str(),0xFFFFFF);
	T.str("");
	T << "view mode ";
	if(view_mode){
		T << "on";
	} else {
		T << "off";
	};
	p.y=110;
	draw_text(p,T.str(),0xFFFFFF);
	T.str("");
	T << "edit mode ";
	if(edit_mode){
		T << "on";
	} else {
		T << "off";
	};
	p.y=130;
	draw_text(p,T.str(),0xFFFFFF);
	T.str("");
	T << "moves " << moves;
	p.y=150;
	draw_text(p,T.str(),0xFFFFFF);	
	
	
	T.str("");
	T << "H: " << P.health << " F: " << P.food << " G: " << P.gold << " W: " << P.wood;
	p.y=180;
	draw_text(p,T.str(),0xFFAAAA);
	T.str("");
	T << "D:  " << P.dexterity << "  S:  " << P.strength << "  I:  " << P.intelligence;
	p.y=200;
	draw_text(p,T.str(),0xFFAAAA);
	
	for(i=0;i<(int) P.item_list.size();i++){
		T.str("");
		T << P.item_list[i];
		p.y=290+(20*i);
		draw_text(p,T.str(),0xFFAAAA);
	};
};

void world::draw(){
	draw_graphics();
	draw_info();
};