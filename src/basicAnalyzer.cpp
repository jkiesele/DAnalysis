/*
 * basicAnalyzer.cc
 *
 *  Created on: 20 May 2016
 *      Author: kiesej
 */


#include "../interface/basicAnalyzer.h"
#include "../interface/fileReader.h"
#include "TH1.h"
#include "TFile.h"

#include "../interface/metaInfo.h"



namespace d_ana{

basicAnalyzer::basicAnalyzer():fileForker(),
		inputfile_(""),
		legendname_(""),
		col_(0),
		norm_(1),
		legorder_(0),
		signal_(false),
		lumi_(0),
		freplaced_(0),
		testmode_(false),
		isMC_(true),
		datalegend_("data")
{}

<<<<<<< HEAD
    ntuples_ = new TTree();
=======
basicAnalyzer::~basicAnalyzer(){
	for(auto& it: histos_) {
		if(it.second)
			delete it.second;
	}
	histos_.clear();

>>>>>>> f7cc3f5817d066c414ebc1c25925994f6508fe44
}

void basicAnalyzer::process(){
	size_t anaid=ownChildIndex();
	inputfile_=infiles_.at(anaid); //modified in some mode options
	legendname_=legentries_.at(anaid);
	col_=colz_.at(anaid);
	legorder_=legords_.at(anaid);
	signal_=issignal_.at(anaid);
	norm_=norms_.at(anaid);
	isMC_ = legendname_ != datalegend_;
	analyze(anaid);
}


void basicAnalyzer::readFileList(const std::string& inputfile){
	if(debug)
		std::cout << "basicAnalyzer::readFileList" << std::endl;
	using namespace d_ana;
	using namespace std;

	fileReader fr;
	fr.setDelimiter(",");
	fr.setComment("$");
	fr.setStartMarker("[inputfiles-begin]");
	fr.setEndMarker("[inputfiles-end]");
	fr.readFile(inputfile);

	infiles_.clear();
	legentries_.clear();
	colz_.clear();
	norms_.clear();
	legords_.clear();
	issignal_.clear();
	extraopts_.clear();
	std::vector<std::string> infiles;

	std::cout << "basicAnalyzer::readFileList: reading input file " << std::endl;

	for(size_t line=0;line<fr.nLines();line++){
		if(fr.nEntries(line) < 5){
			std::cout << "basicAnalyzer::readFileList: line " << line << " of inputfile is broken ("<<fr.nEntries(line)<< " entries.)" <<std::endl;
			sleep(2);
			continue;
		}
		infiles_.push_back   ((fr.getData<TString>(line,0)));
		if(debug)
			std::cout << "basicAnalyzer::readFileList: " << infiles_.at(infiles_.size()-1) << std::endl;
		legentries_.push_back(fr.getData<TString>(line,1));
		colz_.push_back      (fr.getData<int>    (line,2));
		norms_.push_back     (fr.getData<double> (line,3));
		legords_.push_back    (fr.getData<size_t> (line,4));
		if(fr.nEntries(line) > 5)
			issignal_.push_back(fr.getData<bool> (line,5));
		else
			issignal_.push_back(false);
		if(fr.nEntries(line) > 6)
			extraopts_.push_back(fr.getData<TString> (line,6));
		else
			extraopts_.push_back("");

	}
	std::vector<std::string > newinfiles;
	for(size_t i=0;i<infiles_.size();i++){
		//if(legentries_.at(i) == dataname_)
		//	continue;
		infiles_.at(i) =   replaceExtension(infiles_.at(i));
		///load pdf files
		newinfiles.push_back(infiles_.at(i).Data());

	}
	fileForker::setInputFiles(newinfiles);


}


TString basicAnalyzer::replaceExtension(TString filename){
	for(size_t i=0;i<ftorepl_.size();i++){
		if(filename.Contains(ftorepl_.at(i))){
			freplaced_++;
			return filename.ReplaceAll(ftorepl_.at(i),fwithfix_.at(i));
		}
	}
	return filename;
}

fileForker::fileforker_status basicAnalyzer::runParallels(int interval){
	prepareSpawn();
	fileForker::fileforker_status stat=fileForker::ff_status_parent_busy;
	int counter=0;
	interval*=4; //to make it roughly seconds

	double killthreshold=1800; //seconds. Kill after 30 Minutes without notice

	time_t now;
	time_t started;
	time(&started);
	time(&now);
	std::vector<double> lastAliveSignals(infiles_.size());
	std::vector<int>    lastBusyStatus(infiles_.size());
	while(stat==fileForker::ff_status_parent_busy || stat== fileForker::ff_status_parent_childstospawn){

		fileForker::fileforker_status writestat=spawnChildsAndUpdate();
		stat=getStatus();
		if(writestat == ff_status_parent_filewritten || (interval>0  && counter>interval)){
			time(&now);
			double runningseconds = fabs(difftime(started,now));
			std::cout << "PID    "<< textFormatter::fixLength("Filename",50)                << " statuscode " << " progress " <<std::endl;
			int totalstatus=0;
			for(size_t i=0;i<infiles_.size();i++){
				totalstatus+=getBusyStatus(i);
				double percentpersecond=getBusyStatus(i)/runningseconds;
				double estimate=(100-getBusyStatus(i))/percentpersecond;
				std::cout
				<< textFormatter::fixLength(textFormatter::toString(getChildPids().at(i)),7)
				<< textFormatter::fixLength(infiles_.at(i),         50)
				<< textFormatter::fixLength(translateStatus(getStatus(i)), 15)
				<< textFormatter::fixLength(textFormatter::toString(getBusyStatus(i))+"%",4,false);
				if(getBusyStatus(i)>4 && getStatus(i) == ff_status_child_busy){
					std::cout  <<" ETA: ";
					int minutes=estimate/60;
					std::cout << std::setw(2) << std::setfill('0')<< minutes << ":" <<
							std::setw(2) << std::setfill('0')<<(int)(estimate - minutes*60) ;
				}
				std::cout <<std::endl;
				if(lastBusyStatus.at(i)!=getBusyStatus(i) || getStatus(i) != ff_status_child_busy){
					lastBusyStatus.at(i)=getBusyStatus(i);
					lastAliveSignals.at(i)=runningseconds;
				}
				else if(getStatus(i) == ff_status_child_busy){
					if(runningseconds-lastAliveSignals.at(i) > killthreshold){
						//for a long time no signal, but "busy" something is very likely wrong
						std::cout << "-> seems to be hanging. Process will be killed." <<std::endl;
						abortChild(i);
					}
				}
			}
			std::cout << "total: "<< totalstatus / (100* infiles_.size()) << "%"<< std::endl;
			std::cout << std::endl;
			counter=0;
		}
		counter++;
		usleep (250e3);
	}
	std::cout << "End report:" <<std::endl;
	std::cout << textFormatter::fixLength("Filename",30)                << " statuscode " << " progress " <<std::endl;
	for(size_t i=0;i<infiles_.size();i++)
		std::cout << textFormatter::fixLength(infiles_.at(i).Data(),50) << "     " << textFormatter::fixLength(translateStatus(getStatus(i)),15) <<"     " << "   " << getBusyStatus(i)<<"%"<<std::endl;
	std::cout << std::endl;


	//if no data, then create pseudodata
	if(std::find(legentries_.begin(),legentries_.end(),datalegend_) == legentries_.end()){
		///TBI!  FIXME
	}

	return stat;
}


void basicAnalyzer::setOutDir(const TString& dir){
	if(dir.EndsWith("/"))
		outdir_=dir;
	else
		outdir_=dir+"/";
}

<<<<<<< HEAD
TH1* basicAnalyzer::addPlot(TString name, TH1* histo, bool replace) {
    if(histo==0) {
        std::cout << "WARNING (basicAnalyzer::addPlot): input histogram '" 
                  << name << "' is a null pointer." << std::endl;
    }

    // TODO: format name according to some ruleset 
    TString formattedName=name;

    if(histos_[formattedName] && !replace) 
        histos_[formattedName]->Add(histo); 
    else { 
        histos_.erase(formattedName.Data());
        histos_[formattedName]=histo;
    }

    return histo;
}

TBranch* basicAnalyzer::addBranch(TString name, TString type, Long_t address) {
    // TODO: format name according to some ruleset 
    TString formattedName=name;
    TString formattedType=formattedName + "/" + type;

    if(address != 0) {
        return (TBranch*) ntuples_->Branch(formattedName,address,formattedType); 
    } else {
        std::cout << "ERROR (basicAnalyzer::addBranch): Null address while adding branch " 
                  << name << std::endl;
        return 0;
    }
=======
TH1* basicAnalyzer::addPlot(TH1* histo, bool replace) {
	// TODO: format name according to some ruleset
	TString formattedName=histo->GetName();
	histo->Sumw2();
	if(histos_[formattedName] && !replace)
		histos_[formattedName]->Add(histo);
	else {
		histos_.erase(formattedName.Data()); //Jan: mem leak?
		histos_[formattedName]=histo;
	}
	return histo;
>>>>>>> f7cc3f5817d066c414ebc1c25925994f6508fe44
}

void basicAnalyzer::rmvPlot(const TString& name) {
	histos_.erase(name.Data());
}

void basicAnalyzer::rmvPlots(const TRegexp& nameExp) {
	for(auto& it: histos_) {
		if(it.first.Contains(nameExp)) histos_.erase(it.first.Data());
	}
}

void basicAnalyzer::rmvBranch(TString name) {
    ntuples_->DropBranchFromCache(name);
}

void basicAnalyzer::rmvBranches(const TRegexp& nameExp) {
    TObjArray *branchList = (TObjArray*) ntuples_->GetListOfBranches();

    for(Int_t i=0; i < branchList->GetSize(); i++) {
        TString name = TString(branchList->At(i)->GetName());
        if(name.Contains(nameExp)) {
            ntuples_->DropBranchFromCache(name);
        }
    }
}

void basicAnalyzer::fillTree() {
	if(debug)
		std::cout << "basicAnalyzer::fillTree" <<std::endl;

    ntuples_->Fill();
}

void basicAnalyzer::fillPlot(TString name, Double_t value, Double_t weight) {
	if(debug)
		std::cout << "basicAnalyzer::fillPlot" <<std::endl;

    if(histos_[name] == 0) {
        std::cout << "ERROR (basicAnalyzer::fillPlot): histogram '" 
                  << name << "' does not exist." << std::endl;
    }

    histos_[name]->Fill(value,weight);
}

void basicAnalyzer::fillPlots(const TRegexp& nameExp, Double_t value, Double_t weight) {
	if(debug)
		std::cout << "basicAnalyzer::fillPlots" <<std::endl;

    for(auto& it : histos_) {
        if(it.first.Contains(nameExp) && it.second != 0) {
	        if(debug)
		        std::cout << "basicAnalyzer::fillPlots || filling " << it.first << std::endl;
            it.second->Fill(value,weight);
        }
    }
}

fileForker::fileforker_status basicAnalyzer::writeOutput(Bool_t recreate, Bool_t writeTree){
	if(debug)
		std::cout << "basicAnalyzer::writeHistos: " <<legendname_ <<std::endl;

	TFile *outFile = new TFile(getOutPath(), "UPDATE");

	TString tdirname=textFormatter::makeCompatibleFileName(legendname_.Data());
	tdirname="d_"+tdirname; //necessary otherwise problems with name "signal" in interactive root sessions


	bool exists=outFile->cd(tdirname);

	if(!exists){
		outFile->mkdir(tdirname);
		outFile->cd(tdirname);
	}


<<<<<<< HEAD
    TString openSetting = recreate ? "RECREATE" : "UPDATE";
    TFile *outFile = new TFile(getOutPath(), openSetting);
	if(debug)
	    std::cout << "basicAnalyzer::writeHistos || Opening TFile with setting " << openSetting << std::endl;

    try {
    	if(debug)
    	    std::cout << "basicAnalyzer::writeHistos || Writing histograms" <<std::endl;
        for(auto& it: histos_) {
            outFile->cd();
            it.second->Write();
        }
    } catch(Int_t e) { 
    	std::cout << "ERROR (basicAnalyzer::writeHistos): Problem while writing histograms to outfile" <<std::endl;
        return ff_status_child_aborted;
    }


    try {
        if(writeTree) {
	        if(debug)
	    	    std::cout << "basicAnalyzer::writeHistos || Writing ntuples" <<std::endl;
            outFile->cd();
            ntuples_->Write();
        }
    } catch(Int_t e) {
    	std::cout << "ERROR (basicAnalyzer::writeHistos): Problem while writing ntuples to outfile" <<std::endl;
        return ff_status_child_aborted; 
    }

    outFile->Close();
=======
	//write meta info
	metaInfo meta;
	meta.legendname=legendname_;
	meta.color=col_;
	meta.norm=norm_;
	meta.legendorder=legorder_;
	if(!exists)
		meta.Write();


	//here one would also write the ntuple
	for(auto& it: histos_) {
		//it.second->SetName( it.second->GetName() );
		it.second->Scale(norm_);
>>>>>>> f7cc3f5817d066c414ebc1c25925994f6508fe44


		if(!exists)
			it.second->Write();
		else{

			/*
			 * add to corresponding existing histogram TBI FIXME
			 */

		}
	}
	/*
	 * nor write the ntuple (if any)
	 * keep in mind: there will be a problem with the norms if they
	 * have the same legend name.... !
	 */


	outFile->Close();

	//clean-up

	delete outFile;


	return ff_status_child_success;
}


void basicAnalyzer::reportStatus(const Long64_t& entry,const Long64_t& nEntries){
	static Long64_t step=0;
	static Long64_t next=0;
	if(!step){
		step=nEntries/400;
	}
	if(entry==next || entry==nEntries-1){
		next+=step;
		int status=(entry+1) * 100 / nEntries;
		reportBusyStatus(status);
	}
}

void basicAnalyzer::setFilePostfixReplace(const TString& file,const TString& pf,bool clear){
	if(clear){ fwithfix_.clear();ftorepl_.clear();}
	ftorepl_.push_back(file); fwithfix_.push_back(pf);
}
void basicAnalyzer::setFilePostfixReplace(const std::vector<TString>& files,const std::vector<TString>& pf){
	if(files.size() != pf.size()){
		std::string errstr= "setFilePostfixReplace: vectors have to be same size!";
		throw std::logic_error(errstr);
	}
	ftorepl_=files;fwithfix_=pf;
}

bool basicAnalyzer::createOutFile()const{
	if(outdir_.Length())
		system(("mkdir -p "+outdir_).Data());
	TFile f(getOutPath(),"RECREATE");
	return true;
}


}
