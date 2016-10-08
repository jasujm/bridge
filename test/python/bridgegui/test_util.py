import unittest

from bridgegui.util import freeze

class UtilTest(unittest.TestCase):
    """Unit test suite for utilities"""

    def testFreeze(self):
        obj = [1, {2: 3, 4: 5}, {6, 7, 8}, (9, 10)]
        frozen = freeze(obj)
        d = {frozen: obj}
        self.assertEqual(d[frozen], obj)
