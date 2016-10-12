"""Bidding widgets for bridge frontend

This module contains widgets that are used to make and display calls made during
the bidding phase.

Functions:
asBid        -- convert serialized bid into internal representation
asCall       -- convert serialized bid into internal representation
makePass     -- make pass call object
makeBid      -- make bid call object
makeDouble   -- make double call object
makeRedouble -- make redouble call object
formatCall   -- retrieve human readable text representation of call

Classes:
CallPanel -- panel for making calls
CallTable -- table for displaying calls made
"""

from collections import namedtuple

from PyQt5.QtCore import pyqtSignal
from PyQt5.QtWidgets import (
    QPushButton, QWidget, QGridLayout, QTableWidget, QTableWidgetItem)

import bridgegui.messaging as messaging
import bridgegui.positions as positions

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

TYPE_TAGS = (BID_TAG, PASS_TAG, DOUBLE_TAG, REDOUBLE_TAG)
LEVELS = 7
STRAIN_TAGS = (CLUBS_TAG, DIAMONDS_TAG, HEARTS_TAG, SPADES_TAG, NOTRUMP_TAG)

# TODO: Localization
CALL_TYPE_FORMATS = { PASS_TAG: 'PASS', DOUBLE_TAG: 'X', REDOUBLE_TAG: 'XX' }
STRAIN_FORMATS = {
    CLUBS_TAG: 'C', DIAMONDS_TAG: 'D', HEARTS_TAG: 'H', SPADES_TAG: 'S',
    NOTRUMP_TAG: 'NT'
}
POSITION_FORMATS = {
    positions.NORTH_TAG: "North", positions.EAST_TAG: "East",
    positions.SOUTH_TAG: "South", positions.WEST_TAG: "West"
}

Bid = namedtuple("Bid", [LEVEL_TAG, STRAIN_TAG])
Call = namedtuple("Call", [TYPE_TAG, BID_TAG])

def _make_call(type_, **kwargs):
    kwargs.update({ TYPE_TAG: type_ })
    return kwargs


def asBid(bid):
    """Convert serialized bid representation into Bid object

    The serialized representation is a dictionary containing level and strain
    keys (see bridge protocol specification). This function converts the
    serialized representation into the representation used by this module, which
    is a named tuple containing level and strain fields (in this order).

    This function also does validation of the serialized bid, raising
    ProtocolError if the format is not valid.

    This function is idempotent, meaning that it is safe to call it with Bid
    object.

    Keyword Arguments:
    bid -- bid object
    """
    if isinstance(bid, Bid):
        return bid
    try:
        bid = Bid(bid[LEVEL_TAG], bid[STRAIN_TAG])
    except Exception:
        raise messaging.ProtocolError("Invalid bid: %r" % bid)
    if not (0 <= bid.level <= LEVELS) or bid.strain not in STRAIN_TAGS:
        raise messaging.ProtocolError(
            "Invalid level or strain in bid: %r" % bid)
    return bid


def asCall(call):
    """Convert serialized call representation into Call object

    The serialized representation is a dictionary containing type and bid
    keys (see bridge protocol specification). This function converts the
    serialized representation into the representation used by this module, which
    is a named tuple containing type and bid fields (in this order).

    This function also does validation of the serialized call (and recursively
    the bid if present), raising ProtocolError if the format is not valid.

    This function is idempotent, meaning that it is safe to call it with Call
    object.

    Keyword Arguments:
    call -- call object
    """
    if isinstance(call, Call):
        return call
    try:
        type_ = call[TYPE_TAG]
        if type_ == BID_TAG:
            return Call(type_, asBid(call[BID_TAG]))
    except Exception:
        raise messaging.ProtocolError("Invalid call: %r" % call)
    if type_ not in TYPE_TAGS:
        raise messaging.ProtocolError(
            "Invalid type in bid: %r" % bid)
    return Call(type_, None)


def makePass():
    """Return Call object representing pass"""
    return Call(PASS_TAG, None)


def makeBid(level, strain):
    """Return Call object representing bid

    The function accepts level and strain as arguments
    """
    return Call(BID_TAG, Bid(level, strain))


def makeDouble():
    """Return Call object representing double"""
    return Call(DOUBLE_TAG, None)


def makeRedouble():
    """Return Call object representing redouble"""
    return Call(REDOUBLE_TAG, None)


def formatCall(call):
    """Return text representation of call

    Keyword Arguments:
    call -- object representing the call in call serialization format
    """
    s = CALL_TYPE_FORMATS.get(call.type, None)
    if s:
        return s
    return "%d%s" % (call.bid.level, STRAIN_FORMATS[call.bid.strain])


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

        This method enables calls given as argument and disables others. The
        calls can be either in serialized representation or Call objects.

        Keyword Arguments:
        calls -- an interable containing the allowed calls
        """
        for button in self.buttons():
            button.setEnabled(False)
        for call in calls:
            button = self.getButton(asCall(call))
            button.setEnabled(True)

    def getButton(self, call):
        """Return button corresponding to the call given as argument"""
        return self._buttons[asCall(call)]

    def buttons(self):
        """Return list of all call buttons"""
        return list(self._buttons.values())

    def _init_call(self, row, col, call):
        emit_call = { TYPE_TAG: call.type }
        if call.bid is not None:
            emit_call[BID_TAG] = call.bid._asdict()
        button = QPushButton(formatCall(call))
        button.setEnabled(False)
        button.clicked.connect(lambda: self.callMade.emit(emit_call))
        self.layout().addWidget(button, row, col)
        self._buttons[call] = button


class CallTable(QTableWidget):
    """Table view used for displaying the calls made"""

    def __init__(self):
        """Initialize call table"""
        super().__init__(0, 4)
        labels = (POSITION_FORMATS[tag] for tag in positions.POSITION_TAGS)
        self.setHorizontalHeaderLabels(labels)
        self._cursor = None

    def addCall(self, position, call):
        """Add call to the table

        Add new call coming from the specified position to the table. The call
        can be either in serialized representation or Call objects. The position
        is string.

        Keyword Arguments:
        position -- the position of the player to make the call
        call     -- the call to be added
        """
        try:
            col = positions.POSITION_TAGS.index(position)
        except ValueError:
            raise messaging.ProtocolError(
                "Trying to add call with invalid position: %r" % position)
        format_ = formatCall(asCall(call))
        if self._cursor is None or (
                col == 0 and self._cursor + 1 == self.columnCount()):
            row = self.rowCount()
            self.setRowCount(row + 1)
        elif self._cursor + 1 == col:
            row = self.rowCount() - 1
        else:
            raise messaging.ProtocolError(
                "Trying to add call to invalid column: %d" % col)
        self._cursor = col
        self.setItem(row, col, QTableWidgetItem(format_))
