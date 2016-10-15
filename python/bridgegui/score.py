"""Score widgets for bridge frontend

Thi module contains widgets for displaying scoresheet.

Classes:
ScoreTable -- table for displaying scoresheet
"""

from PyQt5.QtWidgets import QTableWidget, QTableWidgetItem

import bridgegui.messaging as messaging

PARTNERSHIP_TAG = "partnership"
NORTH_SOUTH_TAG = "northSouth"
EAST_WEST_TAG = "eastWest"
SCORE_TAG = "score"

PARTNERSHIP_TAGS = NORTH_SOUTH_TAG, EAST_WEST_TAG

# TODO: Localization
PARTNERSHIP_FORMATS = {
    NORTH_SOUTH_TAG: "North South", EAST_WEST_TAG: "East West"
}


class ScoreTable(QTableWidget):
    """Table displaying scoresheet"""

    def __init__(self, parent=None):
        """Inititalize score table

        Keyword Arguments:
        parent -- the parent widget
        """
        super().__init__(0, len(PARTNERSHIP_TAGS), parent)
        labels = (PARTNERSHIP_FORMATS[tag] for tag in PARTNERSHIP_TAGS)
        self.setHorizontalHeaderLabels(labels)
        self.setMinimumWidth(
            5 + self.verticalHeader().width() +
            sum(self.columnWidth(n) for n in range(self.columnCount())))

    def setScoreSheet(self, scoresheet):
        """Set scoresheet

        Sets scoresheet from given argument. The scoresheet is a list containing
        one entry for each deal (see bridge protocol specification). This method
        set one table row for each entry, assigning the score to correct
        partnership or (in case of passed out deal) indicates that no score was
        awarded.
        """
        new_scores = [self._generate_score_tuple(score) for score in scoresheet]
        self.clearContents()
        self.setRowCount(len(new_scores))
        for row, score in enumerate(new_scores):
            for col, amount in enumerate(score):
                self.setItem(row, col, QTableWidgetItem(amount))

    def _generate_score_tuple(self, score):
        if score is None:
            return ("0", "0")
        try:
            amount = str(score[SCORE_TAG])
            winner = PARTNERSHIP_TAGS.index(score[PARTNERSHIP_TAG])
            return (amount, "0") if winner == 0 else ("0", amount)
        except Exception:
            raise messaging.ProtocolError("Invalid score: %r" % score)
