/*
 * tBranchHandler.h
 *
 *  Created on: May 28, 2014
 *      Author: kiesej
 */

#ifndef TBRANCHHANDLER_H_
#define TBRANCHHANDLER_H_

#include "TString.h"
#include "TBranch.h"
#include <stdexcept>
#include <iostream>
#include <map>
#include "tTreeHandler.h"
//#include  <boost/type_traits/is_fundamental.hpp>
#include  <boost/type_traits.hpp>

namespace d_ana{


class tBranchHandlerBase{
public:
	tBranchHandlerBase():gotentry_(false),t_(0),buf_max_(1){}
	tBranchHandlerBase(size_t buf):gotentry_(false),t_(0),buf_max_(buf),missingbranch_(false){}
	virtual ~tBranchHandlerBase(){}

	const TString& getBranchName()const{return branchname_;}

	const bool& gotEntry()const{return gotentry_;}
	void newEntry(){gotentry_=false;}

	static bool debug;

	virtual void removeTree(tTreeHandler * )=0;

	const size_t & getBufMax()const{return buf_max_;}

protected:

	void handleReturns(int,bool&,bool)const;

	void addTreeAndBranch(tTreeHandler * t, const TString& branchname);
	void removeTreeAndBranch(tTreeHandler * t, const TString& branchname);
	//avoids double setting
	static std::map< tTreeHandler* ,std::vector<TString> > branchesfortree_;
	bool gotentry_;
	tTreeHandler *t_;
	TString branchname_;
	const size_t buf_max_;
	bool missingbranch_;
};

template<class T>
class tBranchHandler;
namespace tBranchHandlerHelpers{
template<class T>
int tBranchHandler_createContentsAndAssociate(tBranchHandler<T>*bh, T &rc, T*& rcp, TBranch*& br, bool& isprimitive, bool& ispointer){
	if(tBranchHandlerBase::debug)
		std::cout << "tBranchHandler_createContents: non-array type for "<<bh->getBranchName()<<std::endl;
	ispointer=false;
	isprimitive = (boost::is_fundamental<T>::value && ! boost::is_void<T>::value)
									||(boost::is_pointer<T>::value && boost::is_fundamental<typename boost::remove_pointer<T>::type>::value);

	if(tBranchHandlerBase::debug && isprimitive)
		std::cout << "tBranchHandler: " << bh->getBranchName() << ": primitive type"<<std::endl;
	rc=  T();
	rcp=&rc;
	if(isprimitive){
		return bh->tree()->tree()->SetBranchAddress(bh->getBranchName(),rcp,&br);
	}
	else{
		rcp=0;
		return bh->tree()->tree()->SetBranchAddress(bh->getBranchName(),&rcp,&br);
	}
}
template<class T>
int tBranchHandler_createContentsAndAssociate(tBranchHandler<T*>*bh, T* &rc, T** & rcp, TBranch*& br, bool& isprimitive, bool& ispointer){
	if(tBranchHandlerBase::debug)
		std::cout << "tBranchHandler_createContents: array type for "<<bh->getBranchName() <<" buffer: "<<bh->buffMax() <<std::endl;
	if(bh->buffMax()<1)
		throw std::out_of_range("tBranchHandler_createContents: buffer size below 1 not allowed");
	ispointer=true;
	rc= new T[bh->buffMax()];
	isprimitive = (boost::is_fundamental<T>::value && ! boost::is_void<T>::value)
										||(boost::is_pointer<T>::value && boost::is_fundamental<typename boost::remove_pointer<T>::type>::value);

	if(tBranchHandlerBase::debug && isprimitive)
		std::cout << "tBranchHandler: " << bh->getBranchName() << ": primitive type"<<std::endl;
	if(isprimitive){
		rcp=&rc;
		return bh->tree()->tree()->SetBranchAddress(bh->getBranchName(),rc,&br);
	}
	else
		throw std::runtime_error("tBranchHandler_createContentsAndAssociate: pointer to non-primitive not implemented"); //return bh->tree()->tree()->SetBranchAddress(bh->getBranchName(),&rcp,&br);

}

template<class T>
void tBranchHandler_removeContents(tBranchHandler<T>*b, T &t){
	if(tBranchHandlerBase::debug)
		std::cout << "tBranchHandler_removeContents: non-array type for "<<b->getBranchName()<<std::endl;
	return;
}

template<class T>
void tBranchHandler_removeContents(tBranchHandler<T*>*b, T* &t){
	if(tBranchHandlerBase::debug)
		std::cout << "tBranchHandler_removeContents: array type for "<<b->getBranchName()<<std::endl;
	 delete t;
}
}

/**
 * This is just a small wrapper to make TBranch reading and access less ambiguous
 * and easier
 */
template<class T>
class tBranchHandler : public tBranchHandlerBase{
public:
	tBranchHandler():tBranchHandlerBase(),pcontent_(0),branch_(0),
	isPrimitive_(false),isPrimitiveArray_(false){
		// doesn't do anything
		throw std::logic_error("tBranchHandler: default constructor should not be used");
	}
	tBranchHandler(tTreeHandler * t, const TString& branchname,  size_t buf_max=1 ):tBranchHandlerBase(buf_max),
			branch_(0),isPrimitive_(false),isPrimitiveArray_(false){
		if(!t){
			throw std::runtime_error("tBranchHandler: tree pointer is NULL!");
		}
		if(debug)
			std::cout << "tBranchHandler: " << branchname_<< std::endl;


		branchname_=branchname;
		t_=t;


		int ret= tBranchHandlerHelpers::tBranchHandler_createContentsAndAssociate
				(this,realcontent_,pcontent_,branch_,isPrimitive_,isPrimitiveArray_);


		handleReturns(ret,missingbranch_,allow_missing);
		if(missingbranch_){
			pcontent_=&realcontent_;
		}
		if(debug)
			std::cout << "tBranchHandler: loaded " << branchname_<< std::endl;
		addTreeAndBranch(t,branchname);
		if(debug)
			std::cout << "tBranchHandler: associated " << branchname_ << " with " << t->tree()->GetName()<< std::endl;

	}
	~tBranchHandler(){
		if(debug)
			std::cout << "~tBranchHandler: " << branchname_<< std::endl;
		removeTreeAndBranch(t_,branchname_);
		if(pcontent_ && !missingbranch_){
			if(!isPrimitive_)
				delete pcontent_;
			 if(isPrimitiveArray_){
				tBranchHandlerHelpers::tBranchHandler_removeContents(this,realcontent_);
			}

			pcontent_=0;
		} /*obsolete but in case some sharing is done at some point*/
		t_=0;

	}

