"""Score widgets for bridge frontend

Thi module contains widgets for displaying scoresheet.

Classes:
ScoreTable -- table for displaying scoresheet
"""

from PyQt5.QtWidgets import QTableWidget, QTableWidgetItem

import bridgegui.messaging as messaging
import bridgegui.positions as positions

SCORE_TAG = "score"
PARTNERSHIP_TAG = "partnership"


class ScoreTable(QTableWidget):
    """Table displaying scoresheet"""

    def __init__(self, parent=None):
        """Inititalize score table

        Keyword Arguments:
        parent -- the parent widget
        """
        super().__init__(0, len(positions.Partnership), parent)
        self.setHorizontalHeaderLabels(
            positions.partnershipLabel(partnership) for
            partnership in positions.Partnership)
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
            winner = positions.asPartnership(score[PARTNERSHIP_TAG])
            if winner == positions.Partnership.northSouth:
                return (amount, "0")
            else:
                return ("0", amount)
        except Exception:
            raise messaging.ProtocolError("Invalid score: %r" % score)
