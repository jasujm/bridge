import unittest
import sys
import random

from PyQt5.QtCore import QPointF, Qt
from PyQt5.QtWidgets import QApplication
from PyQt5.QtTest import QSignalSpy, QTest

import bridgegui.cards as cards
from bridgegui.cards import RANK_TAGS, SUIT_TAGS
import bridgegui.messaging as messaging
from bridgegui.positions import asPosition, POSITION_TAGS

def _generate_random_cards():
    ranks = random.sample(RANK_TAGS, 4)
    suits = random.sample(SUIT_TAGS, 4)
    return [cards.Card(rank, suit) for (rank, suit) in zip(ranks, suits)]

def _generate_random_card():
    return cards.Card(random.choice(RANK_TAGS), random.choice(SUIT_TAGS))


class HandPanelTest(unittest.TestCase):
    """Test suite for hand panel"""

    @classmethod
    def setUpClass(cls):
        random.seed(str(cls))

    def setUp(self):
        self._app = QApplication(sys.argv)
        self._hand_panel = cards.HandPanel(
            vertical=random.choice((True, False)))
        self._cards = _generate_random_cards()

    def tearDown(self):
        del self._app

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


class TrickPanelTest(unittest.TestCase):
    """Test suite for trick panel"""

    @classmethod
    def setUpClass(cls):
        random.seed(str(cls))

    def setUp(self):
        self._app = QApplication(sys.argv)
        self._trick_panel = cards.TrickPanel()
        self._cards = [
            dict(position=position, card=card) for (position, card) in
            zip(POSITION_TAGS, _generate_random_cards())]
        self._internal_cards = [
            (asPosition(pair[cards.POSITION_TAG]), pair[cards.CARD_TAG]) for
            pair in self._cards]

    def tearDown(self):
        del self._app

    def testInvalidPosition(self):
        with self.assertRaises(messaging.ProtocolError):
            self._trick_panel.setPlayerPosition('invalid')

    def testCards(self):
        self._set_player_position()
        self._trick_panel.setCards(self._cards)
        self.assertEqual(self._trick_panel.cards(), self._internal_cards)

    def testInvalidCards(self):
        self._set_player_position()
        self._cards.append('invalid')
        with self.assertRaises(messaging.ProtocolError):
            self._trick_panel.setCards(self._cards)

    def testPlayCard(self):
        self._set_player_position()
        card_played = self._cards.pop()
        self._trick_panel.setCards(self._cards)
        self._trick_panel.playCard(**card_played)
        self.assertEqual(self._trick_panel.cards(), self._internal_cards)

    def testPlayCardDoesNotAcceptCardIfCardHasBeenPlayed(self):
        self._set_player_position()
        self._trick_panel.setCards(self._cards)
        position = random.choice(POSITION_TAGS)
        card_played = _generate_random_card()
        while card_played == self._internal_cards[asPosition(position)]:
            card_played = _generate_random_card()
        self._trick_panel.playCard(position, card_played)
        self.assertNotEqual(
            card_played, self._trick_panel.cards()[asPosition(position)])

    def testSetEmptyCardsDoesNotImmediatelyClearCards(self):
        self._set_player_position()
        self._trick_panel.setCards(self._cards)
        self._trick_panel.setCards({})
        self.assertEqual(self._trick_panel.cards(), self._internal_cards)

    def testPlayCardClearsPreviousTrick(self):
        self._set_player_position()
        self._trick_panel.setCards(self._cards)
        self._trick_panel.setCards({})
        card_played = (random.choice(POSITION_TAGS), _generate_random_card())
        self._trick_panel.playCard(*card_played)
        self.assertEqual(
            self._trick_panel.cards(),
            [(asPosition(card_played[0]), card_played[1])])

    def _set_player_position(self):
        self._trick_panel.setPlayerPosition(random.choice(POSITION_TAGS))
