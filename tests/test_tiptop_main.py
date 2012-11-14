from test_tiptop_fd import test_tiptop_fd
import unittest
import subprocess

def main():   
    
    old_tiptop = raw_input ("Enter relative path to old tiptop (eg ../src/tiptop): ")
    new_tiptop = raw_input ("Enter relative path to new tiptop: ")
    
    base_oldcmd = old_tiptop + " -b -c -n 2 -o "
    base_newcmd = new_tiptop + " -b -c -n 2 -o "
    oldcmdlist = []
    newcmdlist = []
    processlist = []
    
    #read testobjects.txt and build list of commands
    try:
        test_objects = open ("testobjects.txt")
        for line in test_objects:
            processlist.append(line.strip('\n') + " & ")
            oldcmdlist.append(base_oldcmd + line.strip('\n') + ".output")
            newcmdlist.append(base_newcmd + line.strip('\n') + ".output")        
        test_objects.close()
    except IOError:
        print "Missing testobjects.txt"
    
    #Run commands in background 
    for cmd in processlist:
        subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
 
    #use tiptop to evaluate info
    for cmd in oldcmdlist:
        subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
        
    for cmd in newcmdlist:
        subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    
    suite = unittest.TestLoader().loadTestsFromTestCase(test_tiptop_fd)
    unittest.TextTestRunner(verbosity=2).run(suite)
    
if __name__ == "__main__":
    main()