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
formatBid    -- retrieve human readable text representation of bid
formatCall   -- retrieve human readable text representation of call

Classes:
CallPanel -- panel for making calls
CallTable -- table for displaying calls made
"""

from collections import namedtuple

from PyQt5.QtCore import pyqtSignal, Qt
from PyQt5.QtGui import QBrush
from PyQt5.QtWidgets import (
    QPushButton, QWidget, QGridLayout, QLabel, QTableWidget, QTableWidgetItem)

import bridgegui.messaging as messaging
import bridgegui.positions as positions

POSITION_TAG = "position"
CALL_TAG = "call"
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
BID_TAG = "bid"
DOUBLING_TAG = "doubling"
UNDOUBLED_TAG = "undoubled"
DOUBLED_TAG = "doubled"
REDOUBLED_TAG = "redoubled"

TYPE_TAGS = (BID_TAG, PASS_TAG, DOUBLE_TAG, REDOUBLE_TAG)
LEVELS = 7
STRAIN_TAGS = (CLUBS_TAG, DIAMONDS_TAG, HEARTS_TAG, SPADES_TAG, NOTRUMP_TAG)
DOUBLING_TAGS = (UNDOUBLED_TAG, DOUBLED_TAG, REDOUBLED_TAG)

# TODO: Localization
CALL_TYPE_FORMATS = { PASS_TAG: "PASS", DOUBLE_TAG: "X", REDOUBLE_TAG: "XX" }
STRAIN_FORMATS = {
    CLUBS_TAG: "C", DIAMONDS_TAG: "D", HEARTS_TAG: "H", SPADES_TAG: "S",
    NOTRUMP_TAG: "NT"
}
DOUBLING_FORMATS = { UNDOUBLED_TAG: "", DOUBLED_TAG: "X", REDOUBLED_TAG: "XX" }

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


def formatBid(bid):
    """Return human readable text representation of bid"""
    bid = asBid(bid)
    return "%d%s" % (bid.level, STRAIN_FORMATS[bid.strain])


def formatCall(call):
    """Return human readable text representation of call"""
    call = asCall(call)
    s = CALL_TYPE_FORMATS.get(call.type, None)
    if s:
        return s
    return formatBid(call.bid)


class CallPanel(QWidget):
    """Panel used to select calls to be made"""

    callMade = pyqtSignal(dict)

    def __init__(self, parent=None):
        """Initialize call panel

        Keyword Arguments:
        parent -- the parent widget"""
        super().__init__(parent)
        self.setLayout(QGridLayout(self))
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
        button = QPushButton(formatCall(call), self)
        button.setEnabled(False)
        button.clicked.connect(lambda: self.callMade.emit(emit_call))
        self.layout().addWidget(button, row, col)
        self._buttons[call] = button


class CallTable(QTableWidget):
    """Table view used for displaying the calls made"""

    _VULNERABILITY_BRUSHES = {
        False: QBrush(Qt.white),
        True: QBrush(Qt.red)
    }

    def __init__(self, parent=None):
        """Initialize call table

        Keyword Arguments:
        parent -- the parent widget
        """
        super().__init__(0, len(positions.Position), parent)
        self.setHorizontalHeaderLabels(
            positions.positionLabel(position) for
            position in positions.Position)
        self.setVulnerability(
            { tag: False for tag in positions.PARTNERSHIP_TAGS })
        self._reset_calls()

    def setCalls(self, calls):
        """Set multiple call at once

        This method first tries to parse the iterable of position call pairs
        given as arguments. If the list is invalid, raises error. Otherwise
        clears the table and proceeds as if addCall() was called with each pair
        in the argument.
        """
        def _generate_position_call_pair(pair):
            return (
                positions.asPosition(pair[POSITION_TAG]), asCall(pair[CALL_TAG]))
        try:
            new_calls = [_generate_position_call_pair(call) for call in calls]
        except Exception:
            raise messaging.ProtocolError("Invalid calls: %r" % calls)
        self._reset_calls()
        for pair in new_calls:
            self._add_call_helper(pair)

    def addCall(self, position, call):
        """Add call to the table

        Add new call coming from the specified position to the table. Both
        arguments can be either in serialized or internal representation.

        If the call is not in correct order (that is, positions do not follow
        each other clockwise), the call is ignored. Error is raised if the
        position or call does not have correct format (see bridge protocol
        specification).

        Keyword Arguments:
        position -- the position of the player to make the call
        call     -- the call to be added
        """
        self._add_call_helper((positions.asPosition(position), asCall(call)))

    def setVulnerability(self, vulnerability):
        """Set vulnerabilities for partnerships

        Set vulnerability indications (color of header items) according to the
        vulnerability object given as argument. The vulnerability object is a
        mapping from partnership tags to vulnerability status (see bridge
        protocol specification).
        """
        vulnerable_partnerships = set()
        try:
            for tag, partnership in zip(
                    positions.PARTNERSHIP_TAGS, positions.Partnership):
                if vulnerability.get(tag, False):
                    vulnerable_partnerships.add(partnership)
        except Exception:
            raise messaging.ProtocolError(
                "Invalid vulnerability object: %r" % vulnerability)
        for position in positions.Position:
            partnership = positions.partnershipFor(position)
            brush = self._VULNERABILITY_BRUSHES[
                partnership in vulnerable_partnerships]
            self.horizontalHeaderItem(position).setBackground(brush)

    def _reset_calls(self):
        self.clearContents()
        self.setRowCount(0)
        self._cursor = None

    def _add_call_helper(self, position_call_pair):
        col, call = position_call_pair
        if self._cursor is None or (
                col == 0 and self._cursor + 1 == self.columnCount()):
            row = self.rowCount()
            self.setRowCount(row + 1)
        elif self._cursor + 1 == col:
            row = self.rowCount() - 1
        else:
            return
        self._cursor = col
        self.setItem(row, col, QTableWidgetItem(formatCall(call)))


class ResultLabel(QLabel):
    """Label for displaying information about results of the bidding"""

    def __init__(self, parent=None):
        """Initialize result label"""
        super().__init__(parent)

    def setBiddingResult(self, declarer, contract):
        """Set bidding result

        The effect of this method is to set the text of the label to one
        indicating the declarer and the contract reached. If declarer or
        contract is None, the text is cleared.

        Keyword Arguments:
        declarer -- the declarer determined by the bidding
        declarer -- the contract determined by the bidding
        """
        if declarer and contract:
            declarer_format = positions.positionLabel(declarer)
            try:
                bid_format = formatBid(contract[BID_TAG])
                doubling_format = DOUBLING_FORMATS[contract[DOUBLING_TAG]]
            except Exception:
                raise messaging.ProtocolError("Invalid contract: %r" % contract)
            # TODO: Localization
            self.setText(
                "{declarer} declares {bid} {doubling}".format(
                    declarer=declarer_format, bid=bid_format,
                    doubling=doubling_format))
        else:
            self.clear()
