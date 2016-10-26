import os
import unittest
import uuid

import bridgegui.config as config

FILENAME = "_test_config_file"


class ConfigTest(unittest.TestCase):
    """Test suite for configs"""

    def testGetIdentityWithoutFilename(self):
        self.assertIsInstance(config.getIdentity(), uuid.UUID)

    def testGetIdentity(self):
        self._remove_file_if_exists()
        id1 = config.getIdentity(FILENAME)
        self.assertTrue(os.path.isfile(FILENAME))
        id2 = config.getIdentity(FILENAME)
        self._remove_file_if_exists()
        self.assertEqual(id1, id2)

    def _remove_file_if_exists(self):
        if os.path.isfile(FILENAME):
            os.remove(FILENAME)
