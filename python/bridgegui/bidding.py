"""Bidding widgets for bridge frontend

A call is not represented with any dedicated class. Instead, the representation
of a call is an object corresponding to the call representation in the bridge
protocol. This makes it convenient to used deserialized messages directly as
call representation when using classes and functions from this module.
"""

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import (
    QPushButton, QWidget, QGridLayout, QTableWidget, QTableWidgetItem)

from bridgegui.util import freeze

TYPE_TAG = "type"
BID_TAG = "bid"
LEVEL_TAG = "level"
STRAIN_TAG = "strain"
PASS_TAG = "pass"
DOUBLE_TAG = "double"
REDOUBLE_TAG = "redouble"
CLUBS_TAG = "clubs"
DIAMONDS_TAG = "diamonds"
HEARTS_TAG = "hearts"
SPADES_TAG = "spades"
NOTRUMP_TAG = "notrump"
NORTH_TAG = "north"
EAST_TAG = "east"
SOUTH_TAG = "south"
WEST_TAG = "west"

STRAIN_TAGS = (CLUBS_TAG, DIAMONDS_TAG, HEARTS_TAG, SPADES_TAG, NOTRUMP_TAG)
POSITION_TAGS = (NORTH_TAG, EAST_TAG, SOUTH_TAG, WEST_TAG)

# TODO: Localization
CALL_TYPE_FORMATS = { PASS_TAG: 'PASS', DOUBLE_TAG: 'X', REDOUBLE_TAG: 'XX' }
STRAIN_FORMATS = {
    CLUBS_TAG: 'C', DIAMONDS_TAG: 'D', HEARTS_TAG: 'H', SPADES_TAG: 'S',
    NOTRUMP_TAG: 'NT'
}
POSITION_FORMATS = {
    NORTH_TAG: "North", EAST_TAG: "East", SOUTH_TAG: "South", WEST_TAG: "West"
}

def _make_call(type_, **kwargs):
    kwargs.update({ TYPE_TAG: type_ })
    return kwargs

def makePass():
    """Return representation of pass call"""
    return _make_call(PASS_TAG)


def makeBid(level, strain):
    """Return representation of pass call"""
    return _make_call(BID_TAG, bid={ LEVEL_TAG: level, STRAIN_TAG: strain })


def makeDouble():
    """Return representation of pass call"""
    return _make_call(DOUBLE_TAG)


def makeRedouble():
    """Return representation of pass call"""
    return _make_call(REDOUBLE_TAG)


def formatCall(call):
    """Return text representation of call

    If the call structure is invalid, returns None.

    Keyword Arguments:
    call -- object representing the call in call serialization format
    """
    try:
        s = CALL_TYPE_FORMATS.get(call[TYPE_TAG], None)
        if s:
            return s
        bid = call[BID_TAG]
        return "%d%s" % (bid[LEVEL_TAG], STRAIN_FORMATS[bid[STRAIN_TAG]])
    except (TypeError, KeyError):
        return None


class CallPanel(QWidget):
    """Panel used to select calls to be made"""

    callMade = pyqtSignal(dict)

    def __init__(self):
        """Initialize call panel"""
        super().__init__()
        self.setLayout(QGridLayout())
        self._buttons = {}
        for row, level in enumerate(range(1, 8)):
            for col, strain in enumerate(STRAIN_TAGS):
                self._init_call(row, col, makeBid(level, strain))
        self._init_call(8, 0, makePass())
        self._init_call(8, 1, makeDouble())
        self._init_call(8, 2, makeRedouble())

    def setAllowedCalls(self, calls):
        """Set calls that can be made using the panel

        This method enables calls given as argument and disables others.

        Keyword Arguments:
        calls -- an interable containing the allowed calls
        """
        for button in self.buttons():
            button.setEnabled(False)
        for call in calls:
            button = self.getButton(call)
            if not button:
                return False
            button.setEnabled(True)
        return True

    def getButton(self, call):
        """Return button corresponding to the call given as argument"""
        return self._buttons.get(freeze(call), None)

    def buttons(self):
        """Return list of all call buttons"""
        return list(self._buttons.values())

    def _init_call(self, row, col, call):
        button = QPushButton(formatCall(call))
        button.setEnabled(False)
        button.clicked.connect(lambda: self.callMade.emit(call))
        self.layout().addWidget(button, row, col)
        self._buttons[freeze(call)] = button


class CallTable(QTableWidget):
    """Table view used for displaying the calls made"""

    def __init__(self):
        """Initialize call table"""
        super().__init__(0, 4)
        labels = (POSITION_FORMATS[tag] for tag in POSITION_TAGS)
        self.setHorizontalHeaderLabels(labels)
        self._cursor = None

    def addCall(self, position, call):
        try:
            col = POSITION_TAGS.index(position)
        except ValueError: # Not found
            return False
        format_ = formatCall(call)
        if not format_:
            return False
        if self._cursor is None or (
                col == 0 and self._cursor + 1 == self.columnCount()):
            row = self.rowCount()
            self.setRowCount(row + 1)
        elif self._cursor + 1 == col:
            row = self.rowCount() - 1
        else:
            return False
        self._cursor = col
        self.setItem(row, col, QTableWidgetItem(format_))
        return True
