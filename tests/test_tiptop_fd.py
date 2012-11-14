from sys import argv

"""
Usage: python test_tiptop_fd.py 
"""
import random
import unittest

class test_tiptop_fd (unittest.TestCase):
    """
    Test Tiptop File descriptor limits
    """

    def setUp(self):        
        self.seq = range(10)

    def test_compare(self, map1, map2):
        objectlist = sorted(map1.keys())        
        for item in objectlist:
            self.assertEqual (map1[item], map2[item])
        
    def test_findquestionmark(self, map2):
        objectlist = sorted(map2.keys())        
        for item in objectlist:
            self.assertNotIn('?', map2[item], "Found no ?")

    def test_sample(self):
        with self.assertRaises(ValueError):
            random.sample(self.seq, 20)
        for element in random.sample(self.seq, 5):
            self.assertTrue(element in self.seq)

#if __name__ == '__main__':
#    unittest.main()
