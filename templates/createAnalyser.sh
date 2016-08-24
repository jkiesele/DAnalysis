#!/bin/zsh

ananame=$1

if [[ $ananame == "" ]]
then
    echo "USAGE: ${0} <analyser name>"
    exit -1
fi

src=$DANALYSISPATH/templates/

sed -e 's;ANALYSER_TEMPL;'${ananame}';g' < $src/ANALYSER_TEMPL.h > $DANALYSIS_WORKDIR/interface/${ananame}.h
sed -e 's;ANALYSER_TEMPL;'${ananame}';g' < $src/ANALYSER_TEMPL.cpp > $DANALYSIS_WORKDIR/src/${ananame}.cpp
sed -e 's;ANALYSER_TEMPL;'${ananame}';g' < $src/ANALYSER_TEMPL_EXEC.cpp > $DANALYSIS_WORKDIR/bin/${ananame}.cpp

