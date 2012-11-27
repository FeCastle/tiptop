import unittest
import os
import re

class test_tiptop_thread (unittest.TestCase):
    def test_thread_count(self):
        f = open('tiptop.new')
        contents = f.read()
        f.close()

        for line in contents.split('\n')[6:]:
            bits = re.split('\s+',line.strip())
            if bits[-1]!='./nthreads': continue
            assert int(bits[4]) == 10

def main():   
    n = 5

    # build and spawn nthreads, which spawns 10 threads
    os.system('gcc -o nthreads nthreads.c -lpthread')
    os.system('./nthreads &')

    # run tiptop
    base_newcmd = "../src/tiptop -b -c -n " + str(n) + " -o tiptop.new 2> err.log" 
    os.system(base_newcmd)
                
    #Run test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(test_tiptop_thread)
    unittest.TextTestRunner(verbosity=2).run(suite)
    
if __name__ == "__main__":
    main()
