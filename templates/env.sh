SCRIPT=$(readlink -f "$0")
BASEDIR=$(dirname "${SCRIPT}")
OLDDIR=`pwd`
cd $BASEDIR
cd CMSSW_8_0_4
eval `scramv1 runtime -sh`
cd ..
export PYTHIA8=$CMSSW_RELEASE_BASE/../../../external/pythia8/212-ikhhed3
export LD_LIBRARY_PATH=$PYTHIA8/lib:$LD_LIBRARY_PATH
cd delphes
source DelphesEnv.sh
export DELPHES_PATH=`pwd`
cd $BASEDIR
cd DAnalysis
source env.sh
export DANALYSIS_WORKDIR=$BASEDIR/../
cd $OLDDIR
