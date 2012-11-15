"""
Usage: python test_tiptop_fd.py 
"""
import unittest
import cPickle as pickle

class test_tiptop_fd (unittest.TestCase):
    """
    Test Tiptop File descriptor limits
    """

    def setUp(self):      
        try:
            (self.map1, self.map2) = pickle.load (open( "tiptop_test.obj", "rb"))
        except IOError:
            print "Could not find tiptop data file tiptop_test.obj" 

    def test_compare(self):
        objectlist = sorted(self.map1.keys())        
        for item in objectlist:
            if self.map1[item] == '?':
                self.assertEqual (self.map1[item], self.map2[item], "Not equal")
        
    def test_findquestionmark(self):
        objectlist = sorted(self.map2.keys())        
        for item in objectlist:
            self.assertNotIn('?', self.map2[item], "Found ? for %s" % item)
            #print self.map2[item]

    def suite(self):
        suite = unittest.TestSuite()
        suite.addTest(test_tiptop_fd('test_compare'))
        suite.addTest(test_tiptop_fd('test_findquestionmark'))
        return suite

#if __name__ == '__main__':
#    unittest.main()
