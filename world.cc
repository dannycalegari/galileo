/* world.cc basic world class and declaration of functions */

#define MAX_SPRITE_NUM 16
#define MAX_SKILL_NUM 6
#define MAX_WALL_NUM 13

struct party_state{	// data file for party
	int x,y;	// global party location
	
	int food;	
	int health;
	int max_health;
	int gold; 
	int wood;	
	
	// skills
	bool skill_item[MAX_SKILL_NUM];		// has item to use skill
	int skill[MAX_SKILL_NUM];			// proficiency in skill; out of 100
};


struct item{
	int x,y;	// coordinates 
	int type;
};

struct npc{
	int x,y;	// coordinates 
	int id;		//
		// skills; out of 100
	int sword;
	int bow;
	int net;
	int farming;
	int axe;
		// attributes; out of 100
	int health;
	int food;
	int wood;
};

class world{
	public:
		// party data
		
		string last_command;
		string gender;
		int moves;
		bool edit_mode;
		bool view_mode;
		party_state P;
		point saved_coordinates;
		string map_name;
		bool in_city;
		bool in_combat;

		// world data
		
		vector<vector<int> > world_map;
		vector<vector<int> > flora_fauna_map;
		vector<vector<int> > wall_map;
		
		int flora_fauna_count[5];
		void count_flora_fauna();

		vector<item> items;
		vector<npc> npcs;
	
		void initialize();
		void save_state();
		void save_maps(string geo_map_name, string ff_map_name, string wall_map_name);

		void read_map(ifstream &input_file, int type);
		void write_map(ofstream &output_file, int type);
		void read_party(ifstream &input_file);
		void write_party(ofstream &output_file);
		
		void enter_city(string S);
		void exit_city();
		
		// geography and wall layers

		void draw_geographical_square(int i, int j);
		void draw_wall(int i, int j);
		void adjust_mountain_heights();
		
		// flora_fauna layer
		
		void clear_flora_fauna();
		void plant_trees();
		
		XImage *img[MAX_SPRITE_NUM];		// XImage for sprite
		XImage *clp[MAX_SPRITE_NUM];		// XImage clip for sprite
		Pixmap pix[MAX_SPRITE_NUM];		// clipping pixmap for sprite
		
		void load_sprites();
		void draw_sprite(int i, int x, int y, int h);
		
		point towards_avatar(int i, int j);
		void update_map();
		void draw();
		void draw_graphics();
		void draw_info();
		
		// game commands
		
		void select_direction_interface(int &select_direction_x, int &select_direction_y);
		void use_object(int x, int y);
		void talk(int x, int y);
		void attack(int x, int y);
		void exit_combat();
		
		void attempt_move(int x, int y);
		bool test_of_skill(int i);
		
		void user_interface();

};


void world::attempt_move(int x, int y){
	int X,Y;
	X=P.x+x;
	Y=P.y+y;

	if(X<1 || X> (int) world_map.size()-2 || Y<1 || Y> (int) world_map[0].size()-2){	
		// exit city or combat map; or if in europe,
		if(in_city){
			exit_city();
		} else if(in_combat){
			exit_combat();
		} else {
			last_command="out of range!";
		};
	} else if(edit_mode){
		P.x=X;
		P.y=Y;
	} else {
		if(world_map[X][Y]>=4){	// mountain
			if(world_map[X][Y]>=5){
				last_command="blocked";
			} else {
			// unless mountaineering skill and grapple
				if(test_of_skill(4)==true){	// if have grapple and pass mountaineering test
					P.x=X;
					P.y=Y;
				} else {
					last_command="stumble";
				};
			};
		} else if (world_map[X][Y]==0){		// water
			if(test_of_skill(0)==true){		// if embarked
	//			if(P.embarked==true){	// sailing on a boat
					//	test of sailing
				P.x=X;
				P.y=Y;
			} else {
				if(flora_fauna_map[X][Y]==13){	// if moving onto a boat
					P.x=X;
					P.y=Y;
				} else {	
					last_command="blocked";
				};
			};
		} else {	// ordinary move
			if(P.skill_item[0]==false){	// if not embarked
				P.x=X;
				P.y=Y;
			} else {
				last_command="blocked";
			};
		};
	};
};