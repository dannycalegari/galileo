/* world.cc basic world class and declaration of functions */

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

struct conversation_item{
	string prompt;
	string reply;	
	// a reply has two components: a verbal part, and a nonverbal part; they are
	// separated by a semicolon. So for example, a response could be: hello!;smile
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
		
		string gender;
		int moves;
		bool edit_mode;
		bool view_mode;
		party_state P;
		point saved_coordinates;
		string map_name;
		bool in_city;

		// world data
		
		vector<vector<int> > world_map;
		vector<vector<int> > flora_fauna_map;
		vector<vector<int> > wall_map;
		
		int flora_fauna_count[MAX_SPRITE_NUM];
		void count_flora_fauna();

		vector<monster> monsters;
		vector<npc> npcs;
	
		void initialize();
		void save_state();
		void save_current_map();
//		void save_maps(string geo_map_name, string ff_map_name, string wall_map_name);

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

		void update_map();
		void draw();
		void draw_graphics();
		
		// info
		
		vector<balloon_speech > popup_message;
		vector<string > message;
		void add_new_message(string S);
		void modify_last_line(string S);
		void add_popup_message(int i, int j, string S);
		string get_line_of_text();
		void draw_info();
		
		// game commands
		
		void select_direction_interface(int &select_direction_x, int &select_direction_y);
		void use_object(int x, int y);
		void talk(int x, int y);
		
		// combat

		bool in_combat;
		void attack(int x, int y);
		void enter_combat(int type);
		void exit_combat();
		void update_combat_map();
		monster make_new_monster(int type, int x, int y);

		// npc
		
		int occupied_by_special(int x, int y);		// also used in combat
		int special_in_direction(int x, int y);		// also used in combat
		void conversation_with_npc(int c);
		string get_response(int c, string S);		// response of npc[c] to string S
		npc make_new_npc(int type, int x, int y);


		// move
		
		void attempt_move(int x, int y);
		bool can_move_into_square(int w, int x, int y);
		point best_free_direction(int i, int j, point desired_move, int type);
		point towards_object(int i, int j, int type);
		
		bool test_of_skill(int i);
		
		void user_interface();

};

