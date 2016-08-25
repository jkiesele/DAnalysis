/*
 * stackPlotter.cpp
 *
 *  Created on: 24 Aug 2016
 *      Author: eacoleman 
 */

#include "../interface/stackPlotter.h"

namespace d_ana{

stackPlotter::stackPlotter()
{}

stackPlotter::~stackPlotter(){
	//for(auto& it: stacks_) {
	//	if(it.second)
	//		delete it.second;
	//}
	
    //stacks_.clear();

    if(outfile_) {
        outfile_->Close();
        //delete outfile_;
    }
}


void stackPlotter::moveDirHistsToStacks(TDirectory* tdir){
    if(debug)
        std::cout << "stackPlotter::moveDirHistsToStacks" << std::endl; 
    
    // get metainfo from directory, else exit TODO
    metaInfo tMI;
    //tMI.extractFrom(tdir);

    TIter    histIter(tdir->GetListOfKeys());
    TObject* cHistObj;
    TKey*    cHistKey;

    // loop through keys in the directory
    while((cHistKey = (TKey*) histIter())) {
        cHistObj=tdir->Get(cHistKey->GetName());
        if(!cHistObj->InheritsFrom(TH1::Class())) continue; 

        // prepare the histogram to be added to the stack
        TH1* cHist = (TH1*) cHistObj->Clone();
        cHist->SetFillColor(tMI.color);
        cHist->SetMarkerColor(kNone);
        cHist->SetLineColor(kBlack);
        cHist->SetTitle(tMI.legendname);
        cHist->Scale(tMI.norm);

        // initialize the THStack if needed
        if(!stacks_[cHist->GetName()]) {
            THStack *stack = new THStack(cHist->GetName(),cHist->GetName());
            stacks_[cHist->GetName()]=stack;
        }
       
        // add plot to stack 
        stacks_[cHist->GetName()]->Add(cHist);
    }


}


void stackPlotter::plotStack(const TString& key) {
    if(debug)
        std::cout << "stackPlotter::plotStack" << std::endl; 

    TCanvas *c = new TCanvas(key,key,800,600);
    THStack *stack = stacks_[key];

    if(!stack) {
        std::cout << "ERROR (stackPlotter::plotStack): stack '" 
                  << key << "' does not exist!" << std::endl;
        exit(EXIT_FAILURE);
    }

    // draw plot and legend
    c->cd();
    stack->Draw();
    c->Modified();
    gPad->BuildLegend(0.75,0.75,0.95,0.95);

    // save and exit
    if(saveplots_) {
       c->SaveAs(key+".pdf");
    }

    if(savecanvases_ && outfile_) {
       outfile_->cd();
       c->Write();
    }

}


void stackPlotter::plot() {
    if(debug)
        std::cout << "stackPlotter::plot" << std::endl; 

    gROOT->SetBatch(true);
    TFile *fIn = new TFile(infile_,"READ");
    fIn->cd();
    
    if(debug)
        std::cout << "stackPlotter::plot || input file '" << infile_ << "' is being read..." << std::endl; 

    TIter   dirIter(fIn->GetListOfKeys());
    TObject *cDirObj;
    TKey    *key;

    // iterate over directories and get all stacks
    while((key = (TKey *) dirIter())) {
        cDirObj=fIn->Get(key->GetName());
        if(!cDirObj->InheritsFrom(TDirectory::Class())) continue; 
      
        TDirectory* cDir = (TDirectory*) cDirObj;
        if(debug)
            std::cout << "stackPlotter::plot || Moving histograms from directory " << cDir->GetName() 
                      << " to relevant maps." << std::endl;

        moveDirHistsToStacks(cDir);

        delete cDir;
    }
    
    if(debug)
        std::cout << "stackPlotter::plot || input file '" << infile_ << "' has been read. Closing..." << std::endl; 
    
    // intermediate cleanup
    fIn->Close();
    delete fIn;

    if(debug)
        std::cout << "stackPlotter::plot || Closed. Saving output..." << std::endl; 
    
    // create the outfile if need be
    if(savecanvases_) {
        if(debug)
            std::cout << "stackPlotter::plot || Opening output ROOT file" << std::endl; 
        TString writeOption = rewriteoutfile_ ? "REWRITE" : "UPDATE";
        outfile_ = new TFile(outdir_+"/plotter.root",writeOption);
    }

    // plot all the stacks & save appropriately
    if(debug)
        std::cout << "stackPlotter::plot || Plotting all the canvases" << std::endl; 
    for(const auto& it : stacks_) {
        //plotStack(it.first);
    }

    // close, save, and cleanup
    if(savecanvases_ && outfile_) {
        if(debug)
            std::cout << "stackPlotter::plot || Closing the outfile" << std::endl; 
        outfile_->Close();
    }
    
    if(debug)
        std::cout << "stackPlotter::plot || Done!" << std::endl; 
}

}



int main(int argc, const char** argv){

    if(argc < 3) {   
        std::cout << "***** stackPlotter ***************************************" << std::endl;
        std::cout << "**                                                       *" << std::endl;
        std::cout << "** Usage: ./stackPlotter <input file> <output directory> *" << std::endl;
        std::cout << "**                                                       *" << std::endl;
        std::cout << "**********************************************************\n\n" << std::endl;
        std::cout << "Incorrect usage: number of arguments is " << argc << std::endl;
        exit(EXIT_FAILURE);
    }

    d_ana::stackPlotter sPlots;

    sPlots.rewriteOutfile(true);
    sPlots.savePlots(false);
    sPlots.saveCanvasRootFile(false);

    sPlots.setInputFile(argv[1]);
    sPlots.setOutDir(argv[2]);
    sPlots.setLumi(1);

    sPlots.plot();
}
