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
			last_command="success";
			return(true);
		} else {
			last_command="failed";
			return(false);
		};
	} else {
		if(i==0 || i==4){	// attempting to sail or mountain climb
			last_command="blocked";
		} else {
			last_command="how?";
		};
		return(false);
	};
};