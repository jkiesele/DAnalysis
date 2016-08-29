
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
sed -e 's;##workdir##;'${workdir}';g' -e 's;##basedir##;'${BASEDIR}';g' < $BASEDIR/templates/env.sh > $workdir/env.sh
sed -e 's;##workdir##;'${workdir}';g' < $BASEDIR/templates/createAnalyser.sh > $workdir/createAnalyser.sh
chmod +x $workdir/createAnalyser.sh
#maybe more of those
cp $BASEDIR/templates/*.txt config/

cd $OLDDIR

echo ""
echo "Working directory ${workdir} set up."
echo "Before doing anything, please first set up the environment by sourcing <workdir>/env.(c)sh."
echo ""
echo "To create a skeleton analyser, run createAnalyser.sh <analyser name>"
echo "Standard configuration files for the analyser can be found in the subdirectory \"config\""
echo "The file testConfig.txt located there, gives further indications."
echo ""
echo "To run production, the corresponding scripts are reachable in \$DANALYSISPATH/scripts"
echo "A README file containing instructions for production is also located there"




