/* sprite.cc	operations for reading, clipping and displaying sprites */

void world::load_sprites(){
	int i;
	string S[MAX_SPRITE_NUM];
	char T[40];
	string R;
	GC local_gc;
	XImage *local_img;
	XImage *local_clp;

	for(i=0;i<MAX_SPRITE_NUM;i++){	// initialize
		S[i]="";
	};
	
	S[0]="tree.xpm";
	S[1]="fruit_tree.xpm";
	S[2]="palm_tree.xpm";
	S[3]="pine_tree.xpm";
	S[4]="sapling.xpm";
	S[5]="flowers.xpm";
	
	S[10]="cow.xpm";
	S[11]="boar.xpm";
	S[12]="goat.xpm";
	S[13]="deer.xpm";
	S[14]="fish.xpm";
	S[15]="dog.xpm";
	S[16]="horse.xpm";
	S[17]="sheep.xpm";
	
	S[20]="bear.xpm";
	S[21]="wolf.xpm";
	
//	S[30]="cambridge.xpm";
	S[31]="london.xpm";
	S[32]="amsterdam.xpm";
	S[33]="paris.xpm";
	S[34]="venice.xpm";
//	S[35]="rome.xpm";
//	S[36]="zurich.xpm";
//	S[37]="athens.xpm";
//	S[38]="constantinople.xpm";
//	S[39]="madrid.xpm";
//	S[40]="gibraltar.xpm";
	S[41]="castle.xpm";
	
	S[49]="scholar.xpm";
	S[50]="milkmaid.xpm";
	S[51]="merchant.xpm";
	S[52]="woodcutter.xpm";
	S[53]="robber.xpm";
	S[54]="knight.xpm";
	S[55]="cook.xpm";
	S[56]="monk.xpm";
	S[57]="musician.xpm";
	S[58]="farmer.xpm";
	S[59]="shepherd.xpm";
	
	S[60]="boat.xpm";
	S[61]="arrow.xpm";
	S[62]="crucifix.xpm";
	
	if(P.gender=="female"){
		S[99]="female_avatar.xpm";
	} else {
		S[99]="male_avatar.xpm";
	};

	for(i=0;i<MAX_SPRITE_NUM;i++){
		if(S[i]!=""){	// only read sprites for nonempty codes
			R="xpm_files/"+S[i];
			strcpy(T,R.c_str());
			if (XpmReadFileToImage(display, T, &local_img, &local_clp, NULL)) {
				cout << "problem reading image " << i << "\n";
				exit (1);
			} else {
				clp[i]=local_clp;
				img[i]=local_img;

				pix[i] = XCreatePixmap(display, win, (clp[i])->width, (clp[i])->height, (clp[i])->depth);
 				local_gc = XCreateGC (display, pix[i], 0, NULL);
				XPutImage(display, pix[i], local_gc, clp[i], 0, 0, 0, 0, clp[i]->width, clp[i]->height);
			};
		} else {
			img[i]=NULL;
		};
	};
};

void world::draw_sprite(int i, int x, int y, int h){	// sprite type i, sprite origin x,y height h
	XPoint p;
	if(img[i]!=NULL){
		p.x=x+17;	// need to work this out better
		p.y=y-51;
		p=affine_transform(p);
		p.y=p.y-h;
	    XSetClipMask(display, gc, pix[i]);
	    XSetClipOrigin(display, gc, p.x-(img[i]->width)/2, p.y-(img[i]->height)/2);
	    XPutImage(display, win, gc, img[i], 0, 0, p.x-(img[i]->width)/2, p.y-(img[i]->height)/2, img[i]->width, img[i]->height);
	    XSetClipMask(display, gc, None);
	};
};

