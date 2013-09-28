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

int world::off_height(int i, int j, int a, int b){	// a is from -1 to 1, b from -1 to 1
	int h;
	if(is_in_range(i,j,0)==false){
		h=0;
	} else {
		h=world_map[i][j]+world_map[i+a][j]+world_map[i][j+b]+world_map[i+a][j+b];
		if(a==0 && b==0 && world_map[i][j]>=4){
			h=world_map[i][j]*5;
		};
	};
	h=h*4;
	return(h);
};

XPoint world::grid_location(int i, int j, int x, int y, int a, int b){
	// location on the screen of grid point.
	// i,j is from -5 to 5
	// x,y is absolute location of i=0,j=0
	// a,b is from 0 to 2 grid (subdivision of i,j square)
	
	XPoint q;
	q.x=365+(i*70)+(a*35);
	q.y=435-(j*70)-(b*35);
	q=affine_transform(q);
	q.y=q.y-off_height(x+i,y+j,a-1,b-1);
	return(q);
};

long shadow(long c, double e){	// darkens color c by factor e in [0,1]
	int r,g,b;
	long d;
	b = c%256;
	g = (c/256)%256;
	r = (c/(256*256))%256;
	
	r=r*e;	// e=0.7;
	g=g*e;
	b=b*e;
	d=(256*256*r)+(256*g)+b;
	return(d);
};

