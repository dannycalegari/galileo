/* skill.cc		function to test and train skills */

/*
	index			skill				item				failure
	
	0				sailing				embarked			stalled
	1				woodcutting			axe					mishit
	2				hunting				bow					missed
	3				fishing				net					missed
	4				climbing			grapple				stumbled
	5				milking				bucket				spilled
*/

bool world::test_of_skill(int i){	// attempt skill index i
	if(P.skill_item[i]==true){	// do you own the requisite item?
		if(rand()%100 < P.skill[i]){
			if(rand()%1000 > 900+P.skill[i]){	// attempt to improve
				P.skill[i]++;
			};
		//	add_new_message("success");
			return(true);
		} else {
		//	add_new_message("failed");
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