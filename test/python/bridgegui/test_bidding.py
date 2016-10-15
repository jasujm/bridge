import unittest
import sys
import random

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import QApplication
from PyQt5.QtTest import QSignalSpy

import bridgegui.bidding as bidding
import bridgegui.messaging as messaging
from bridgegui.positions import POSITION_TAGS

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

    def testSetAllowedCallsWithInvalidCall(self):
        with self.assertRaises(messaging.ProtocolError):
            self._call_panel.setAllowedCalls(('invalid',))

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
        self.assertEqual(bidding.asCall(spy[0][0]), call)


class CallTableTest(unittest.TestCase):
    """Test suite for call table"""

    @classmethod
    def setUpClass(cls):
        random.seed(str(cls))

    def setUp(self):
        self._app = QApplication(sys.argv)
        self._call_table = bidding.CallTable()

    def testItHasOneColumnForEachPosition(self):
        self.assertEqual(
            self._call_table.columnCount(), len(POSITION_TAGS))

    def testAddCallInvalidPosition(self):
        call = random.choice(CALLS)
        with self.assertRaises(messaging.ProtocolError):
            self._call_table.addCall('invalid', call)

    def testAddCallInvalidCall(self):
        position = random.choice(POSITION_TAGS)
        with self.assertRaises(messaging.ProtocolError):
            self._call_table.addCall(position, 'invalid')

    def testAddSingleCall(self):
        rows = 0,
        ns = random.randrange(len(POSITION_TAGS)),
        self._add_calls_and_assert_success(rows, ns)

    def testAddMultipleCalls(self):
        rows = 0, 0
        ns = 1, 2
        self._add_calls_and_assert_success(rows, ns)

    def testAddCallsToMultipleRows(self):
        rows = 0, 1
        ns = len(POSITION_TAGS)-1, 0
        self._add_calls_and_assert_success(rows, ns)

    def testAddCallsOutOfOrder(self):
        position1, position2 = reversed(POSITION_TAGS[1:3])
        call1, call2 = random.sample(CALLS, 2)
        self._call_table.addCall(position1, call1)
        self._call_table.addCall(position2, call2)
        row = self._call_table.rowCount() - 1
        col = POSITION_TAGS.index(position2)
        self.assertIsNone(self._call_table.item(row, col))

    def testSetCalls(self):
        calls = random.sample(CALLS, 4)
        formats = tuple(bidding.formatCall(call) for call in calls)
        self._call_table.setCalls(
            dict(position=position, call=call) for (position, call) in
            zip(POSITION_TAGS, calls))
        for col, format_ in enumerate(formats):
            item = self._call_table.item(0, col)
            self.assertEqual(item.text(), format_)

    def testSetCallsWithInvalidPosition(self):
        calls = random.sample(CALLS, 4)
        formats = tuple(bidding.formatCall(call) for call in calls)
        with self.assertRaises(messaging.ProtocolError):
            self._call_table.setCalls(
                dict(position=position, call=call) for (position, call) in
                zip(list(POSITION_TAGS[:3]) + ['invalid'], calls))

    def testSetCallsWithInvalidCall(self):
        calls = random.sample(CALLS, 3)
        formats = tuple(bidding.formatCall(call) for call in calls)
        with self.assertRaises(messaging.ProtocolError):
            self._call_table.setCalls(
                dict(position=position, call=call) for (position, call) in
                zip(POSITION_TAGS, calls + ['invalid']))

    def _add_calls_and_assert_success(self, rows, ns):
        positions = tuple(POSITION_TAGS[n] for n in ns)
        calls = random.sample(CALLS, 2)
        formats = tuple(bidding.formatCall(call) for call in calls)
        for row, n, position, call, format_ in zip(
                rows, ns, positions, calls, formats):
            self._call_table.addCall(position, call)
            item = self._call_table.item(row, n)
            self.assertEqual(item.text(), format_)
