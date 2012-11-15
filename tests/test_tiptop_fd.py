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

    def test_found(self):       
        self.assertIsNotNone(self.map1, "Found no entry for objects")            
    
    def test_comparequestionmark(self):
        objectlist = sorted(self.map1.keys())               
        for item in objectlist:
            i = 0
            oldresult = [] 
            newresult = []
            while i < len(self.map1[item]):
                if self.map1[item][i] == '?':
                    self.assertNotEqual('?', self.map2[item][i], "Found ? for both tiptop")
                    oldresult.append(self.map1[item][i])
                    newresult.append(self.map2[item][i])
                    #print "for %s, %s becomes %s" % (item, self.map1[item][i], self.map2[item][i])
                i = i + 1
            if oldresult != []:
                print "for %s, %s becomes %s" % (item, ' '.join(oldresult), ' '.join(newresult))
    
    def test_comparedash(self):
        objectlist = sorted(self.map1.keys())        
        for item in objectlist:
            i = 0
            oldresult = [] 
            newresult = []
            while i < len(self.map1[item]):
                if self.map1[item][i] == '-':
                    self.assertNotEqual('-', self.map2[item][i], "Found - for both tiptop")
                    oldresult.append(self.map1[item][i])
                    newresult.append(self.map2[item][i])
                    #print "for %s, %s becomes %s" % (item, self.map1[item][i], self.map2[item][i])
                i = i + 1
            if oldresult != []:
                print "for %s, %s becomes %s" % (item, ' '.join(oldresult), ' '.join(newresult))
        
    def test_findquestionmark(self):
        objectlist = sorted(self.map2.keys())        
        for item in objectlist:
            self.assertNotIn('?', self.map2[item], "Found ? for %s" % item)

    def suite(self):
        suite = unittest.TestSuite()
        suite.addTest(test_tiptop_fd('test_found'))
        suite.addTest(test_tiptop_fd('test_comparequestionmark'))
        suite.addTest(test_tiptop_fd('test_comparedash'))
        suite.addTest(test_tiptop_fd('test_findquestionmark'))
        return suite

#if __name__ == '__main__':
#    unittest.main()
