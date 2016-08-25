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
    tMI.extractFrom(tdir);
    
    if(debug) {
        std::cout << "stackPlotter::moveDirHistsToStacks || metaInfo color=" << tMI.color << std::endl; 
        std::cout << "stackPlotter::moveDirHistsToStacks || metaInfo legendname=" << tMI.legendname<< std::endl; 
        std::cout << "stackPlotter::moveDirHistsToStacks || metaInfo legendorder=" << tMI.legendorder << std::endl; 
    }


    TIter    histIter(tdir->GetListOfKeys());
    TObject* cHistObj;
    TKey*    cHistKey;

    if(debug)
        std::cout << "stackPlotter::moveDirHistsToStacks || Iterating through histograms." << std::endl; 

    // loop through keys in the directory
    while((cHistKey = (TKey*) histIter())) {
        cHistObj=tdir->Get(cHistKey->GetName());
        if(!cHistObj->InheritsFrom(TH1::Class())) continue; 
    
        if(debug)
            std::cout << "stackPlotter::moveDirHistsToStacks || Found histogram " 
                      << cHistKey->GetName() << std::endl; 

        // prepare the histogram to be added to the stack
        TH1* cHist = (TH1*) cHistObj->Clone();
        cHist->SetDirectory(0);
        TString mapName = cHist->GetName();
            
        std::pair<Int_t,TH1*> newEntry(tMI.legendorder,cHist);

        // initialize the THStack if needed
        if(!stacks_[mapName]) {
            if(debug)
                std::cout << "stackPlotter::moveDirHistsToStacks || Initializing THStacks..." << std::endl; 
            THStack *stack = new THStack(mapName,mapName);
            stacks_[mapName]=stack;

            std::vector<std::pair<Int_t,TH1*> > legInfo(0);
            legInfo.push_back(newEntry);
            stacksLegEntries_[mapName] = legInfo;
        }
        
        cHist->SetFillColor(tMI.color);
        cHist->SetFillStyle(1001);
        cHist->SetMarkerStyle(kNone);
        cHist->SetMarkerColor(kBlack);
        cHist->SetLineColor(kBlack);
        cHist->SetTitle(tMI.legendname);
        cHist->SetName(tMI.legendname);
       
        // add plot to stack 
        stacks_[mapName]->Add(cHist,"HIST");

        std::vector<std::pair<Int_t,TH1*> > legEntries = stacksLegEntries_[mapName];
        if(debug)
            std::cout << "stackPlotter::moveDirHistsToStacks || legEntries size is " << legEntries.size() << std::endl; 
        for(size_t i=0; i < legEntries.size(); i++) {
            if(legEntries.at(i).second == cHist && legEntries.at(i).first == tMI.legendorder) break;

            if(legEntries.at(i).first >= tMI.legendorder) {
                if(debug)
                    std::cout << "stackPlotter::moveDirHistsToStacks || i is " << i << std::endl; 
                stacksLegEntries_[mapName].insert(stacksLegEntries_[mapName].begin()+i,newEntry);
                break;
            }

            if(i==legEntries.size()-1) {
                stacksLegEntries_[mapName].push_back(newEntry);
                break;
            }
        }

        if(debug)
            std::cout << "stackPlotter::moveDirHistsToStacks || legEntries size is " << legEntries.size() << std::endl; 
    }

}


void stackPlotter::plotStack(const TString& key) {
    if(debug)
        std::cout << "stackPlotter::plotStack" << std::endl; 

    TCanvas *c = new TCanvas(key,key,800,600);
    TLegend *leg = new TLegend(0.75,0.75,0.95,0.95);
    THStack *stack = stacks_[key];
    std::vector<std::pair<Int_t,TH1*> > legEntries = stacksLegEntries_[key];

    if(!stack) {
        std::cout << "ERROR (stackPlotter::plotStack): stack '" 
                  << key << "' does not exist!" << std::endl;
        exit(EXIT_FAILURE);
    }

    for(size_t i=0; i < legEntries.size(); i++) {
       TString legName = legEntries.at(i).second->GetName();
       if(legName == "") continue;
       leg->AddEntry(legEntries.at(i).second,legName,"F");
    }

    // draw plot and legend
    c->cd();
    stack->Draw();
    leg->Draw();

    // save and exit
    if(saveplots_) {
       c->SaveAs(outdir_+"/"+key+".pdf");
    }

    if(savecanvases_ && outfile_) {
       outfile_->cd();
       c->Write();
    }

}


void stackPlotter::plot() {
    if(debug)
        std::cout << "stackPlotter::plot" << std::endl; 

    TH1::AddDirectory(kFALSE);
    TDirectory::AddDirectory(kFALSE);
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
        TString writeOption = rewriteoutfile_ ? "RECREATE" : "UPDATE";
        outfile_ = new TFile(outdir_+"/plotter.root",writeOption);
    }

    // plot all the stacks & save appropriately
    if(debug)
        std::cout << "stackPlotter::plot || Plotting all the canvases" << std::endl; 
    for(const auto& it : stacks_) {
        plotStack(it.first);
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

    system((TString("mkdir -p ")+TString(argv[2])).Data());
    d_ana::stackPlotter sPlots;

    sPlots.rewriteOutfile(true);
    sPlots.savePlots(true);
    sPlots.saveCanvasRootFile(true);

    sPlots.setInputFile(argv[1]);
    sPlots.setOutDir(argv[2]);
    sPlots.setLumi(1);

    sPlots.plot();
}
