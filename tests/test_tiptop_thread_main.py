"""
Usage: python test_tiptop_main.py [pass|passive]
pass - Used when user already configure commands run in background.
passive - Used when user already ran tiptop and saved in tiptop.old and tiptop.new
Need to put list of commands in testobjects.txt before running this script
"""
from sys import argv
from test_tiptop_thread import test_tiptop_thread
import unittest
import subprocess
import cPickle as pickle
import time

def main():   
    processlist = []
    objectlist = []
    objectoldmap = {}
    objectnewmap = {}
    objectnewtmap = {}
    old_tiptop = ''
    new_tiptop = ''
    n = 10
    
    if len (argv) > 1 and argv[1] != 'passive':
        old_tiptop = raw_input ("Enter path to old tiptop (eg ../src/tiptop): ")
        new_tiptop = raw_input ("Enter path to new tiptop: ")
    
    if old_tiptop == '':
        old_tiptop = "../../oldtiptop/tiptop/src/tiptop"
    if new_tiptop == '':
        new_tiptop = "../src/tiptop"
   
    #build tiptop command
    base_oldcmd = old_tiptop + " -b -c -n " + str(n) + " -o tiptop.old"
    base_newcmd = new_tiptop + " -b -c -n " + str(n) + " -o tiptop.new"    
    base_newtcmd = new_tiptop + " -b -c -H -n " + str(n) + " -o tiptop.newt"  
    
    #read testobjects.txt and build list of commands
    try:
        test_objects = open ("testobjects.txt")
        for line in test_objects:
            processlist.append(line.strip('\n') + " & ")
            objectlist.append(line.strip('\n'))      
        test_objects.close()
    except IOError:
        print "Missing testobjects.txt"
    
    #Run commands in background like firefox, google-chrome, skip if invoked with 'pass'
    if len (argv) > 1 and argv[1] != 'pass' and argv[1] != 'passive':
        for cmd in processlist:
            subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        time.sleep(2)         #wait to finish
     
    #use tiptop to evaluate info
    if len (argv) > 1 and argv[1] != 'passive':
        subprocess.Popen(base_oldcmd, stdout=subprocess.PIPE, shell=True)
        subprocess.Popen(base_newcmd, stdout=subprocess.PIPE, shell=True)
        subprocess.Popen(base_newtcmd, stdout=subprocess.PIPE, shell=True)
        time.sleep(n)         #wait to finish
                
    #Examine old file
    tiptop_output = open ("tiptop.old")  
    for line in tiptop_output:
        for elem in objectlist:
            if (elem in line) and (elem not in objectoldmap):
                tmplist = line.split()
                del (tmplist[11:])
                objectoldmap [elem] = tmplist               
    tiptop_output.close()
        
    #Examine new file
    tiptop_output = open ("tiptop.new")  
    for line in tiptop_output:
        for elem in objectlist:
            if (elem in line) and (elem not in objectnewmap):
                tmplist = line.split()
                del (tmplist[12:])
                objectnewmap [elem] = tmplist
    tiptop_output.close()

    #Examine new thread file
    tiptop_output = open ("tiptop.newt")  
    for line in tiptop_output:
        for elem in objectlist:
            if (elem in line) and (elem not in objectnewtmap):
                tmplist = line.split()
                del (tmplist[11:])
                objectnewtmap [elem] = tmplist
    tiptop_output.close()
    
    #dump info to file for test suite to read
    pickle.dump((objectoldmap, objectnewmap, objectnewtmap), open( "tiptop_test.obj", "wb"))
    
    #Run test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(test_tiptop_thread)
    unittest.TextTestRunner(verbosity=2).run(suite)
    
if __name__ == "__main__":
    main()