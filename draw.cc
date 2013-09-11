/* draw.cc draws the screen */

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
	XPoint q[4];
	
	x=P.x;
	y=P.y;
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
			q[2].x=365+(i*70)+r[a+1]*35;
			q[2].y=435-(j*70)-s[a+1]*35;
			q[2]=affine_transform(q[2]);
			q[2].y=q[2].y-height[r[a+1]][s[a+1]]*4-wall_height;
			q[3].x=365+(i*70)+r[a+1]*35;
			q[3].y=435-(j*70)-s[a+1]*35;
			q[3]=affine_transform(q[3]);
			q[3].y=q[3].y-height[r[a+1]][s[a+1]]*4;				
			c=d[a];
   			XSetForeground(display, gc, c);
			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
			XFillPolygon(display, win, gc, q, 4, Convex, CoordModeOrigin);
		};
	};
};


long determine_color(int h){	// h = sum of altitudes of vertices
	// interpolates colors for smoother graphics in geography layer
	long c;
	int r,g,b;
	if(h==0){
		r=0;
		g=40;
		b=255;
	} else {
		if(0<h && h<=12){
			r=0;
			g=(160*h)/12+(40*(12-h))/12;
			b=(255*(12-h))/12;
		};
		if(12<h && h<=24){
			r=0;
			g=((200*(h-12))+(160*(24-h)))/12;
			b=0;
		};
		if(24<h && h<=36){
			r=(160*(h-24))/12;
			g=((200*(36-h))+(128*(h-24)))/12;
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
				l=0x0000FF;	// water				0
				l=0x00A000; // low-lying grass		3
				l=0x00FF00; // grassland			6
				l=0xA08030; // scrub				9
				l=0xBBBBBB; // mountain				12
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


void world::draw_geographical_square(int i, int j){	// i,j in [-5,5] is location relative to P.x,P.y
	int a,b;
	int x,y;
	int height[3][3];
	long c;
	XPoint q[3];
	
	x=P.x;
	y=P.y;
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

void world::draw(){
	draw_graphics();
	draw_info();
};

void world::draw_graphics(){
	int i,j,k;
	int h;
	
	int tile_size;
	tile_size=70;	// should make this some global variable?

	erase_graphics_field();
	
	if(view_mode==true){
		// view mode map
		// draw overview map with 5x5 pixels just colored squares
	for(j=-80;j<80;j++){
		for(i=-80;i<80;i++){
				if((-1 < P.x+i) && (P.x+i < (int) world_map.size()) && (-1 < P.y+j) && (P.y+j < (int) world_map[0].size())){
					draw_square(5*(80+i),5*(80-j),5,color_code(world_map[P.x+i][P.y+j]));
					if(flora_fauna_map[P.x+i][P.y+j]>=100){	// city
						draw_square(5*(80+i),5*(80-j),5,0xFF0000);
					};
					if(wall_map.size()>0){	// if there is a wall map
						if(wall_map[P.x+i][P.y+j]>-1){	// wall
							draw_square(5*(80+i),5*(80-j),3,0x000000);
						};
					};
				};
			};
		};
		draw_square(400,400,5,0xFFC0CB);	// draw avatar square pink
	} else {
		// local map

		
		for(j=5;j>=-5;j--){
			for(i=5;i>=-5;i--){
				if((0 < P.x+i) && (P.x+i < (int) world_map.size()-1) && (0 < P.y+j) && (P.y+j < (int) world_map[0].size()-1)){
					
					draw_geographical_square(i,j);	// draw geography layer
					
					if(0 < (int) wall_map.size()){	// if map has a wall layer
						if(wall_map[P.x+i][P.y+j]>-1){
							draw_wall(i,j);				// draw wall layer
						};
					};
					
				};
			};
			for(i=5;i>=-5;i--){
				if((0 < P.x+i) && (P.x+i < (int) world_map.size()-1) && (0 < P.y+j) && (P.y+j < (int) world_map[0].size()-1)){

					h=world_map[P.x+i][P.y+j]*16;

					k=flora_fauna_map[P.x+i][P.y+j];
					if(k>=0 && k<100){
						draw_sprite(k,400+(i*tile_size),400-(j*tile_size),h);
					};
					if(k>=100 && k<200){
						switch(k){
							case 101:
								draw_sprite(15,400+(i*tile_size),400-(j*tile_size),h);	// London
								break;
							case 102:
								draw_sprite(16,400+(i*tile_size),400-(j*tile_size),h);	// Amsterdam
								break;
							case 103:
								draw_sprite(14,400+(i*tile_size),400-(j*tile_size),h);	// Paris
								break;
							default:
								draw_sprite(6,400+(i*tile_size),400-(j*tile_size),h);	// generic city; need to make these specific
								break;
						};
					};
					if(k>=200 && k<300){
						draw_sprite(12,400+(i*tile_size),400-(j*tile_size),h);	// npc; need to make these specific			
					};
					if(i==0 && j==0){
						h=world_map[P.x][P.y]*16;
						if(world_map[P.x][P.y]>=4){
							h=world_map[P.x][P.y]*20;
						};
						if(P.skill_item[0]==false){		// if not embarked
							draw_sprite(5,400,400,h);	// draw avatar
						} else {
							draw_sprite(13,400,400,h);	// draw boat
						};
					};
				};			
			};
		};		
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
	draw_text(p,T,0xAAFFAA);
	T.str("");
	T << "coordinates " << P.x << " " << P.y;
	p.y=90;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "view mode " << view_mode;
	p.y=110;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "edit mode " << edit_mode;
	p.y=130;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "moves " << moves;
	p.y=150;
	draw_text(p,T,0xFFFFFF);	
	
	
	T.str("");
	T << "health " << P.health;
	p.y=180;
	draw_text(p,T,0xFFAAAA);
	T.str("");
	T << "food " << P.food;
	p.y=200;
	draw_text(p,T,0xFFAAAA);
	T.str("");
	T << "wood " << P.wood;
	p.y=220;
	draw_text(p,T,0xFFAAAA);
	T.str("");
	T << "gold " << P.gold;
	p.y=240;
	draw_text(p,T,0xFFAAAA);
	
	for(i=0;i<MAX_SKILL_NUM;i++){
		T.str("");
		T << "skill " << i << " " << P.skill_item[i] << " " << P.skill[i];
		p.y=290+(20*i);
		draw_text(p,T,0xFFAAAA);
	};
	
	p.y=420;
	for(i=0;i<(int) message.size();i++){
		T.str("");
		T << message[i];
		p.y=p.y+20;
		draw_text(p,T,0xAAAAFF);
	};
	

};