import unittest
import sys
import random

from PyQt5.QtCore import QPointF, Qt
from PyQt5.QtWidgets import QApplication
from PyQt5.QtTest import QSignalSpy, QTest

import bridgegui.cards as cards
import bridgegui.messaging as messaging


class HandPanelTest(unittest.TestCase):
    """Test suite for hand panel"""

    @classmethod
    def setUpClass(cls):
        random.seed(str(cls))

    def setUp(self):
        self._app = QApplication(sys.argv)
        self._hand_panel = cards.HandPanel(
            vertical=random.choice((True, False)))
        ranks = random.sample(cards.RANK_TAGS, 4)
        suits = random.sample(cards.SUIT_TAGS, 4)
        self._cards = [
            cards.Card(rank, suit) for (rank, suit) in zip(ranks, suits)]

    def testSetInvalidCards(self):
        with self.assertRaises(messaging.ProtocolError):
            self._hand_panel.setCards('invalid')

    def testSetInvalidAllowedCards(self):
        self._hand_panel.setCards(self._cards)
        with self.assertRaises(messaging.ProtocolError):
            self._hand_panel.setAllowedCards('invalid')

    def testPlayAllowedCard(self):
        spy = QSignalSpy(self._hand_panel.cardPlayed)
        self._hand_panel.setCards(self._cards)
        card = random.choice(self._cards)
        self._hand_panel.setAllowedCards((card,))
        self._click_card_helper(card)
        self.assertEqual(spy[0][0], card)

    def testPlayNotAllowedCard(self):
        spy = QSignalSpy(self._hand_panel.cardPlayed)
        self._hand_panel.setCards(self._cards)
        card = random.choice(self._cards)
        self._click_card_helper(card)
        self.assertEqual(len(spy), 0)

    def testPlayNullCard(self):
        spy = QSignalSpy(self._hand_panel.cardPlayed)
        self._hand_panel.setCards([None, None, None])
        card, rect = random.choice(self._hand_panel.cards())
        self.assertIsNone(card)
        point = (rect.topLeft() + QPointF(1, 1)).toPoint()
        QTest.mouseClick(self._hand_panel, Qt.LeftButton, Qt.NoModifier, point)
        self.assertFalse(spy, card)

    def testConfirmPlayCard(self):
        self._hand_panel.setCards(self._cards)
        card, remaining_card = random.sample(self._cards, 2)
        self._hand_panel.playCard(card)
        remaining_cards = [card for (card, _) in self._hand_panel.cards()]
        self.assertNotIn(card, remaining_cards)
        self.assertIn(remaining_card, remaining_cards)

    def testConfirmPlayNullCard(self):
        self._hand_panel.setCards([None, None, None])
        card = random.choice(self._cards)
        self._hand_panel.playCard(card)
        remaining_cards = [card for (card, _) in self._hand_panel.cards()]
        self.assertEqual(remaining_cards, [None, None])

    def _click_card_helper(self, card):
        rects = dict(self._hand_panel.cards())
        point = (rects[card].topLeft() + QPointF(1, 1)).toPoint()
        QTest.mouseMove(self._hand_panel, point)
        QTest.mouseClick(self._hand_panel, Qt.LeftButton, Qt.NoModifier, point)
