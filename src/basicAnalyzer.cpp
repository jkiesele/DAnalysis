/*
 * basicAnalyzer.cc
 *
 *  Created on: 20 May 2016
 *      Author: kiesej
 */


#include "../interface/basicAnalyzer.h"
#include "../interface/fileReader.h"
#include "../interface/helpers.h"
#include "TH1.h"
#include "TFile.h"
#include<dirent.h>
#include<unistd.h>
#include <ctgmath>

#include "../interface/metaInfo.h"
#include "classes/DelphesClasses.h"


namespace d_ana{

//helper
class tempFile{
public:
	tempFile(){create();}
	~tempFile(){tempFile::close();}
	void create();
	std::string getName(){
		return namebuff_;
	}
	void close();

private:
	char namebuff_[32];
};

void tempFile::create(){
	memset(namebuff_,0,sizeof(namebuff_));
	strncpy(namebuff_,"/tmp/myTmpFile-XXXXXX",21);
	int d=mkstemp(namebuff_);
	if(d<1)
		throw std::runtime_error("tempFile::create: error");
}
void tempFile::close(){
	unlink(namebuff_);
	memset(namebuff_,0,sizeof(namebuff_));
}


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
		datalegend_("data"),
		treename_("Delphes"),
		tree_(0)
{
	ntuplefile_=0;
	ntuples_=0;
}

basicAnalyzer::~basicAnalyzer(){
	for(auto& it: histos_) {
		if(it.second)
			delete it.second;
	}
	histos_.clear();

	if(ntuplefile_) {
		ntuplefile_->Close();
	}

	delete ntuplefile_;
	delete ntuples_;
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
	//open tree
	tree_=new tTreeHandler(getSamplePath(),treename_);
	adjustNormalization(tree_);
	analyze(anaid);
}

void basicAnalyzer::processEndFunction(){
	fileForker::processEndFunction();
	if(tree_)
		delete tree_;
}

void basicAnalyzer::readConfigFile(const std::string& inputfile){
	if(debug)
		std::cout << "basicAnalyzer::readFileList" << std::endl;
	using namespace d_ana;
	using namespace std;

	fileReader fr;
	fr.setDelimiter(",");
	fr.setComment("$");
	fr.setStartMarker("[config-begin]");
	fr.setEndMarker("[config-end]");
	fr.readFile(inputfile);
	fr.setRequireValues(false);
	//get the configuration
	setOutDir(fr.getValue<TString>("Outputdir",""));
	setOutputFileName(fr.getValue<std::string>("Outputfile"));
	setLumi(fr.getValue<double>("Lumi"));
	setTestMode(fr.getValue<bool>("Testmode",false));

	setMaxChilds(fr.getValue<int>("Maxchilds",6));

	setDataSetDirectory(fr.getValue<TString>("Samplesdir"));

	fr.setRequireValues(true);
	fr.setStartMarker("[inputfiles-begin]");
	fr.setEndMarker("[inputfiles-end]");
	fr.readFile(inputfile);

	//check if ok

	infiles_.clear();
	legentries_.clear();
	colz_.clear();
	norms_.clear();
	legords_.clear();
	issignal_.clear();
	extraopts_.clear();
	std::vector<std::string> infiles;

	/*
	 *
	 * In case a directory is given, a possibility is to split or to extend (in the end the same)
	 * splitting further with same legend etc.
	 *
	 * Or the jobs are split by a script before. In this case, the stackplotter needs to be able
	 * to handle it (can manage many files at once)
	 * splitter needs to take care that same legends go to same job
	 *
	 */

	std::cout << "basicAnalyzer::readFileList: reading input file " << std::endl;

	std::string lscommandbegin;
	if(datasetdirectory_.BeginsWith("root://")){ //this is xrootd
		std::string fullpath=datasetdirectory_.Data();
		std::string pathworoot=fullpath.substr(7, fullpath.length());
		std::string server=pathworoot.substr(0,pathworoot.find_first_of("/"));
		server="root://"+server;
		std::string localpath=pathworoot.substr(pathworoot.find_first_of("/")+1,pathworoot.length());
		lscommandbegin="eos "+server;
		lscommandbegin+=" ls ";
		lscommandbegin+=localpath;

		//make checks regarding eos and grid proxys before continuing


	}
	else{
		lscommandbegin="ls ";
		lscommandbegin+=datasetdirectory_.Data();
	}


	for(size_t line=0;line<fr.nLines();line++){
		if(fr.nEntries(line) < 5){
			std::cout << "basicAnalyzer::readFileList: line " << line << " of inputfile is broken ("<<fr.nEntries(line)<< " entries.)" <<std::endl;
			sleep(2);
			continue;
		}

		TString singleinfile=(fr.getData<TString>(line,0));
		const TString legentry=fr.getData<TString>(line,1);
		const int col=fr.getData<int>    (line,2);

		std::vector<TString> infileshere;
		std::vector<double> normsinfl;
		const double xsec=fr.getData<double> (line,3);
		const size_t legndord=fr.getData<size_t> (line,4);
		bool issignal=false;
		if(fr.nEntries(line) > 5)
			issignal=fr.getData<bool> (line,5);
		TString otheropts;
		if(fr.nEntries(line) > 6)
			otheropts=fr.getData<TString> (line,6);

		if(singleinfile.EndsWith("/")){//this is a directory
			//ls files in dir
			//check for entries and total entries
			//modify and fill normsinfl entries
			//fill infileshere entries

			std::string command=lscommandbegin;

			command+=singleinfile.Data();
			tempFile tempfile;

			/*
			 char nameBuff[32];

			memset(nameBuff,0,sizeof(nameBuff));
			strncpy(nameBuff,"/tmp/myTmpFile-XXXXXX",21);
			int tmpfile=mkstemp(nameBuff);
			unlink(nameBuff);
			if(tmpfile<1) {
				//error
			}*/
			std::string pipeto=" 2>&1 >";
			pipeto+=tempfile.getName();
			system((command+pipeto).data());
			//read-back
			fileReader lsfr;
			lsfr.setDelimiter(" ");
			lsfr.setTrim("\n");
			lsfr.readFile(tempfile.getName());

			std::cout << "basicAnalyser::INFO: the input " << singleinfile << " is a directory.\nCreating file list and adjusting cross-sections to partial contributions of files\n(might take a while for many files. It is strongly encouraged to \"hadd\" them before!)... \n" <<std::endl; ;



			bool hasmetadata=false;
			for(size_t lsline=0;lsline<lsfr.nLines();lsline++){
				for(size_t lsentr=0;lsentr<lsfr.nEntries(lsline);lsentr++){
					const TString singlefile=singleinfile+lsfr.getData<TString>(lsline,0);
					if(singlefile.EndsWith(".root"))
						infileshere.push_back(singlefile);
					if(singlefile=="metaData"){
						hasmetadata=true;
					}
				}
			}
			lsfr.clear();
			tempfile.close();

			size_t usefiles=infileshere.size();
			if(testmode_ && usefiles){
				usefiles=infileshere.size()/100;
				if(!usefiles)
					usefiles=1;
				infileshere.erase(infileshere.begin()+usefiles,infileshere.end());
			}

			unsigned long totalentries=0;
			std::vector<unsigned long> indiventries;
			if(hasmetadata){
				//try to read from text files
				hasmetadata=false;

				//FIXME to be implemented in a next release
				/*
				command+="/metaData";
				tempfile.create();
				pipeto=" 2>&1 >";
				pipeto+=tempfile.getName();
				system((command+pipeto).data());
				const TString fullpath=singleinfile+"/metaData/";

				 */

			}
			if(!hasmetadata){
				//gEnv->SetValue("XNet.Debug", 2);

				//needs to be forked for root xrootd limitations (did I mention, I don't appreciate roots global vars...)
				IPCPipes<unsigned long> indiv;
				indiv.open((usefiles));
				IPCPipe<unsigned long> totalentr;
				int fret=fork();
				if(!fret){
					for(size_t subinfile=0;subinfile<usefiles;subinfile++){

						double percent= (((double)subinfile)/((double)usefiles));
						percent=round(percent*100. );
						std::cout << "\x1b[A\r" <<  percent << "%" <<std::endl;

						tTreeHandler treetmp(datasetdirectory_+infileshere.at(subinfile),treename_ );

						unsigned long thisentr=treetmp.entries();
						totalentries+=thisentr;
						indiventries.push_back(thisentr);

					}
					for(size_t subinfile=0;subinfile<usefiles;subinfile++){
						indiv.get(subinfile)->pwrite(indiventries.at(subinfile));
					}
					totalentr.pwrite(totalentries);
					std::cout << "\x1b[A\r" << "100%" <<std::endl;
					exit(EXIT_SUCCESS);
				}
				else{
					for(size_t subinfile=0;subinfile<usefiles;subinfile++){
						unsigned long entry=indiv.get(subinfile)->pread();
						indiventries.push_back(entry);
					}
					totalentries=totalentr.pread(); //block until child is done
				}
			}
			gROOT->Reset();
			std::cout << std::endl;
			for(size_t subinfile=0;subinfile<usefiles;subinfile++){
				double modifiedxsec=xsec;
				modifiedxsec *= (double)indiventries.at(subinfile)/(double)totalentries;
				std::cout << infileshere.at(subinfile) << " xsec: " << xsec
						<< " modified: " << modifiedxsec<< std::endl;
				normsinfl.push_back(modifiedxsec);
			}
			if(infileshere.size()<1){
				throw std::runtime_error("basicAnalyzer::readFileList: no input files found in this directory");
			}

		}
		else{
			infileshere.push_back(singleinfile);
			normsinfl.push_back(xsec);
		}
		for(size_t infl=0;infl<infileshere.size();infl++){
			infiles_.push_back   (infileshere.at(infl));
			if(debug)
				std::cout << "basicAnalyzer::readFileList: " << infiles_.at(infiles_.size()-1) << std::endl;
			legentries_.push_back(legentry);
			colz_.push_back      (col);
			norms_.push_back     (normsinfl.at(infl));
			legords_.push_back    (legndord);
			issignal_.push_back(issignal);
			extraopts_.push_back(otheropts);
		}
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
			std::cout << "PID    "<< textFormatter::fixLength("Filename",50)                << " statuscode " << "     progress " <<std::endl;
			int totalstatus=0;
			for(size_t i=0;i<infiles_.size();i++){
				totalstatus+=getBusyStatus(i);
				double percentpersecond=getBusyStatus(i)/runningseconds;
				double estimate=(100-getBusyStatus(i))/percentpersecond;
				std::cout
				<< textFormatter::fixLength(textFormatter::toString(getChildPids().at(i)),7)
				<< textFormatter::fixLength((legentries_.at(i)+"_"+toTString(i)).Data(),         50) <<" "
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
	std::cout << textFormatter::fixLength("Sample",30)                << " statuscode " << " progress " <<std::endl;
	for(size_t i=0;i<infiles_.size();i++)
		std::cout << textFormatter::fixLength((legentries_.at(i)+"_"+toTString(i)).Data(),50) << "     " << textFormatter::fixLength(translateStatus(getStatus(i)),15) <<"     " << "   " << getBusyStatus(i)<<"%"<<std::endl;
	std::cout << std::endl;


	//if no data, then create pseudodata
	if(std::find(legentries_.begin(),legentries_.end(),datalegend_) == legentries_.end()){
		///TBI!  FIXME
	}

	return stat;
}


void basicAnalyzer::setOutDir(const TString& dir){
	if(dir.Length()<1) return;
	if(dir.EndsWith("/"))
		outdir_=dir;
	else
		outdir_=dir+"/";
}


TH1* basicAnalyzer::addPlot(TH1* histo) {
	if(debug)
		std::cout << "basicAnalyzer::addPlot: " << legendname_ <<std::endl;

	if(histo==0) {
		std::cout << "WARNING (basicAnalyzer::addPlot): input histogram is a null pointer. Exiting..." << std::endl;
		exit(EXIT_FAILURE);
	}

	// TODO: format name according to some ruleset
	TString formattedName=histo->GetName();
	histo->Sumw2();
	histo->SetName(formattedName);

	if(histos_[formattedName]) {
		std::cout << "WARNING (basicAnalyzer::addPlot): histo " 
				<< histo->GetName() << " already exists. Adding..." <<std::endl;
		histos_[formattedName]->Add(histo);
	}
	else 
		histos_[formattedName]=histo;

	return histo;
}

TTree* basicAnalyzer::addTree(const TString& name) {
	if(debug)
		std::cout << "basicAnalyzer::addTree: " << legendname_ <<std::endl;

	// TODO: format name according to some ruleset
	TString formattedName=textFormatter::makeCompatibleFileName(name.Data());
	TString tdirname=textFormatter::makeCompatibleFileName(legendname_.Data());
	tdirname="d_"+tdirname;
	tdirname+=ownChildIndex();

	if(!ntuplefile_) {
		ntuplefile_ = new TFile(getOutDir() + tdirname+"_"+getTreePath(),"RECREATE");

		// write meta info
		metaInfo meta;
		meta.legendname=legendname_;
		meta.color=col_;
		meta.norm=norm_;
		meta.legendorder=legorder_;
		meta.Write();
	}

	ntuplefile_->cd();
	if(ntuples_ == 0) ntuples_ = new TTree(formattedName,formattedName);

	return ntuples_;
}


void  basicAnalyzer::adjustNormalization(const tTreeHandler*t){
	double entries=t->entries();
	double xsec=norm_;
	norm_= xsec*lumi_/entries;
}

fileForker::fileforker_status basicAnalyzer::writeOutput(){
	if(debug)
		std::cout << "basicAnalyzer::writeOutput: " << legendname_ <<std::endl;

	// get the directory name to write to
	TString tdirname=textFormatter::makeCompatibleFileName(legendname_.Data());
	tdirname="d_"+tdirname; //necessary otherwise problems with name "signal" in interactive root sessions

	// do we want to recreate the output file?
	TFile * outFile = new TFile(getOutPath(), "UPDATE");
	if(debug)
		std::cout << "basicAnalyzer::writeOutput || Opening TFile with setting UPDATE" << std::endl;

	bool exists=outFile->GetDirectory(tdirname,false);
	if(!exists){
		outFile->mkdir(tdirname);
		//outFile->cd(tdirname);

	}
	outFile->cd(tdirname);

	// write meta info
	metaInfo meta;
	meta.legendname=legendname_;
	meta.color=col_;
	meta.norm=norm_;
	meta.legendorder=legorder_;
	if(!exists)
		meta.Write();
	/*
	 * Try to write histograms
	 */
	try {
		if(debug)
			std::cout << "basicAnalyzer::writeOutput || Writing histograms" <<std::endl;
		for(auto& it: histos_) {

			it.second->Scale(getNorm());
			if(!exists)
				it.second->Write();
			else {
				TH1* thisto = (TH1*) outFile->Get(tdirname+"/"+it.second->GetName());
				if(thisto!=0) {
					thisto->Add(it.second);
					gDirectory->Delete((TString)it.second->GetName()+";1");
				}
				thisto->Write();
				delete thisto;
			}
		}
	} catch(Int_t e) {
		std::cout << "ERROR (basicAnalyzer::writeOutput): Problem while writing histograms to outfile" << std::endl;
		return ff_status_child_aborted;
	}

	/*
	 * Try to write ntuples
	 */
	try {
		if(writeTree_ && ntuplefile_ && ntuples_) {
			if(debug)
				std::cout << "basicAnalyzer::writeOutput || Writing ntuples" <<std::endl;
			/*
			 * now write the ntuple (if any)
			 * keep in mind: there will be a problem with the norms if they
			 * have the same legend name.... !
			 *
			 * TODO Create naming scheme for TTrees + merge ntuples_ with any
			 *      existing trees
			 */

			ntuplefile_->cd();
			meta.Write();
			ntuples_->Write();
			ntuplefile_->Close();
			delete ntuplefile_;
		}
	} catch(Int_t e) {
		std::cout << "ERROR (basicAnalyzer::writeHistos): Problem while writing ntuples to outfile" <<std::endl;
		return ff_status_child_aborted;
	}




	// clean-up
	outFile->Close();
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