	void removeTree(tTreeHandler * t){
		if(t && t == t_){
			t_->tree()->ResetBranchAddress(branch_);
			t_=0;
		}
	}

	/**
	 * always copy | safer
	 */
	T  * content(){
		if(!t_)
			throw std::out_of_range("tBranchHandler::content: no tree associated");
		if(!gotentry_){
			getEntry(t_->currentEntry());
			if(!isPrimitive_)
				realcontent_=*pcontent_;//copy high level objects
		}
		return &realcontent_;
	}
	/**
	 * Allows missing and just returns an empty vector
	 */
	static bool allow_missing;

	bool ismissing()const{return missingbranch_;}

	tTreeHandler* tree(){return t_;}
	const tTreeHandler* tree()const {return t_;}

	const size_t& buffMax()const{return buf_max_;}

private:

	void getEntry(const Long64_t& entry){
		if(!missingbranch_){
			if(!branch_)
				throw std::logic_error("tBranchHandler::getEntry: branch NULL");
			branch_->GetEntry(entry);
		}
		gotentry_=true;
	}

	T* pcontent_;
	T  realcontent_;
	TBranch * branch_;
	bool isPrimitive_ ;
	bool isPrimitiveArray_ ;

};

template <typename T>
bool tBranchHandler<T>::allow_missing=false;



}

#endif /* TBRANCHHANDLER_H_ */
