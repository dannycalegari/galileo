/*******************************************************
 * graphics.cc - sets up window stuff for main program *
 *                                                     *
 * uses standard Xlib stuff, because I'm too perverse  *
 * to use a GUI toolkit. the main reason is that       *
 * they keep upgrading to newer and newer libraries    *
 * which are not backward compatible, so that my       *
 * programs always start to break. boohoohoo.          *
 *                                                     *
 * Danny Calegari 12/17/2000                           *
 *******************************************************/

Display *display;
int screen_num;
unsigned int display_width, display_height;
XEvent report;
GC gc;
Window win;
int border_width = 4;
unsigned int width, height;
XFontStruct *font;

void setup_font(void){
    const char * fontname = "-*-georgia-*-r-*-*-14-*-*-*-*-*-*-*";
 //   const char * fontname = "-*-times-*-r-*-*-16-*-*-*-*-*-*-*";

    font = XLoadQueryFont (display, fontname);
    /* If the font could not be loaded, revert to the "fixed" font. */
    if (! font) {
        font = XLoadQueryFont (display, "fixed");
        cout << "couldn't find font!\n";
    };
    XSetFont (display, gc, font->fid);
};

void setup_graphics(void){
	display=XOpenDisplay(NULL);
	display_width = DisplayWidth(display, screen_num);
	display_height = DisplayHeight(display, screen_num);
	screen_num = DefaultScreen(display); 
	width = 1400;
	height = 800;
	win = XCreateSimpleWindow(display, RootWindow(display, screen_num), 0, 0, width, 
		height, border_width, WhitePixel(display, screen_num), BlackPixel(display, screen_num));
	XSelectInput(display, win, ExposureMask | KeyPressMask | ButtonPressMask | PointerMotionMask);
	gc = DefaultGC(display, screen_num);
	XSetForeground(display, gc, WhitePixel(display, screen_num));
	XSetBackground(display, gc, 0x000000);
//	XSetBackground(display, gc, BlackPixel(display, screen_num));
  	XMapWindow(display, win);
  	setup_font();
};

void erase_field(void){
	XClearWindow(display, win);
};

void erase_graphics_field(void){
	XClearArea(display, win, 0,0,1100,800,False);
}

void erase_writing_field(void){
	XClearArea(display, win, 1100,0,300,800,False);
}

point mouse_location(){
//    Bool result;
    Window window_returned;
    int root_x, root_y;
    int win_x, win_y;
    unsigned int mask_return;
    point p;
    
	XQueryPointer(display, win, &window_returned,
                &window_returned, &root_x, &root_y, &win_x, &win_y,
                &mask_return);
    p.x=win_x;
    p.y=win_y;
    return(p);
};

void draw_point(point p, long col){
    XSetForeground(display, gc, col);
	XDrawPoint(display, win, gc, p.x, p.y);
};

void draw_line(int x1, int y1, int x2, int y2, long col){
    XSetForeground(display, gc, col);
    XSetLineAttributes(display, gc, 2, LineSolid, 1, 1);
    XDrawLine(display, win, gc, x1, y1, x2, y2);
};

void draw_arc(point c, int r, int angle1, int angle2, long col){
    XSetForeground(display, gc, col);
    XSetLineAttributes(display, gc, 1, LineSolid, 1, 1);
    XSetFillStyle(display, gc, FillSolid);
    XDrawArc(display, win, gc, c.x-r, c.y-r, 2*r, 2*r, angle1, angle2);
};

void draw_faint_line(point p1, point p2, long col){
    XSetForeground(display, gc, (long) 0xDDDDDD);
	XSetLineAttributes(display, gc, 1, LineOnOffDash, 1, 1);
    XDrawLine(display, win, gc, p1.x, p1.y, p2.x, p2.y);
};

void erase_circle(point p, int r){
	XSetForeground(display, gc, 0xFFFFFF);
    XSetLineAttributes(display, gc, 1, LineOnOffDash, 1, 1);
	XSetFillStyle(display, gc, FillSolid);
    XFillArc(display, win, gc, p.x-r, p.y-r, 2*r, 2*r, 0, 23040);
};

void draw_circle(point p, int r, long col){
    XSetForeground(display, gc, col);
    XSetLineAttributes(display, gc, 1, LineSolid, 1, 1);
    XSetFillStyle(display, gc, FillSolid);
    XDrawArc(display, win, gc, p.x-r, p.y-r, 2*r, 2*r, 0, 23040);
};

void draw_dot(int x, int y, long col){
    XSetForeground(display, gc, col);
    XSetLineAttributes(display, gc, 1, LineSolid, 1, 1);
    XSetFillStyle(display, gc, FillSolid);
    XDrawArc(display, win, gc, x-1, y-1, 2, 2, 0, 23040);
};

void draw_letter(point p, char C, long col){
	string S;
	XSetForeground(display, gc, col);
	S=S+C;
	XDrawString(display,win,gc,p.x,p.y,S.c_str(),strlen(S.c_str()));
};

void draw_text(point &p, string S, long col){
    string U,V;
    XSetForeground(display, gc, col);
    if(S.size()<MAX_TEXT_WIDTH){
		XDrawString(display,win,gc,p.x,p.y,S.c_str(),strlen(S.c_str()));
    } else {
    	U=S.substr(0,MAX_TEXT_WIDTH);
    	V=S.substr(MAX_TEXT_WIDTH,S.size()-MAX_TEXT_WIDTH);
		XDrawString(display,win,gc,p.x,p.y,U.c_str(),strlen(U.c_str()));
		p.y=p.y+20;
		draw_text(p,V,col);
    };
};

void draw_square(int i, int j, int w, long col){
	XSetForeground(display, gc, col);
	XFillRectangle(display,win,gc,i,j,w,w);
};

XPoint affine_transform(XPoint q){	// skew map coordinates for isometric display
	double theta;
	XPoint p;
	
	theta = 0.3;
	p.x = q.x*cos(theta) + q.y*sin(theta);
	p.x = ((p.x-400.0)*1.1)+450.0;
	p.y = q.y*cos(theta) - q.x*sin(theta);
	p.y = ((p.y-400.0)*0.7)+510.0;
	return(p);
};
