/*
 * exampleanalyser.cpp
 *
 *  Created on: 5 Aug 2016
 *      Author: jkiesele
 */



#include "../interface/basicAnalyzer.h"
#include <iostream>

class exampleanalyser: public d_ana::basicAnalyzer{
public:
	exampleanalyser():d_ana::basicAnalyzer(){}
	~exampleanalyser(){}



private:
	/*
	 *
	 * the following have to be implemented here
	 *
	 */
	void analyze(size_t id){

		std::cout << "event loop on " << getSampleFile()  <<std::endl;

		if(id>0)
			sleep(2); //just to hve different status % in this example
		for(size_t i=0;i<1000;i++){
			reportStatus(i,1000);
			usleep(4e4);
		}

		processEndFunction();
	}



};


int main(){

	exampleanalyser ana;
	ana.setDataSetDirectory("");

	ana.readFileList("exampleinput.txt");
//	ana.setMaxChilds(1);

	ana.start();

}

