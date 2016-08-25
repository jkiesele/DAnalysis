/*
 * helpers.h
 *
 *  Created on: 1 Jul 2016
 *      Author: jkiesele
 */

#ifndef CROSSSECTION_SUMMER16_HELPERS_H_
#define CROSSSECTION_SUMMER16_HELPERS_H_

#include "TMatrixT.h"
#include "TVectorT.h"

template<class t>
std::string toString(t in) {
	std::ostringstream s;
	s << in;
	std::string out = s.str();
	return out;
}


template<class t>
TString toTString(t in) {
	std::ostringstream s;
	s << in;
	TString out = s.str();
	return out;
}

template <class T>
std::ostream& operator<<(std::ostream& os,  const TMatrixT<T>& m)
{
	std::streamsize save=os.width();
	os.width(4);
	std::streamsize savep=os.precision();
	os.precision(3);
	for(size_t i=0;i<(size_t)m.GetNrows();i++){
		for(size_t j=0;j<(size_t)m.GetNcols();j++){
			os.width(4);
			os.precision(2);
			os << std::left << toString(m[i][j])<<" ";
		}
		os<<'\n';
	}

	os.width(save);
	os.precision(savep);
	return os;
}

template <class T>
std::ostream& operator<<(std::ostream& os,  const TVectorT<T>& m)
{
	std::streamsize save=os.width();
	os.width(4);
	std::streamsize savep=os.precision();
	os.precision(3);
	for(size_t i=0;i<m.GetNrows();i++){
		os.width(4);
		os.precision(2);
		os << std::left << toString(m[i])<<" ";
		os<<'\n';
	}

	os.width(save);
	os.precision(savep);
	return os;
}



#endif /* CROSSSECTION_SUMMER16_HELPERS_H_ */
