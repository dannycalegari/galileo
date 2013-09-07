/* sprite.cc	operations for reading, clipping and displaying sprites */

void world::load_sprites(){
	int i;
	string S[MAX_SPRITE_NUM];
	char T[40];
	string R;
	GC local_gc;
	XImage *local_img;
	XImage *local_clp;

	S[0]="tree.xpm";
	S[1]="fruit_tree.xpm";
	S[2]="deer.xpm";
	S[3]="fish.xpm";
	S[4]="bear.xpm";
	if(gender=="female"){
		S[5]="female_avatar.xpm";
	} else {
		S[5]="male_avatar.xpm";
	};
	S[6]="castle.xpm";	// generic castle sprite
	S[7]="cow.xpm";
	S[8]="farmer.xpm";
	S[9]="robber.xpm";
	S[10]="merchant.xpm";
	S[11]="lumberjack.xpm";
	S[12]="npc.xpm";	// generic npc sprite
	S[13]="boat.xpm";
	S[14]="paris.xpm";
	S[15]="london.xpm";
	
	for(i=0;i<16;i++){
		R="xpm_files/"+S[i];
		strcpy(T,R.c_str());
		if (XpmReadFileToImage(display, T, &local_img, &local_clp, NULL)) {
			cout << "problem reading image " << i << "\n";
			exit (1);
		} else {
		};
		clp[i]=local_clp;
		img[i]=local_img;

		pix[i] = XCreatePixmap(display, win, (clp[i])->width, (clp[i])->height, (clp[i])->depth);
 		local_gc = XCreateGC (display, pix[i], 0, NULL);
		XPutImage(display, pix[i], local_gc, clp[i], 0, 0, 0, 0, clp[i]->width, clp[i]->height);
	};
};

void world::draw_sprite(int i, int x, int y, int h){	// sprite type i, sprite origin x,y height h
	XPoint p;
	p.x=x;
	p.y=y-35;
	p=affine_transform(p);
	p.y=p.y-h;
    XSetClipMask(display, gc, pix[i]);
    XSetClipOrigin(display, gc, p.x-(img[i]->width)/2, p.y-(img[i]->height)/2);
    XPutImage(display, win, gc, img[i], 0, 0, p.x-(img[i]->width)/2, p.y-(img[i]->height)/2, img[i]->width, img[i]->height);
    XSetClipMask(display, gc, None);
};

