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

def main():   
    processlist = []
    outputlist = []
    objectlist = []
    objectoldmap = {}
    objectnewmap = {}
    
    old_tiptop = raw_input ("Enter relative path to old tiptop (eg ../src/tiptop): ")
    new_tiptop = raw_input ("Enter relative path to new tiptop: ")
    
    if old_tiptop == '':
        old_tiptop = "../../oldtiptop/tiptop/src/tiptop"
    if new_tiptop == '':
        new_tiptop = "../src/tiptop"
    
    #build tiptop command
    base_oldcmd = old_tiptop + " -b -c -n 2 -o "
    base_newcmd = new_tiptop + " -b -c -n 2 -o "    
    
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
    if argv[1] != 'pass':
        for cmd in processlist:
            subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
 
    #use tiptop to evaluate info
    for output in outputlist:
        subprocess.Popen(base_oldcmd + output + ".old", stdout=subprocess.PIPE, shell=True)
        subprocess.Popen(base_newcmd + output + ".new", stdout=subprocess.PIPE, shell=True)
                
    #Examine old file
    tiptop_output = open (output + ".old")  
    for line in tiptop_output:
        for elem in objectlist:
            if elem in line:
                objectoldmap [elem] = line.split()
    tiptop_output.close()
        
    #Examine new file
    tiptop_output = open (output + ".new")  
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