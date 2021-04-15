
import unittest

import tohil


class TestTcleDict(unittest.TestCase):
    def test_tcldict1(self):
        """exercise tohil.tcldict data type"""
        x = tohil.tcldict("a 1 b 2 c 3 d 4 e 5")

        self.assertEqual("c" in x, True)
        self.assertEqual("f" in x, False)

    def test_tcldict2(self):
        """exercise tohil.tcldict item access"""
        x = tohil.tcldict("a 1 b 2 c 3 d 4 e 5")

        self.assertEqual(x['c'], "3")
        self.assertEqual(len(x), 5)

        with self.assertRaises(KeyError):
            x['zzz']

    def test_tcldict3(self):
        """exercise tohil.tcldict item insertion and modification"""
        x = tohil.tcldict("a 1 b 2 c 3 d 4 e 5")

        x['d'] = '42'

        self.assertEqual(x['c'], '3')
        self.assertEqual(x['d'], '42')
        self.assertEqual(len(x), 5)

        x['f'] = '6'
        self.assertEqual(x['f'], '6')
        self.assertEqual(len(x), 6)


if __name__ == "__main__":
    unittest.main()
