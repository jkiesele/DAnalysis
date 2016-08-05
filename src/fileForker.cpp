

#include "../interface/fileForker.h"
#include <sys/stat.h>
#include <iostream>
#include <sys/types.h>
#include <stdexcept>
#include "TString.h"

namespace d_ana{



bool fileForker::debug=false;


fileForker::fileForker():ischild_(false),spawnready_(false),
		maxchilds_(12),
		runningchilds_(0),
		donechilds_(0),
		lastspawned_(-1),
		PID_(0),
		ownchildindex_(FF_PARENTINDEX),
		processendcalled_(false)
{



}


fileForker::~fileForker(){

}


fileForker::fileforker_status fileForker::prepareSpawn(){
	if(!createOutFile())
		return ff_status_parent_generror;
	lastspawned_=-1;
	size_t filenumber=inputfiles_.size();
	if(!filenumber)
		throw std::logic_error("fileForker::prepareSpawn: no input");
	p_idx.open(filenumber);
	p_allowwrite.open(filenumber);
	p_askwrite.open(filenumber);
	p_status.open(filenumber);
	p_busystatus.open(filenumber);

	childPids_.resize(filenumber,0);
	runningchilds_=0;
	busystatus_.resize(filenumber,0);
	childstatus_.resize(filenumber,ff_status_child_hold);
	ischild_=false;
	spawnready_=true;
	return ff_status_parent_success;
}

fileForker::fileforker_status fileForker::spawnChildsAndUpdate(){
	if(debug)
		std::cout << "fileForker::spawnChilds: " << inputfiles_.size()<<std::endl;
	using namespace std;
	using namespace d_ana;
	if(!spawnready_)
		throw std::logic_error("fileForker::spawnChilds: first prepare spawn");


	size_t filenumber=inputfiles_.size();
	//spawn
	if(filenumber>1){
		for(size_t i=0;i<filenumber;i++){
			while(p_busystatus.get(i)->preadready())
				busystatus_.at(i)=p_busystatus.get(i)->pread();
		}


		for(int i=lastspawned_+1;i<(int)filenumber;i++){
			if(runningchilds_>=maxchilds_)
				break;
			if(debug)
				std::cout << "last: " << lastspawned_<< " i: "<< i << std::endl;
			childPids_.at(i)=fork();
			if(childPids_.at(i)==0){//child process
				ischild_=true;

				try{
					if(debug)
						std::cout << "fileForker::spawnChilds child "<<i << std::endl;
					ownchildindex_=p_idx.get(i)->pread(); //wait until parent got ok
					p_status.get(ownchildindex_)->pwrite((int)ff_status_child_busy);
					reportBusyStatus(0);
					processendcalled_=false;
					process();
					if(!processendcalled_){
						throw std::logic_error("fileForker: implementation of process() must call processEndFunction() at its end!");
					}
					std::exit(EXIT_SUCCESS);
				}catch(std::exception& e){
					std::cout << "*************** Exception ***************" << std::endl;
					std::cout << e.what() << std::endl;
					std::cout << "in " << getInputFileName() << '\n'<<std::endl;
					writeReady_block(); //pro-forma
					writeDone(ff_status_child_exception);
					std::exit(EXIT_FAILURE);
				}
			}
			else{ //parent process
				if(debug)
					std::cout << "fileForker::spawnChilds parent "<< std::endl;
				ownchildindex_=FF_PARENTINDEX;//start processing
				p_idx.get(i)->pwrite(i); //send start signal
				childstatus_.at(i)=(fileforker_status)p_status.get(i)->pread();
				runningchilds_++;
				lastspawned_=i;

				//check for write requests
			}
		}

		//future: this could go to another thread, if so, make ++ and -- atomic
		//here, there is no root bogus, so it should be no problem (are pipes thread safe?)
		int it_readytowrite=checkForWriteRequest();
		if(it_readytowrite >= 0){ // daugh ready to write
			p_allowwrite.get(it_readytowrite)->pwrite(it_readytowrite);
			childstatus_.at(it_readytowrite)=ff_status_child_writing;
			if(debug)
				std::cout << "fileForker::spawnChilds waiting for write "<< std::endl;
			childstatus_.at(it_readytowrite)=(fileforker_status)p_status.get(it_readytowrite)->pread();
			//done
			runningchilds_--;
			donechilds_++;
		}


		for(size_t c=0;c<inputfiles_.size();c++){//update the status
			if(p_status.get(c)->preadready())
				childstatus_.at(c)=(fileforker_status)p_status.get(c)->pread();
		}

		if(donechilds_ == filenumber)
			return ff_status_parent_success;

		if(it_readytowrite>=0)
			return ff_status_parent_filewritten;

	}
	else{ //only one process
		std::cout << "fileForker::spawnChilds: only one process, running in main thread. No progress information will be available"  << std::endl;
		ownchildindex_=0;
		ischild_=false;
		donechilds_++;
		lastspawned_=1;
		process();
	}


	return ff_status_parent_busy;
}

fileForker::fileforker_status fileForker::getStatus(const size_t& childindex)const{

	return childstatus_.at(childindex);

}

fileForker::fileforker_status fileForker::getStatus()const{
	//update child status

	if(debug){
		std::cout << "fileForker::getStatus: "<< donechilds_<<"("<< runningchilds_<< ")/"<< inputfiles_.size()<<
				" last "<< lastspawned_<< std::endl;
		std::cout << "child stats: " <<std::endl;
		for(size_t c=0;c<inputfiles_.size();c++){
			std::cout << c << "\t"  << (int)childstatus_.at(c) <<"\t"<<inputfiles_.at(c) <<std::endl;
		}
	}
	if(! inputfiles_.size())
		return ff_status_parent_success;
	if(lastspawned_<(int)inputfiles_.size()-1)
		return ff_status_parent_childstospawn;
	if(donechilds_<inputfiles_.size())
		return ff_status_parent_busy;
	for(size_t i=0;i<childstatus_.size();i++){
		if(childstatus_.at(i) == ff_status_child_aborted
				|| childstatus_.at(i) == ff_status_child_exception
				|| childstatus_.at(i) == ff_status_child_generror)
			return ff_status_parent_generror;
	}

	return
			ff_status_parent_success;
}



int fileForker::getBusyStatus(const size_t& childindex){
	while(p_busystatus.get(childindex)->preadready())
		busystatus_.at(childindex)=p_busystatus.get(childindex)->pread();
	return busystatus_.at(childindex);
}

bool fileForker::writeReady_block(){
	if(debug)
		std::cout << "fileForker::writeReady_block: ischild: "<< ischild_ << std::endl;
	if(!ischild_)
		return true;

	p_askwrite.get(ownchildindex_)->pwrite(ownchildindex_);
	p_allowwrite.get(ownchildindex_)->pread();
	if(debug)
		std::cout << "fileForker::writeReady_block: done" << std::endl;
	return true;

}
void fileForker::writeDone(fileforker_status stat){
	if(debug)
		std::cout << "fileForker::writeDone" << std::endl;
	if(!ischild_)
		return;
	p_status.get(ownchildindex_)->pwrite(stat);

}
std::string fileForker::translateStatus(const fileforker_status& stat)const{
#define RETURNSTATUSSTRING(x) {if(stat==ff_status_ ## x) str= #x;}
	std::string str;
	RETURNSTATUSSTRING(child_hold)
	RETURNSTATUSSTRING(child_busy)
	RETURNSTATUSSTRING(child_writing)
	RETURNSTATUSSTRING(child_success)
	RETURNSTATUSSTRING(child_generror)
	RETURNSTATUSSTRING(child_exception)
	RETURNSTATUSSTRING(child_aborted)
	RETURNSTATUSSTRING(parent_childstospawn)
	RETURNSTATUSSTRING(parent_success)
	RETURNSTATUSSTRING(parent_busy)
	RETURNSTATUSSTRING(parent_generror)
	RETURNSTATUSSTRING(parent_exception)
	RETURNSTATUSSTRING(parent_filewritten)
	RETURNSTATUSSTRING(allowwrite)
	RETURNSTATUSSTRING(askwrite)
	return str;
#undef RETURNSTATUSSTRING
}

void fileForker::reportStatus(fileforker_status stat){
	p_status.get(ownchildindex_)->pwrite(stat);
}
void fileForker::reportBusyStatus(int bstat){
	p_busystatus.get(ownchildindex_)->pwrite(bstat);
}

void fileForker::processEndFunction(){
	if(debug)
		std::cout << "fileForker::processEndFunction" << std::endl;
	processendcalled_=true;
	writeReady_block() ;
	fileforker_status stat=writeOutput();
	writeDone(stat);
}

int fileForker::checkForWriteRequest(){
	for(size_t i=0;i<p_askwrite.size();i++){
		if(p_askwrite.get(i)->preadready()){
			p_askwrite.get(i)->pread(); //to free pipe again
			return i; //return first ready to write!
		}
	}
	return -1;
}

void fileForker::abortChild(size_t idx){
	if(idx>=childPids_.size())
		throw std::out_of_range("fileForker::abortChild: index");

	//make nasty system call
	std::string syscall="kill -9 ";
	syscall+=((TString)(childPids_.at(idx))).Data();
	system(syscall.data());
	childstatus_.at(idx)=ff_status_child_aborted;
	runningchilds_--;
	donechilds_++;
}

}




