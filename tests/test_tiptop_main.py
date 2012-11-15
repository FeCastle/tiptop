"""
Usage: python test_tiptop_main.py [pass]
pass - Used when user already configure commands run in background.
Need to put list of commands in testobjects.txt before running this script
"""
from sys import argv
from test_tiptop_fd import test_tiptop_fd
import unittest
import subprocess
import cPickle as pickle
import time

def main():   
    processlist = []
    outputlist = []
    objectlist = []
    objectoldmap = {}
    objectnewmap = {}
    n = 2
    
    old_tiptop = raw_input ("Enter path to old tiptop (eg ../src/tiptop): ")
    new_tiptop = raw_input ("Enter path to new tiptop: ")
    
    if old_tiptop == '':
        old_tiptop = "../../oldtiptop/tiptop/src/tiptop"
    if new_tiptop == '':
        new_tiptop = "../src/tiptop"
   
    #build tiptop command
    base_oldcmd = old_tiptop + " -b -c -n " + str(n) + " -o "
    base_newcmd = new_tiptop + " -b -c -n " + str(n) + " -o "    
    
    #read testobjects.txt and build list of commands
    try:
        test_objects = open ("testobjects.txt")
        for line in test_objects:
            processlist.append(line.strip('\n') + " & ")
            objectlist.append(line.strip('\n'))
            outputlist.append(line.strip('\n') + ".output")       
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
        #for output in outputlist:
        subprocess.Popen(base_oldcmd + "tiptop.old", stdout=subprocess.PIPE, shell=True)
        subprocess.Popen(base_newcmd + "tiptop.new", stdout=subprocess.PIPE, shell=True)
    
        time.sleep(n)         #wait to finish
                
    #Examine old file
    tiptop_output = open ("tiptop.old")  
    for line in tiptop_output:
        for elem in objectlist:
            if elem in line:
                objectoldmap [elem] = line.split()
    tiptop_output.close()
        
    #Examine new file
    tiptop_output = open ("tiptop.new")  
    for line in tiptop_output:
        for elem in objectlist:
            if elem in line:
                objectnewmap [elem] = line.split()
    tiptop_output.close()

    #dump info to file for test suite to read
    pickle.dump((objectoldmap, objectnewmap), open( "tiptop_test.obj", "wb"))
    
    #Run test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(test_tiptop_fd)
    unittest.TextTestRunner(verbosity=2).run(suite)
    
if __name__ == "__main__":
    main()