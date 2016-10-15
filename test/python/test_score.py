import unittest

import bridgegui.score as score


class ScoreTableTest(unittest.TestCase):
    """Test suite for scoresheet"""

    def testScoreTable(self):
        score_table = score.ScoreTable()
        scoresheet = [
            None,
            dict(partnership=score.NORTH_SOUTH_TAG, amount=100),
            dict(partnership=score.EAST_WEST_TAG, amount=200),
        ]
        score_table.setScoreSheet(scoresheet)
        score_items = [
            ("0", "0"),
            ("100", "0"),
            ("0", "200")
        ]
        for row, row_item in enumerate(score_items):
            for col, item in enumerate(row_item):
                self.assertEqual(score_table.item(row, col).text(), item)
