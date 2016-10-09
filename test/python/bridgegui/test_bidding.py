import unittest
import sys
import random

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QApplication
from PyQt5.QtTest import QSignalSpy

import bridgegui.bidding as bidding

CALLS = [
    bidding.makePass(),
    bidding.makeDouble(),
    bidding.makeRedouble(),
    bidding.makeBid(1, bidding.CLUBS_TAG),
    bidding.makeBid(7, bidding.NOTRUMP_TAG),
]


class CallPanelTest(unittest.TestCase):
    """Test suite for bidding"""

    @classmethod
    def setUpClass(cls):
        random.seed(str(cls))

    def setUp(self):
        self._app = QApplication(sys.argv)
        self._call_panel = bidding.CallPanel()

    def testCallPanelHasButtonsForAllCalls(self):
        button_texts = {button.text() for button in self._call_panel.buttons()}
        for call in CALLS:
            format_ = bidding.formatCall(call)
            self.assertIn(format_, button_texts)

    def testSetAllowedCalls(self):
        allowed, not_allowed = random.sample(CALLS, 2)
        self._call_panel.setAllowedCalls((allowed,))
        self.assertTrue(self._call_panel.getButton(allowed).isEnabled())
        self.assertFalse(self._call_panel.getButton(not_allowed).isEnabled())

    def testMakeCall(self):
        spy = QSignalSpy(self._call_panel.callMade)
        call = random.choice(CALLS)
        button = self._call_panel.getButton(call)
        button.clicked.emit()
        self.assertEqual(spy[0][0], call)
