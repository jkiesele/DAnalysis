
#!/bin/zsh



SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "${SCRIPT}")
OLDDIR=`pwd`

workdir=$1

if [[ $workdir == "" ]]
then
	workdir="DAnalysis_workdir"
fi

if [[ -d 	$workdir ]]
then
    echo "directory ${workdir} already exists. Please specify another name"
    #exit -1
    rm -rf $workdir
fi	

mkdir $workdir
cd $workdir
workdir=`pwd -P`

echo "copying DAnalysis framework and libraries"
cp -r $BASEDIR $workdir/


echo "copying delphes libraries"
cp -r $BASEDIR/../delphes $workdir/

echo "setting up working directory structure"
cd $workdir
mkdir src
mkdir interface
mkdir bin
mkdir obj

cp -r DAnalysis/examples .
cd bin
for f in ../examples/*
do
	echo ln -s $f
done
cd -
mv DAnalysis/templates/Makefile .
mv DAnalysis/templates/env.sh .
mv DAnalysis/templates/createAnalyser.sh .

scramv1 project CMSSW CMSSW_8_0_4




