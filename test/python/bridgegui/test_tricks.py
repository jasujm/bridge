import random
import sys
import unittest

from PyQt5.Qt import QApplication

import bridgegui.messaging as messaging
import bridgegui.positions as positions
import bridgegui.tricks as tricks


class TricksWonLabelTest(unittest.TestCase):
    """Test suite for tricks won label"""

    @classmethod
    def setUpClass(cls):
        random.seed(str(cls))

    def setUp(self):
        self._app = QApplication(sys.argv)
        self._tricks_won_label = tricks.TricksWonLabel()
        self._tricks_won = {
            tag: random.randrange(13) for tag in positions.PARTNERSHIP_TAGS }

    def tearDown(self):
        del self._app

    def testSetTricksWon(self):
        self._tricks_won_label.setTricksWon(self._tricks_won)
        for partnership in positions.Partnership:
            self._assert_text_helper(partnership)

    def testSetTricksWonInvalidObject(self):
        with self.assertRaises(messaging.ProtocolError):
            self._tricks_won_label.setTricksWon('invalid')

    def testAddTrick(self):
        position = random.choice(positions.POSITION_TAGS)
        partnership = positions.partnershipFor(position)
        self._tricks_won_label.setTricksWon(self._tricks_won)
        self._tricks_won_label.addTrick(position)
        self._assert_text_helper(partnership, 1)

    def testAddTrickInvalid(self):
        self._tricks_won_label.setTricksWon(self._tricks_won)
        with self.assertRaises(messaging.ProtocolError):
            self._tricks_won_label.addTrick('invalid')

    def _assert_text_helper(self, partnership, extra=0):
        label = "%s: %d" % (
            positions.partnershipLabel(partnership),
            self._tricks_won[positions.PARTNERSHIP_TAGS[partnership]] + extra)
        self.assertIn(label, self._tricks_won_label.text())
