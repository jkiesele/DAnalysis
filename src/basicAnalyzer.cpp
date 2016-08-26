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
#include <dirent.h>
#include <unistd.h>
#include <ctgmath>
#include <sys/stat.h>

#include "../interface/metaInfo.h"
#include "classes/DelphesClasses.h"
#include <fstream>
#include "TInterpreter.h"

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
		thissample_("","",0,0,0,0,false,""),
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
	//gInterpreter->GenerateDictionary("vector<Electron>","classes/DelphesClasses.h;vector");
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
	thissample_=samples_.at(anaid);
	isMC_=thissample_.getLegend() != datalegend_;
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
	configfile_=textFormatter::getFilename(inputfile,false);
	std::cout << configfile_ << std::endl;
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
	/*
	infiles_.clear();
	legentries_.clear();
	colz_.clear();
	norms_.clear();
	legords_.clear();
	issignal_.clear();
	extraopts_.clear();
	std::vector<std::string> infiles;
	 */
	samples_.clear();

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




	for(size_t line=0;line<fr.nLines();line++){
		if(fr.nEntries(line) < 6){
			std::cout << "basicAnalyzer::readFileList: line " << line << " of inputfile is broken ("<<fr.nEntries(line)<< " entries.)" <<std::endl;
			std::cout << fr.getReJoinedLine(line) <<std::endl;
			sleep(1);
			continue;
		}

		TString       if_sample=(fr.getData<TString>(line,0));
		const TString if_legentry=fr.getData<TString>(line,1);
		const int     if_col=fr.getData<int>    (line,2);
		const double  if_xsec=fr.getData<double> (line,3);
		bool autoentries=fr.getData<TString>(line,4)=="auto";
		int if_entries=-1;
		if(!autoentries)
			if_entries=fr.getData<int>(line,4);
		if(if_entries<0)
			autoentries=true;
		if(autoentries)
			if_entries=0;
		const int if_legndord=fr.getData<int> (line,5);
		bool if_issignal=false;
		if(fr.nEntries(line) > 6)
			if_issignal=fr.getData<bool> (line,6);
		TString if_otheropts;
		if(fr.nEntries(line) > 7)
			if_otheropts=fr.getData<TString> (line,7);


		std::vector<double> normsinfl;
		if( ! if_sample.EndsWith("/")){ //single file
			sampleDescriptor sampled(
					if_sample,
					if_legentry,
					if_col,
					if_xsec,
					if_entries,
					if_legndord,
					if_issignal,
					if_otheropts
			);

			samples_.push_back(sampled);
		}
		else{//this is a directory

			bool hasmetadata;
			std::vector<TString> infiles_indirectory;
			infiles_indirectory=lsDirectory(if_sample,hasmetadata);

			if(infiles_indirectory.size()<1){
				throw std::runtime_error("basicAnalyzer::readFileList: no input files found in this directory");
			}

			if(testmode_ && infiles_indirectory.size()){
				size_t usefiles=infiles_indirectory.size();
				usefiles=infiles_indirectory.size()/100;
				if(!usefiles)
					usefiles=1;
				infiles_indirectory.erase(infiles_indirectory.begin()+usefiles,infiles_indirectory.end());
			}

			unsigned long totalentries=0;
			if(autoentries){
				std::cout << "basicAnalyser::INFO: the input " << if_sample << " is a directory.\nCreating file list and adjusting cross-sections to partial contributions of files\n(might take a while for many files. It is strongly encouraged to \"hadd\" them before!)... \n" <<std::endl; ;

				if(hasmetadata){
					//try to read from text files
					hasmetadata=false;
					//FIXME to be implemented in a next release, read from the text files
				}

				if(!hasmetadata){
					getIndivEntries(totalentries, infiles_indirectory);
				}
				std::cout << "extracted the total number of entries in directory: "<<totalentries
						<<"\n(add this to your configuration file instead of \"auto\"!)\n"<< std::endl;
			}
			else{//not auto entries
				totalentries=if_entries;
			}
			for(size_t infl=0;infl<infiles_indirectory.size();infl++){
				sampleDescriptor sampled(
						infiles_indirectory.at(infl),
						if_legentry,
						if_col,
						if_xsec,
						totalentries,
						if_legndord,
						if_issignal,
						if_otheropts
				);
				if(debug)
					std::cout << "basicAnalyzer::readConfigFile: added sample\n"
					<<sampled.getInfile() <<" "<< sampled.getDirentries()<< std::endl;
				samples_.push_back(sampled);
				if(debug)
					std::cout << "basicAnalyzer::readFileList: " << sampled.getLegend() << std::endl;
			}

		}//directory

	}
	std::vector<std::string > ffinfiles;
	for(size_t i=0;i<samples_.size();i++){
		ffinfiles.push_back(samples_.at(i).getInfile().Data());
	}
	fileForker::setInputFiles(ffinfiles);
	std::cout << "basicAnalyzer::readFileList: input file successfully read. Free for changes." <<std::endl;
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
	std::vector<double> lastAliveSignals(samples_.size());
	std::vector<int>    lastBusyStatus(samples_.size());
	while(stat==fileForker::ff_status_parent_busy || stat== fileForker::ff_status_parent_childstospawn){

		fileForker::fileforker_status writestat=spawnChildsAndUpdate();
		stat=getStatus();
		if(writestat == ff_status_parent_filewritten || (interval>0  && counter>interval)){
			time(&now);
			double runningseconds = fabs(difftime(started,now));
			std::cout << "PID    "<< textFormatter::fixLength("Sample",30)                << " statuscode " << "         progress " <<std::endl;
			int totalstatus=0;
			for(size_t i=0;i<samples_.size();i++){
				totalstatus+=getBusyStatus(i);
				double percentpersecond=getBusyStatus(i)/runningseconds;
				double estimate=(100-getBusyStatus(i))/percentpersecond;
				std::cout
				<< textFormatter::fixLength(textFormatter::toString(getChildPids().at(i)),7)
				<< textFormatter::fixLength((samples_.at(i).getLegend()+"_"+toTString(i)).Data(),         30) <<" "
				<< textFormatter::fixLength(translateStatus(getStatus(i)), 20)
				<< textFormatter::fixLength(textFormatter::toString(getBusyStatus(i))+"%",4,false);
				if(getBusyStatus(i)>4 && getStatus(i) == ff_status_child_busy){
					std::cout  <<" - ";
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
			std::cout << "total: "<< (int)(totalstatus / ((double)samples_.size())) << "%"<< std::endl;
			std::cout << std::endl;
			counter=0;
		}
		counter++;
		usleep (250e3);
	}

	//check if skim file has been written. If so, add last line
	writeConfigFooter();

	std::cout << "End report:" <<std::endl;
	std::cout << textFormatter::fixLength("Sample",30)                << " statuscode " << "          progress " <<std::endl;
	for(size_t i=0;i<samples_.size();i++)
		std::cout << textFormatter::fixLength((samples_.at(i).getLegend()+"_"+toTString(i)).Data(),30) << " " << textFormatter::fixLength(translateStatus(getStatus(i)),20) <<" " << getBusyStatus(i)<<"%"<<std::endl;
	std::cout << std::endl;


	//if no data, then create pseudodata
	/*if(std::find(legentries_.begin(),legentries_.end(),datalegend_) == legentries_.end()){
		///TBI!  FIXME
	}
	 */
	return stat;
}


void basicAnalyzer::setOutDir(const TString& dir){
	if(dir.Length()<1) return;
	if(dir.EndsWith("/"))
		outdir_=dir;
	else
		outdir_=dir+"/";
}


TH1* basicAnalyzer::addPlot(TH1* histo, const TString xaxis, const TString yaxis, const TString zaxis) {
	if(debug)
		std::cout << "basicAnalyzer::addPlot: " <<  thissample_.getLegend()<<std::endl;

	if(histo==0) {
		std::cout << "WARNING (basicAnalyzer::addPlot): input histogram is a null pointer. Exiting..." << std::endl;
		exit(EXIT_FAILURE);
	}


	TString formattedName=histo->GetName();
	histo->Sumw2();
	histo->SetName(formattedName);
	histo->GetXaxis()->SetTitle(xaxis);
	histo->GetYaxis()->SetTitle(yaxis);
	if(histo->GetZaxis())
		histo->GetZaxis()->SetTitle(zaxis);

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
		std::cout << "basicAnalyzer::addTree: " << thissample_.getLegend() <<std::endl;




	TString formattedName=textFormatter::makeCompatibleFileName(name.Data());

	if(!ntuplefile_) {
		ntuplefile_ = new TFile(getOutDir() + makeNTupleFileName(),"RECREATE");

		// write meta info
		metaInfo meta;
		meta.legendname=thissample_.getLegend();
		meta.color=thissample_.getColor();
		meta.norm=thissample_.getNorm();
		meta.legendorder=thissample_.getLegendorder();
		meta.Write();
	}

	ntuplefile_->cd();
	if(ntuples_ == 0) ntuples_ = new TTree(formattedName,formattedName);

	return ntuples_;
}


void  basicAnalyzer::adjustNormalization(const tTreeHandler*t){
	long entries=t->entries();
	double xsec=thissample_.getXsec();
	double multiplier=1;
	if(thissample_.getDirentries()){
		multiplier=((double)entries)/(double)thissample_.getDirentries();
		if(debug)
			std::cout << "multi for "<< thissample_.getInfile() << ": "<<multiplier << std::endl;
	}else{
		thissample_.setDirentries(entries);
	}
	thissample_.setNorm( xsec*lumi_/((double)entries) * multiplier);
}

std::vector<TString> basicAnalyzer::lsDirectory(const TString & dir, bool& hasmetadata, const TString mdtag,const TString sampleextension)const{

	std::vector<TString> filesindir;
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

		//make checks regarding eos and grid proxys before continuing!! FIXME


	}
	else{
		lscommandbegin="ls ";
		lscommandbegin+=datasetdirectory_.Data();
	}
	std::string command=lscommandbegin;

	command+=dir.Data();
	tempFile tempfile;

	std::string pipeto=" 2>&1 >";
	pipeto+=tempfile.getName();
	system((command+pipeto).data());
	//read-back
	fileReader lsfr;
	lsfr.setDelimiter(" ");
	lsfr.setTrim("\n ");
	lsfr.readFile(tempfile.getName());

	hasmetadata=false;
	for(size_t lsline=0;lsline<lsfr.nLines();lsline++){
		for(size_t lsentr=0;lsentr<lsfr.nEntries(lsline);lsentr++){
			const TString singlefile=dir+lsfr.getData<TString>(lsline,0);
			if(singlefile.EndsWith(sampleextension))
				filesindir.push_back(singlefile);
			if(singlefile==mdtag){
				hasmetadata=true;
			}
		}
	}
	lsfr.clear();
	tempfile.close();
	return filesindir;
}



