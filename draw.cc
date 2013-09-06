/* draw.cc draws the screen */


long determine_color(int h){	// h = sum of altitudes of vertices
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
		for(a=0;a<2;a++){
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
				if((-1 < P.x+i) && (P.x+i < 1000) && (-1 < P.y+j) && (P.y+j < 800)){
					draw_square(5*(80+i),5*(80-j),5,color_code(world_map[P.x+i][P.y+j]));
					if(flora_fauna_map[P.x+i][P.y+j]>=100){	// city
						draw_square(5*(80+i),5*(80-j),5,0xFF0000);
					};
				};
			};
		};
		draw_square(400,400,5,0xFFC0CB);	// draw avatar square pink
	} else {
		// local map
		for(j=5;j>=-5;j--){
			for(i=-5;i<=5;i++){
				if((-1 < P.x+i) && (P.x+i < 1000) && (-1 < P.y+j) && (P.y+j < 800)){	// in range?
					draw_geographical_square(i,j);

					h=world_map[P.x+i][P.y+j]*16;

					k=flora_fauna_map[P.x+i][P.y+j];
					if(k>=0 && k<100){
						draw_sprite(k,400+(i*tile_size),400-(j*tile_size),h);
					};
					if(k>=100 && k<200){
						draw_sprite(6,400+(i*tile_size),400-(j*tile_size),h);	// city; need to make these specific
					};
					if(k>=200 && k<300{
						draw_sprite(12,400+(i*tile_size),400-(j*tile_size),h);	// npc; need to make these specific			
					};
					if(i==0 && j==0){
						h=world_map[P.x][P.y]*16;
						if(world_map[P.x][P.y]>=4){
							h=world_map[P.x][P.y]*20;
						};
						if(P.embarked==false){
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

	erase_writing_field();
	
	T << "coordinates " << P.x << " " << P.y;
	p.x=1100;
	p.y=50;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "view mode " << view_mode;
	p.y=70;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "edit mode " << edit_mode;
	p.y=90;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "moves " << moves;
	p.y=110;
	draw_text(p,T,0xFFFFFF);	
	T.str("");
	T << "trees " << flora_fauna_count[0];
	p.y=130;
	draw_text(p,T,0xFFFFFF);	
	T.str("");
	T << "fruit trees " << flora_fauna_count[1];
	p.y=150;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "deer " << flora_fauna_count[2];
	p.y=170;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "fish " << flora_fauna_count[3];
	p.y=190;
	draw_text(p,T,0xFFFFFF);
	T.str("");
	T << "bears " << flora_fauna_count[4];
	p.y=210;
	draw_text(p,T,0xFFFFFF);
	
	T.str("");
	T << last_command;
	p.y=300;
	draw_text(p,T,0xFFAAAA);
};