void world::draw_wall(int i, int j){	
	// i,j in [-5,5] is location relative to P.x,P.y
	int k,x,y;
	int n,m,mm,l;
	long wall_code, temp_wall_code;
	long carpet_code[6];
	int wall_height;
	long carpet_color[6];
	long color;
	int wall_steps;
	bool roof;
	
	XPoint q[20];	// maximum number of vertices of polygon
	XPoint r;
	if(in_combat==true){
		x=6;
		y=6;
	} else {
		x=P.x;
		y=P.y;
	};
	k=wall_map[x+i][y+j];	// k is type of wall
	for(n=0;n<6;n++){		// no carpet by default
		carpet_code[n]=9;
	};
//	carpet_color[0]=0xAAAAAA;	// gray floor default
//	carpet_code[0]=903678521;
	wall_height=50;
	roof=false;
	wall_code=90;	// no wall by default
	
	switch(k){
		case 0:		// EW wall
			wall_code=9741;
			break;
		case 1:		// NS wall
			wall_code=9543;
			break;
		case 2:		// corner
			wall_code=913751;
			wall_height=80;
			break;
		case 3:		// NE diagonal
			wall_code=9840;
			break;
		case 4:		// NW diagonal
			wall_code=9642;
			break;
		case 5:		// NE W
			wall_code=9841;
			break;		
		case 6:		// NE S
			wall_code=9348;
			break;			
		case 7:		// NW E
			wall_code=9247;
			break;	
		case 8:		// NW S
			wall_code=9243;
			break;	
		case 9:		// SE W
			wall_code=9641;
			break;
		case 10:	// SE N
			wall_code=9645;
			break;
		case 11:	// SW E
			wall_code=9047;
			break;		
		case 12:	// SW N
			wall_code=9045;
			break;
		case 20:	// tower
			wall_code=9210367852;
			wall_height=180;
			roof=true;
			break;
			
				// floor codes > 20
		case 23:
			carpet_color[0]=0xAAAAAA;	// red checker
			carpet_color[1]=0xFF0000;
			carpet_code[0]=903678521;
			carpet_code[1]=903478541;
			break;
		case 24:	
			carpet_color[0]=0xAAAAAA;	// 0xBC8F8F;
			carpet_color[1]=0x800080;	// purple spiral
			carpet_code[0]=903678521;
			carpet_code[1]=9478452410436;
			break;
		case 25:
			carpet_color[0]=0xAAAAFF;
			carpet_color[1]=0xFFD700;	// gold and chiffon stripes
			carpet_code[0]=903678521;
			carpet_code[1]=96730485103;
			break;
		case 26:
			carpet_color[0]=0xAAAAAA;	// mosaic tile 1
			carpet_color[1]=0xFF0000;
			carpet_code[0]=903678521;
			carpet_code[1]=9145763;
			break;
		case 27:
			carpet_color[0]=0xAAAAAA;	// mosaic tile 2
			carpet_color[1]=0xFF0000;
			carpet_code[0]=903678521;
			carpet_code[1]=91248743;
			break;
		case 28:
			carpet_color[0]=0xAAAAAA;	// mosaic tile 3
			carpet_color[1]=0xFF0000;
			carpet_code[0]=903678521;
			carpet_code[1]=901254764;
			break;
		case 29:
			carpet_color[0]=0xAAAAAA;	// mosaic tile 4
			carpet_color[1]=0xFF0000;
			carpet_code[0]=903678521;
			carpet_code[1]=92574034;
			break;
		case 30:
			carpet_color[0]=0xBBBBBB;	// fancy tile
			carpet_code[0]=903678521;
			carpet_color[1]=0x43362D;	// dark brown
			carpet_code[1]=9150;
			carpet_color[2]=0x915C18;	// gold/brown
			carpet_code[2]=9370;
			carpet_color[3]=0x2F4A43;	// dark green
			carpet_code[3]=98570;
			carpet_color[4]=0x5875B9;	// light blue
			carpet_code[4]=98750;
			break;
		case 31:
			carpet_color[0]=0xBBBBBB;	// fancy tile
			carpet_code[0]=903678521;
			carpet_color[1]=0x43362D;	// dark brown
			carpet_code[1]=9316;
			carpet_color[2]=0x915C18;	// gold/brown
			carpet_code[2]=9756;
			carpet_color[3]=0x2F4A43;	// dark green
			carpet_code[3]=92156;
			carpet_color[4]=0x5875B9;	// light blue
			carpet_code[4]=92516;
			break;
		case 32:
			carpet_color[0]=0xBBBBBB;	// fancy tile
			carpet_code[0]=903678521;
			carpet_color[1]=0x43362D;	// dark brown
			carpet_code[1]=9738;
			carpet_color[2]=0x915C18;	// gold/brown
			carpet_code[2]=9518;
			carpet_color[3]=0x2F4A43;	// dark green
			carpet_code[3]=90318;
			carpet_color[4]=0x5875B9;	// light blue
			carpet_code[4]=90138;
			break;
		case 33:
			carpet_color[0]=0xBBBBBB;	// fancy tile
			carpet_code[0]=903678521;
			carpet_color[1]=0x43362D;	// dark brown
			carpet_code[1]=9572;
			carpet_color[2]=0x915C18;	// gold/brown
			carpet_code[2]=9132;
			carpet_color[3]=0x2F4A43;	// dark green
			carpet_code[3]=96732;
			carpet_color[4]=0x5875B9;	// light blue
			carpet_code[4]=96372;
			break;
		case 34:
			carpet_color[0]=0xBBBBBB;	// fancy tile
			carpet_code[0]=903678521;
			carpet_color[1]=0x2F4A43;
			carpet_code[1]=956183270;
			break;
		case 35:
			carpet_color[0]=0xBBBBBB;	// fancy tile
			carpet_code[0]=903678521;
			carpet_color[1]=0x43362D;	// dark brown
			carpet_code[1]=9143;
			carpet_color[2]=0x915C18;	// gold/brown
			carpet_code[2]=9743;
			carpet_color[3]=0x2F4A43;	// dark green
			carpet_code[3]=9745;
			carpet_color[4]=0x5875B9;	// light blue
			carpet_code[4]=9145;
			break;
		default:
			carpet_code[0]=9;	// no carpet
			carpet_code[1]=9;
			wall_code=98;
			break;
	};
	
	// draw carpet
	l=0;
	while(1){
		if(carpet_code[l]==9){
			break;
		} else {
			n=0;
			while(carpet_code[l]!=9){	
				m=carpet_code[l]%10;
				q[n]=grid_location(i, j, x, y, m/3, m%3);
				carpet_code[l]=carpet_code[l]/10;
				n++;
			};
			XSetForeground(display, gc, carpet_color[l]);
			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
			XFillPolygon(display, win, gc, q, n, Complex, CoordModeOrigin);
			l++;
		};
	};
	
	// draw wall
	temp_wall_code=wall_code;
	while(temp_wall_code>=99){
		m=temp_wall_code%10;
		mm=(temp_wall_code/10)%10;
		q[0]=grid_location(i,j,x,y,m/3,m%3);
		q[1]=grid_location(i,j,x,y,mm/3,mm%3);
		q[2]=q[1];
		q[2].y=q[2].y-wall_height;
		r=q[0];
		r.y=r.y-wall_height;
		
		if(battlement_style==0){
			q[3].x=(int) q[2].x*0.6666666+r.x*0.3333334;	// embattled; should be customizable
			q[3].y=(int) q[2].y*0.6666666+r.y*0.3333334;	
			q[6].x=(int) q[2].x*0.3333334+r.x*0.6666666;
			q[6].y=(int) q[2].y*0.3333334+r.y*0.6666666;
			q[4].x=q[3].x;
			q[4].y=q[3].y+20;
			q[5].x=q[6].x;
			q[5].y=q[6].y+20;
			q[7]=r;
			wall_steps=8;
		} else if(battlement_style==1){
			q[3].x=(int) q[2].x*0.6+r.x*0.4;	// dovetailed
			q[3].y=(int) q[2].y*0.6+r.y*0.4;
			q[6].x=(int) q[2].x*0.4+r.x*0.6;
			q[6].y=(int) q[2].y*0.4+r.y*0.6;
			q[4].x=(int) q[2].x*0.8+r.x*0.2;
			q[4].y=(int) q[2].y*0.8+r.y*0.2+20;
			q[5].x=(int) q[2].x*0.2+r.x*0.8;
			q[5].y=(int) q[2].y*0.2+r.y*0.8+20;
			q[7]=r;
			wall_steps=8;
		} else if(battlement_style==2){
			q[3].x=(int) q[2].x*0.86666666+r.x*0.133333334;	// wavy
			q[3].y=(int) q[2].y*0.86666666+r.y*0.133333334;
			q[4].x=(int) q[2].x*0.6666666+r.x*0.3333334;	
			q[4].y=(int) q[2].y*0.6666666+r.y*0.3333334-10;	
			q[5].x=(int) q[2].x*0.3333334+r.x*0.6666666;	
			q[5].y=(int) q[2].y*0.3333334+r.y*0.6666666+10;	
			q[6].x=(int) q[2].x*0.133333334+r.x*0.86666666;
			q[6].y=(int) q[2].y*0.133333334+r.y*0.86666666;
			q[7]=r;
			wall_steps=8;
		} else if(battlement_style==3){		// stairsteps
			q[3].x=(int) q[2].x*0.8+r.x*0.2;
			q[3].y=(int) q[2].y*0.8+r.y*0.2;
			q[4]=q[3];
			q[4].y=q[4].y-10;
			q[5].x=(int) q[2].x*0.6+r.x*0.4;
			q[5].y=(int) q[2].y*0.6+r.y*0.4-10;
			q[6]=q[5];
			q[6].y=q[6].y-10;
			q[7].x=(int) q[2].x*0.4+r.x*0.6;
			q[7].y=(int) q[2].y*0.4+r.y*0.6-20;
			q[8]=q[7];
			q[8].y=q[8].y+10;
			q[9].x=(int) q[2].x*0.2+r.x*0.8;
			q[9].y=(int) q[2].y*0.2+r.y*0.8-10;
			q[10]=q[9];
			q[10].y=q[10].y+10;
			q[11]=r;
			wall_steps=12;
		} else if(battlement_style==4){		// window
			q[3].x=(int) q[2].x*0.5+r.x*0.5;
			q[3].y=(int) q[2].y*0.5+r.y*0.5;
			q[4]=q[3];
			q[4].y=q[4].y+10;
			q[5].x=(int) q[2].x*0.7+r.x*0.3;
			q[5].y=(int) q[2].y*0.7+r.y*0.3+10;
			q[6]=q[5];
			q[6].y=q[6].y+20;
			q[7].x=(int) q[2].x*0.3+r.x*0.7;
			q[7].y=(int) q[2].y*0.3+r.y*0.7+30;
			q[8]=q[7];
			q[8].y=q[8].y-20;
			q[9]=q[4];
			q[10]=q[3];
			q[11]=r;
			wall_steps=12;
		} else if(battlement_style==5){		// columns
		
		};

		switch(abs(m-mm)){ 	// 3 for horizontal, 1 for vertical, 4 for NE, 2 for NW
			case 1:
				color=shadow(wall_color,0.7);
				break;
			case 2:
				color=shadow(wall_color,0.9);
				break;
			case 3:
				color=shadow(wall_color,1.0);
				break;
			case 4:
				color=shadow(wall_color,0.6);
				break;
			default:
				break;
		};
		XSetForeground(display, gc, color);
		XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
		XFillPolygon(display, win, gc, q, wall_steps, Nonconvex, CoordModeOrigin);
		XSetForeground(display, gc, trim_color);
		q[wall_steps]=q[0];
		XDrawLines(display, win, gc, q, wall_steps+1, CoordModeOrigin);	
		temp_wall_code=temp_wall_code/10;
	};
	
	// draw roof
	if(roof){
		while(wall_code>=99){
			m=wall_code%10;
			mm=(wall_code/10)%10;
			q[0]=grid_location(i,j,x,y,m/3,m%3);
			q[1]=grid_location(i,j,x,y,1,1);
			q[2]=grid_location(i,j,x,y,mm/3,mm%3);
			q[0].y=q[0].y-wall_height;
			q[1].y=q[1].y-wall_height-50;
			q[2].y=q[2].y-wall_height;
		
			switch(abs(m-mm)){ 	// 3 for horizontal, 1 for vertical, 4 for NE, 2 for NW
				case 1:
					color=shadow(0xE3674C,0.3);
					break;
				case 2:
					color=shadow(0xE3674C,0.3);
					break;
				case 3:
					color=shadow(0xE3674C,0.8);
					break;
				case 4:
					color=shadow(0xE3674C,0.4);
					break;
				default:
					break;
			};
			XSetForeground(display, gc, color);
			XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
			XFillPolygon(display, win, gc, q, 3, Nonconvex, CoordModeOrigin);
			XSetForeground(display, gc, trim_color);
			q[3]=q[0];
			XDrawLines(display, win, gc, q, 4, CoordModeOrigin);	
			wall_code=wall_code/10;
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

void world::draw_geographical_square(int i, int j){	// i,j in [-5,5] is location relative to x,y
	int a,b;
	int x,y;
	long c;
	XPoint q[5];
	
	if(in_combat==true){
		x=6;
		y=6;
	} else {
		x=P.x;
		y=P.y;
	};
	
	if(j==5){
		for(b=0;b<=2;b++){
			q[1+b]=grid_location(i,j,x,y,b,2);
		};
		q[0]=q[1];
		q[0].y=0;
		q[4]=q[3];
		q[4].y=0;
		XSetForeground(display, gc, 0x000000);
	   	XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    	XFillPolygon(display, win, gc, q, 5, Nonconvex, CoordModeOrigin);
	};
	
	if(i==5){
		for(b=0;b<=2;b++){
			q[1+b]=grid_location(i,j,x,y,2,b);
		};
		q[0]=q[1];
		q[0].y=0;
		q[4]=q[3];
		q[4].y=0;
		XSetForeground(display, gc, 0x000000);
	   	XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    	XFillPolygon(display, win, gc, q, 5, Nonconvex, CoordModeOrigin);	
	};
	
	if(j==-5){
		for(b=0;b<=2;b++){
			q[1+b]=grid_location(i,j,x,y,b,0);
		};
		q[0]=q[1];
		q[0].y=800;
		q[4]=q[3];
		q[4].y=800;
		XSetForeground(display, gc, 0x000000);
    	XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    	XFillPolygon(display, win, gc, q, 5, Nonconvex, CoordModeOrigin);
	};
	
	if(i==-5){
		for(b=0;b<=2;b++){
			q[1+b]=grid_location(i,j,x,y,0,b);
		};
		q[0]=q[1];
		q[0].y=800;
		q[4]=q[3];
		q[4].y=800;
		XSetForeground(display, gc, 0x000000);
	   	XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    	XFillPolygon(display, win, gc, q, 5, Nonconvex, CoordModeOrigin);	
	};

	for(b=2;b>=0;b=b-2){
		for(a=2;a>=0;a=a-2){
			q[0]=grid_location(i,j,x,y,1,1);
			q[1]=grid_location(i,j,x,y,a,1);
			q[2]=grid_location(i,j,x,y,1,b);
			c=determine_color(off_height(x+i,y+j,0,0)/4+off_height(x+i,y+j,a-1,0)/4+off_height(x+i,y+j,0,b-1)/4);
 			if(off_height(x+i,y+j,0,0)>=80){
 				if(a==2){
 					if(b==2){	// a=2, b=2
 						c=0xAAAAAA;
 					} else {	// a=2, b=0
 					   	c=0xCCCCCC;
 					};
 				} else {
 					if(b==2){	// a=0, b=2
 						c=0xDDDDDD;
 					} else {	// a=0, b=0
 					
 					};					
  				};
 			};
 			XSetForeground(display, gc, c);
    		XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    		XFillPolygon(display, win, gc, q, 3, Convex, CoordModeOrigin);	
			q[0]=grid_location(i,j,x,y,a,b);
			q[2]=grid_location(i,j,x,y,a,1);
			q[1]=grid_location(i,j,x,y,1,b);
			c=determine_color(off_height(x+i,y+j,a-1,b-1)/4+off_height(x+i,y+j,a-1,0)/4+off_height(x+i,y+j,0,b-1)/4);
 			XSetForeground(display, gc, c);
    		XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    		XFillPolygon(display, win, gc, q, 3, Convex, CoordModeOrigin);	
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

bool world::is_visible(int i, int j){	// is absolute location i,j visible from P.x,P.y?
	bool visible;
	point p;
	if(is_in_range(i,j,0)==false){
		visible=false;
	} else {
		visible=true;	// default unless obscured
		if(i==P.x && j==P.y){	// can see self!
			return(visible);
		};
		p=new_point(i,j)+sign(new_point(P.x,P.y)-new_point(i,j));	// step in direction

		while(norm(p-new_point(P.x,P.y))>0){
			if(flora_fauna_map[p.x][p.y]>=0 && flora_fauna_map[p.x][p.y]<4){	// tree in the way
				visible=false;
			};
			if(0<(int) wall_map.size()){	// wall map
				if(wall_map[p.x][p.y]>-1){	// wall in the way
					visible=false;
				};
			};
			p=p+sign(new_point(P.x,P.y)-p);	// step in direction
		};
	};
	return(visible);
};

void world::draw_graphics(){
	int i,j,k;
	int h;
	int x,y;
	XPoint p;
	point q;
	
	int tile_size;
	tile_size=70;	// should make this some global variable?

//	erase_graphics_field();
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
					if(k>=100 && k<111){
						draw_sprite(k-70,400+(i*tile_size),400-(j*tile_size),h); // specific city
					};
					if(k>=111 && k<200){
						draw_sprite(41,400+(i*tile_size),400-(j*tile_size),h);	// generic city; need to make these specific
					};
					if(k>=200 && k<300){
						draw_sprite(12,400+(i*tile_size),400-(j*tile_size),h);	// npc; need to make these specific			
					};
					if(in_combat==true){
						if(occupied_by_special(i+6,j+6)>-1){	// draw monsters
							k=monsters[occupied_by_special(i+6,j+6)].sprite;
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
							k=npcs[occupied_by_special(i+x,j+y)].sprite;
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