std::vector<unsigned long> basicAnalyzer::getIndivEntries(unsigned long& totalentries, const std::vector<TString>& infiles_indirectory)const{

	//needs to be forked for root xrootd limitations
	//(did I mention, I don't like root...)
	size_t usefiles=infiles_indirectory.size();
	std::vector<unsigned long> indiventries;
	totalentries=0;
	IPCPipes<unsigned long> indiv;
	indiv.open(usefiles);
	IPCPipe<unsigned long> totalentr;
	int fret=fork();
	if(!fret){
		for(size_t subinfile=0;subinfile<usefiles;subinfile++){

			double percent= (((double)subinfile)/((double)usefiles));
			percent=round(percent*100. );
			std::cout << "\x1b[A\r" <<  percent << "%" <<std::endl;

			tTreeHandler treetmp(datasetdirectory_+infiles_indirectory.at(subinfile),treename_ );

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
	//parent
	for(size_t subinfile=0;subinfile<usefiles;subinfile++){
		unsigned long entry=indiv.get(subinfile)->pread();
		indiventries.push_back(entry);
	}
	totalentries=totalentr.pread(); //block until child is done

	return indiventries;
}

fileForker::fileforker_status basicAnalyzer::writeOutput(){
	if(debug)
		std::cout << "basicAnalyzer::writeOutput: " << thissample_.getLegend() <<std::endl;

	// get the directory name to write to
	TString tdirname=textFormatter::makeCompatibleFileName(thissample_.getLegend() .Data());
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
	meta.legendname=thissample_.getLegend() ;
	meta.color=thissample_.getColor();
	meta.norm=thissample_.getNorm();
	meta.legendorder=thissample_.getLegendorder();
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

			//create or open output list to be read back
			std::fstream skimfile;
			writeConfigHeader(skimfile);
			skimfile<< makeNTupleFileName() <<", ";
			skimfile << getLegendName() <<", ";
			skimfile << getColor() << ", ";
			skimfile << thissample_.getXsec() << ", ";
			skimfile << thissample_.getDirentries() <<",";
			skimfile << getLegendOrder() ;
			if(thissample_.isSignal())
				skimfile << ", true ";
			else
				skimfile << ", false ";
			if(thissample_.getExtraopts().Length()>0)
				skimfile << ", " << thissample_.getExtraopts();
			skimfile<< "\n";
			skimfile.close();

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

bool basicAnalyzer::writeConfigHeader(std::fstream &file)const{

	const std::string outfile=(outdir_+"skimmed_"+configfile_).Data();
	struct stat buffer;
	bool alreadyexists = (stat (outfile.c_str(), &buffer) == 0);

	if(alreadyexists){

		file.open(outfile, std::fstream::out | std::fstream::app);

		return alreadyexists;
	}

	//	file.open(outfile, std::fstream::out | std::fstream::trunc);

	file.open(outfile, std::fstream::out);
	file << "[config-begin]\n\t";
	file << "Outputdir  = set_new_output_directory" <<"\n\t";
	file << "Outputfile = " << getOutFileName() <<"\n\t";
	file << "Lumi = " << lumi_ <<"\n\t";
	file << "Maxchilds = " << getMaxChilds() <<"\n\t";
	file << "Samplesdir = " << getOutDir() <<"\n";
	file << "[config-end]\n\n[inputfiles-begin]\n";

	file.close();
	writeConfigHeader(file); //write this sample
	return false;
}
void basicAnalyzer::writeConfigFooter()const{
	const std::string outfile=(outdir_+"skimmed_"+configfile_).Data();
	struct stat buffer;
	bool alreadyexists = (stat (outfile.c_str(), &buffer) == 0);
	if(!alreadyexists)return;

	std::fstream file(outfile, std::fstream::out | std::fstream::app);
	file << "[inputfiles-end]\n";
	file.close();

	std::cout << "basicAnalyzer::writeConfigFooter: configuration file for skim written to:\n"
			<< outfile << std::endl;

}

TString basicAnalyzer::makeNTupleFileName()const{


	TString tdirname=textFormatter::makeCompatibleFileName(thissample_.getLegend().Data());

	tdirname+=ownChildIndex();
	tdirname+="_ntuple.root";

	return tdirname;

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
