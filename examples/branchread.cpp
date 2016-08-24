/*
 * exampleanalyser.cpp
 *
 *  Created on: 5 Aug 2016
 *      Author: jkiesele
 */



#include "../interface/basicAnalyzer.h"
#include "../interface/tTreeHandler.h"
#include "../interface/tBranchHandler.h"
#include "../interface/dBranchHandler.h"


/*
 *
 * In reality there should be Delphes class includes
 *
 */
#include "classes/DelphesClasses.h"


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




		/*
		 *
		 * The delphes trees have arrays instead of vectors, so there needs to be some
		 * king of explicit buffering. I did not realise this before. You can get ideas from
		 * https://github.com/jkiesele/TtZAnalysis/blob/master/Analysis/interface/wNTBaseInterface.h
		 *
		 * Something with dictionaries for the Delphes classes MIGHT be needed
		 * tTreeHandler does not work with TChains! This is due to a root limitation
		 * (at least from root 5). But it should not be a problem.
		 * Either hadd it or give it same legend name etc.
		 * For same legend name, output histogram files should just be added (taking into
		 * account the normalisation). This can be done in the writeOutput() function.
		 * This function is thread-safe. The file access is blocked while one process writes.
		 *
		 */

        // Get the DELPHES handlers
		d_ana::tTreeHandler tree(getSamplePath(),"Delphes");
		d_ana::dBranchHandler<Electron> elecs(&tree,"Electron");

        debug=true;

        // Add a histogram to the analysis
		TH1* histo=addPlot(new TH1D("histoname1","histotitle1",100,0,100));

        // Create tree in analysis (Default name is DAnalysis)
        TTree* anatree=addTree();

        // Add a branch to the tree
        Double_t elecPt=0;
        anatree->Branch("pte", &elecPt);

		std::cout << "event loop on " << getSampleFile()  <<std::endl;

		size_t nevents=1000;
		if(isTestMode())
			nevents=100;

		for(size_t i=0;i<nevents;i++){

			tree.setEntry(i); //associate event entry

			//std::cout << elecs.size() <<std::endl;
			if(elecs.size()>0) {
				histo->Fill(elecs.at(0)->PT);
                elecPt=elecs.at(0)->PT;   
            }
			reportStatus(i,nevents);
			//usleep(4e4);

			//	size_t njets=jets.content()->size(); //how to access the branch content
            anatree->Fill();
		}

		processEndFunction(); //needs to be called
	}



};


int main(){

	exampleanalyser ana;

	ana.setOutputFileName("testout.root");
	ana.setDataSetDirectory("/afs/cern.ch/user/j/jkiesele/eos/cms/store/group/upgrade/delphes_framework/");
	//adjust to what is needed. should aso be read in from file when we are done with testing

	ana.readFileList("exampleinput.txt");
	//	ana.setMaxChilds(1);

	//ana.setTestMode(true);//or not

	ana.start();

}

