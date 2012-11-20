import unittest
import cPickle as pickle

class test_tiptop_thread (unittest.TestCase):
    """
    Test Tiptop File descriptor limits
    """

    def setUp(self):      
        try:
            (self.map1, self.map2, self.map3) = pickle.load (open( "tiptop_test.obj", "rb"))
        except IOError:
            print "Could not find tiptop data file tiptop_test.obj" 

    def test_got_entry(self):       
        self.assertIsNotNone(self.map1, "Found no entry for objects")            
    
    def test_compare_info(self):
        objectlist = sorted(self.map1.keys())               
        for item in objectlist:
            self.assertNotEqual(len(self.map1[item]), len(self.map2[item]), "Found no thread info")
            print "Old info: %s" % str(self.map1[item])
            print "New info: %s" % str(self.map2[item])
            
    def test_thread_info(self):
        objectlist = sorted(self.map2.keys())               
        for item in objectlist:
            self.assertEqual(12, len(self.map2[item]), "Found no thread info")
            #Get thread info from column 5
            print "for %s, number of threads per process = %s" % (item, self.map2[item][4])
        
    def test_got_threadinfo(self):
        objectlist = sorted(self.map2.keys())     
        #Check if len = 12, meaning got thread information, instead of the old 11   
        for item in objectlist:
            self.assertEqual(12, len(self.map2[item]), "Found no thread info")
            
    def test_noinfo_threadmode(self):
        objectlist = sorted(self.map3.keys())     
        #Check if len = 11, meaning no new thread column in thread mode (-H) 
        for item in objectlist:
            self.assertEqual(11, len(self.map3[item]), "Found thread info in thread mode")            

    def suite(self):
        suite = unittest.TestSuite()
        suite.addTest(test_tiptop_thread('test_got_threadinfo'))
        suite.addTest(test_tiptop_thread('test_noinfo_threadmode'))
        suite.addTest(test_tiptop_thread('test_got_entry'))
        suite.addTest(test_tiptop_thread('test_compare_info'))
        suite.addTest(test_tiptop_thread('test_thread_info'))
        return suite

#if __name__ == '__main__':
#    unittest.main()
