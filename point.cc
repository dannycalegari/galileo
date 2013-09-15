/* point.cc		struct definition of point and operations on points */

struct point{ 
	int x,y; 
};

// functions to translate between point and XPoint

XPoint point_to_Xpoint(point p){	
	XPoint q;
	q.x=p.x;
	q.y=p.y;
	return(q);
};

point Xpoint_to_point(XPoint q){
	point p;
	p.x=q.x;
	p.y=q.y;
	return(p);
};

 // create new points

point new_point(int x, int y){
	point p;
	p.x=x;
	p.y=y;
	return(p);
};

point rand_point(){
	// returns one of NSEW or (0,0) with equal probability
	switch(rand()%5){
		case 0:
			return(new_point(1,0));
			break;
		case 1:
			return(new_point(-1,0));
			break;
		case 2:
			return(new_point(0,1));
			break;
		case 3:
			return(new_point(0,-1));
			break;
		default:
			break;
	};
	return(new_point(0,0));	// shouldn't get to this point
};

 // arithmetic functions on points
 
int norm(point p){	// L^1 norm
	return(abs(p.x)+abs(p.y));
};

int l2norm(point p){	// L^2 norm
	return((p.x*p.x)+(p.y*p.y));
};

int norm(int x, int y){
	return(abs(x)+abs(y));
};

point operator+(point p, point q){
	point r;
	r.x=p.x+q.x;
	r.y=p.y+q.y;
	return(r);
};

point operator-(point p, point q){
	point r;
	r.x=p.x-q.x;
	r.y=p.y-q.y;
	return(r);
};

point operator-(point p){
	point r;
	r.x=-p.x;
	r.y=-p.y;
	return(r);
};

point sign(point p){
	point q;
	if(p.x>0){
		q.x=1;
	} else if(p.x<0){
		q.x=-1;
	} else {
		q.x=0;
	};
	if(p.y>0){
		q.y=1;
	} else if(p.y<0){
		q.y=-1;
	} else {
		q.y=0;
	};
	return(q);
};