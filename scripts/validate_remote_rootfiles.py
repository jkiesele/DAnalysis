#!/usr/bin/env python


from argparse import ArgumentParser
import glob
import ROOT
from XRootD import client
from XRootD.client.flags import DirListFlags, OpenFlags, MkDirFlags, QueryCode

def check_root_ok(filename, treename="Delphes"):
    retval=True
    #rfile = ROOT.TFile(filename,"READ")
    rfile = ROOT.TFile.Open(filename)
    if rfile.TestBit(ROOT.TFile.kRecovered) : retval = False
    if rfile.IsZombie(): 
        rfile.Close()
        return False
    tree = rfile.Get(treename)
    if (not tree) or tree.IsZombie(): retval = False
    rfile.Close()
    return retval



parser = ArgumentParser('validate delphes root files in a directory')
#parser.add_argument('directory')
parser.add_argument('--SER')
parser.add_argument('--DIR')
args = parser.parse_args()

myclient = client.FileSystem(args.SER+':1094')
status, listing = myclient.dirlist(args.DIR, DirListFlags.STAT)
print listing.parent

rootfiles = []
for entry in listing:
    rootfiles.append(args.SER+'/'+args.DIR+entry.name)
#rootfiles = glob.glob(args.directory+"/*.root")
brokenfiles=[]
allfiles=len(rootfiles)
counter=0
for file in rootfiles:
    print('checking '+file+'... '+str(int(float(counter)/float(allfiles)*1000)/10.)+'%')
    counter+=1
    if not check_root_ok(file): 
        print('broken: '+file)
        brokenfiles.append(file)
        
        
print("Total root files:", len(rootfiles))
print('the following files are broken:')
print("Total broken root files:", len(brokenfiles))
#print(brokenfiles)
