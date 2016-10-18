"""Trick widgets for bridge frontend

This module contains widget that is used to display tricks won by each
partnership.

Classes:
TricksWonLabel -- label for displaying tricks won
"""

from PyQt5.QtWidgets import QLabel

import bridgegui.messaging as messaging
from bridgegui.positions import (
    Partnership, partnershipFor, partnershipLabel, PARTNERSHIP_TAGS)

class TricksWonLabel(QLabel):
    """Label for displaying tricks won by each partnership"""

    def __init__(self, parent=None):
        """Initialize tricks won label

        Keyword Arguments:
        parent -- the parent widget
        """
        super().__init__(parent)
        self._tricks = [0] * len(Partnership)

    def setTricksWon(self, tricksWon):
        """Set tricks won for the deal

        This method accepts a mapping indicating how many tricks have been won
        by each partnership. See bridge protocol specification for the tricks
        won object.

        Keyword Arguments:
        tricksWon -- the tricks won object
        """
        try:
            self._tricks = [int(tricksWon[tag]) for tag in PARTNERSHIP_TAGS]
        except Exception:
            raise messaging.ProtocolError(
                "Invalid tricks won object: %r" % tricksWon)
        self._set_text_helper()

    def addTrick(self, winner):
        """Add single trick

        This method awards a single trick to the player at given position. The
        position can be in either serialized or internal representation.

        Keyword Arguments:
        winner -- the position of the winner
        """
        partnership = partnershipFor(winner)
        self._tricks[partnership] += 1
        self._set_text_helper()

    def _set_text_helper(self):
        # TODO: Localization
        self.setText(
            """Tricks
{northSouthLabel}: {northSouthTricks}
{eastWestLabel}: {eastWestTricks}""".format(
                northSouthLabel=partnershipLabel(Partnership.northSouth),
                northSouthTricks=self._tricks[Partnership.northSouth],
                eastWestLabel=partnershipLabel(Partnership.eastWest),
                eastWestTricks=self._tricks[Partnership.eastWest]
            ))
