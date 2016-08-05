/*
 * exampleanalyser.cpp
 *
 *  Created on: 5 Aug 2016
 *      Author: jkiesele
 */



#include "../interface/basicAnalyzer.h"
#include "../interface/tTreeHandler.h"
#include "../interface/tBranchHandler.h"

/*
 *
 * In reality there should be Delphes class includes
 *
 */

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

		//open file, get some branches (commented since input files missing)
		//	d_ana::tTreeHandler tree(getSampleFile(),"treename");
		//	d_ana::tBranchHandler<std::vector<jet> > jets=d_ana::tBranchHandler<float>(&tree,"jets");


		std::cout << "event loop on " << getSampleFile()  <<std::endl;

		if(id>0)
			sleep(2); //just to hve different status % in this example
		for(size_t i=0;i<1000;i++){

			//	tree.setEntry(i); //associate event entry

			reportStatus(i,1000);
			usleep(4e4);

			//	size_t njets=jets.content()->size(); //how to access the branch content
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

