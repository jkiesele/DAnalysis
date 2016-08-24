/*
 * ANALYSER_TEMPL.cpp
 *
 *  Created on: 24 Aug 2016
 *      Author: jkiesele
 */

#include "interface/ANALYSER_TEMPL.h" //FIX


void ANALYSER_TEMPL::analyze(size_t childid){

	/*
	 * Load the tree in the input file.
	 */
	d_ana::tTreeHandler tree(getSamplePath(),"Delphes");

	/*
	 * Define the branches that are to be considered for the analysis
	 */
	d_ana::dBranchHandler<Electron> elecs(&tree,"Electron");


	/*
	 * Always use this function to add a new histogram
	 * Histograms created this way are automatically added to the output file
	 */
	TH1* histo=addPlot(new TH1D("histoname1","histotitle1",100,0,100));


	size_t nevents=tree.entries();
	for(size_t i=0;i<nevents;i++){
		/*
		 * The following lines report the status and set the entry
		 * Do not remove!
		 */
		reportStatus(i,nevents);
		tree.setEntry(i);


		/*
		 * Begin the event-by-event analysis
		 */
		if(elecs.size()>0)
			histo->Fill(elecs.at(0)->PT);



	}


	/*
	 * Must be called in the end
	 */
	processEndFunction();
}
