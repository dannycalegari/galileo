/* info.cc	info handling */

void world::add_new_message(string S){
	// MAX_MESSAGE_NUM is 5
	if(message.size() >= MAX_MESSAGE_NUM){
		message.erase(message.begin());	// erase first entry
	};
	message.push_back(S);
};

void world::modify_last_line(string S){
	int i;
	i=message.size();
	if(i>0){
		message[i-1]=S;
	} else {
		message.push_back(S);
	};
};