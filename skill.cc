/* skill.cc		function to test and train skills */

/*
	index			skill				item
	
	0				sailing				embarked
	1				woodcutting			axe
	2				hunting				bow
	3				fishing				net
	4				climbing			grapple
	5				milking				bucket
*/

bool world::test_of_skill(int i){	// attempt skill index i
	if(P.skill_item[i]==true){	// do you own the requisite item?
		if(rand()%100 < P.skill[i]){
			if(rand()%1000 > 900+P.skill[i]){	// attempt to improve
				P.skill[i]++;
			};
			add_new_message("success");
			return(true);
		} else {
			add_new_message("failed");
			return(false);
		};
	} else {
		if(i==0 || i==4){	// attempting to sail or mountain climb
			add_new_message("blocked");
		} else {
			add_new_message("how?");
		};
		return(false);
	};
};