/* world.cc basic world class and declaration of functions */

struct party_state{	// data file for party
	int x,y;	// global party location
	string gender;	
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

struct conversation_item{
	string prompt;
	string reply;	
	// a reply has two components: a verbal part, and a nonverbal part; they are
	// separated by a semicolon. So for example, a response could be: hello!;smile
};

conversation_item new_conversation_item(string P, string R){
	conversation_item C;
	C.prompt=P;
	C.reply=R;
	return(C);
};

struct npc{
	int x,y;	// coordinates 
	int id;		//
	int cx,cy;	// center of gravity
	int hx,hy;	// heading direction
	int d;		// maximum L_1 distance from center of gravity
	int goal;	// sprite number of object npc wants to move towards; -1=none
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
	
	vector<conversation_item > talk_list;	// canned responses for conversation
};

struct monster{
	int x,y;	// coordinates
	int id;
	int health;
	bool ranged_attack;
	int strength;
	int dexterity;
};

struct balloon_speech{	// speech item that pops up near npc/fauna/monster on graphic screen
	int x,y;	// coordinates
	string S;
};

class world{
	public:
	
		// party data
		
		party_state P;

		// local data
		
		int moves;
		bool edit_mode;
		bool view_mode;
		point saved_coordinates;
		string map_name;
		bool in_city;

		// world data
		
		vector<vector<int> > world_map;			// geography layer
		vector<vector<int> > flora_fauna_map;	// generic animals/trees/npcs
		vector<vector<int> > wall_map;			// buildings
		vector<monster> monsters;				// monster roster (in combat)
		vector<npc> npcs;						// nongeneric npc roster
		int flora_fauna_count[MAX_SPRITE_NUM];	// inventory of flora/fauna layer

		// load/save functions; in load_save.cc
	
		void initialize();
		void enter_city(string S);
		void exit_city();
		void save_state();
		void save_current_map();
		void read_map(ifstream &input_file, int type);
		void write_map(ofstream &output_file, int type);
		void read_party(ifstream &input_file);
		void write_party(ofstream &output_file);
		
		// functions on map layers; in map.cc

		int count_geography(int x, int y, int range, int type);
		int count_flora_fauna(int x, int y, int range, int type);
		void plant_trees();	// should be folded into add_random_flora_fauna
		void adjust_mountain_heights();
		void clear_flora_fauna();
		void clear_wall();
		void add_random_building(int i, int j, int size);
		void populate_city_with_random_buildings();
		void add_random_flora_fauna_city(int i, int j);
		void populate_city_with_random_flora_fauna();
		void spawn_random_flora_fauna();	// not in combat or city

		// sprites; in sprite.cc
		
		XImage *img[MAX_SPRITE_NUM];		// XImage for sprite
		XImage *clp[MAX_SPRITE_NUM];		// XImage clip for sprite
		Pixmap pix[MAX_SPRITE_NUM];		// clipping pixmap for sprite
		
		void load_sprites();
		void draw_sprite(int i, int x, int y, int h);

		// functions to draw on screen; in draw.cc
		
		void draw_wall(int i, int j);	
		void draw_geographical_square(int i, int j);
		void draw_graphics();
		void draw_info();
		void draw_inventory();
		void draw();

		// communicating with message interface; in info.cc
		
		vector<balloon_speech > popup_message;
		vector<string > message;
		void add_new_message(string S);
		void add_popup_message(int i, int j, string S);
		void modify_last_line(string S);
		
		// conversation; in talk.cc
		
		string get_line_of_text();
		void talk(int x, int y);
		string get_response(int c, string S);		// response of npc[c] to string S
		void conversation_with_npc(int c);

		// functions to determine location, direction, pathfinding; direction.cc
		
		int occupied_by_special(int x, int y);		// also used in combat
		int special_in_direction(int x, int y);		// also used in combat	
		point towards_object(int i, int j, int type);	// find nearest object of type
		point towards_object(int i, int j, int type_lower, int type_upper);	// find object in type range
		point best_free_direction(int i, int j, point desired_move, int type);
		point fancy_best_free_direction(int i, int j, point desired_move, int type, int range);
		
		// movement; in move.cc

		bool can_move_into_square(int w, int x, int y);
		void attempt_move(int x, int y);
		
		// combat; in combat.cc

		bool in_combat;
		monster make_new_monster(int type, int x, int y);
		void attack(int x, int y);
		void enter_combat(int type);
		void exit_combat();
	//	void update_combat_map();

		// npc; in npc.cc
		
		npc make_new_npc(int type, int x, int y);	// returns new npc of desired kind and location
		void remove_npc(int i);
		int update_goal(int type, int goal);
		void achieve_goal(int l, int goal, point desired_move);

		// use object; in use.cc

		void use_object(int x, int y);
		bool test_of_skill(int i);
		
		// update state of world; in update.cc
		
		void update_map();
		
		// get keypress; in interface.cc

		void select_direction_interface(int &select_direction_x, int &select_direction_y);
		void user_interface();

};

