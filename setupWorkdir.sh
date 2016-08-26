
#!/bin/zsh


#dont use basedir etc. check for $DANALYSISPATH environment and link there

if [[ $DANALYSISPATH == "" ]]
then
	echo "the DAnalysis environment must be set. To do this, source the env.(c)sh script in the same directory this script is located in"
    exit -1
fi


BASEDIR=$DANALYSISPATH
OLDDIR=`pwd`

workdir=$1

if [[ $workdir == "" ]]
then
	workdir="DAnalysis_workdir"
fi

if [[ -d 	$workdir ]]
then
    echo "directory ${workdir} already exists. Please specify another name"
    exit -1
    #rm -rf $workdir
fi	

mkdir $workdir
cd $workdir
workdir=`pwd -P`



cd $workdir
mkdir src
mkdir interface
mkdir bin
mkdir obj
mkdir config

cp $BASEDIR/templates/Makefile .
sed -e 's;##workdir##;'${workdir}';g' < $BASEDIR/templates/env.sh > $workdir/env.sh
sed -e 's;##workdir##;'${workdir}';g' < $BASEDIR/templates/createAnalyser.sh > $workdir/createAnalyser.sh
chmod +x $workdir/createAnalyser.sh
#maybe more of those
cp $BASEDIR/templates/*.txt config/


echo "Working directory ${workdir} set up."
echo "To create a skeleton analyser, run createAnalyser.sh <analyser name>"
echo "Standard configuration files for the analyser can be found in the subdirectory \"config\""
echo "The file testConfig.txt located there, gives further indications."




