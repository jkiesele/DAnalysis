#!/usr/bin/env python


from argparse import ArgumentParser
import glob
import ROOT


def check_root_ok(filename, treename="Delphes"):
    retval=True
    rfile = ROOT.TFile(filename,"READ")
    if rfile.TestBit(ROOT.TFile.kRecovered) : retval = False
    if rfile.IsZombie(): 
        rfile.Close()
        return False
    tree = rfile.Get(treename)
    if (not tree) or tree.IsZombie(): retval = False
    rfile.Close()
    return retval



parser = ArgumentParser('validate delphes root files in a directory')
parser.add_argument('directory')
args = parser.parse_args()

rootfiles = glob.glob(args.directory+"/*.root")
brokenfiles=[]
allfiles=len(rootfiles)
counter=0
for file in rootfiles:
    print('checking '+file+'... '+str(int(float(counter)/float(allfiles)*1000)/10.)+'%')
    counter+=1
    if not check_root_ok(file): 
        print('broken: '+file)
        brokenfiles.append(file)
        
        
print('the following files are broken:')
print(brokenfiles)