"""Card widgets for bridge frontend

This module contains widgets that are used to represent cards in hands and
tricks.

Functions:
asCard -- convert serialized card into internal representation

Classes:
HandPanel  -- widget for presenting hand
TrickPanel -- widget for presenting trick
CardArea   -- widget that holds HandPanel and TrickPanel objects
"""

import itertools
from collections import namedtuple

from PyQt5.QtCore import pyqtSignal, QPoint, QRectF, Qt, QSize, QTimer
from PyQt5.QtGui import QPainter
from PyQt5.QtWidgets import QGridLayout, QWidget

import bridgegui.messaging as messaging
import bridgegui.positions as positions
import bridgegui.util as util
from bridgegui.positions import POSITION_TAGS

RANK_TAG = "rank"
SUIT_TAG = "suit"

RANK_TAGS = (
    "2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen", "king",
    "ace"
)
SUIT_TAGS = ("clubs", "diamonds", "hearts", "spades")

Card = namedtuple("Card", (RANK_TAG, SUIT_TAG))

CARD_IMAGES = {}
for rank, suit in itertools.product(RANK_TAGS, SUIT_TAGS):
    card = Card(rank, suit)
    CARD_IMAGES[card] = util.getImage("%s_of_%s.png" % (rank, suit))
BACK_IMAGE = util.getImage("back.png")

_IMAGE_WIDTH = next(iter(CARD_IMAGES.values())).width()
_IMAGE_HEIGHT = next(iter(CARD_IMAGES.values())).height()
_MARGIN = _IMAGE_WIDTH / 4

def _rotate_positions(position):
    try:
        n = POSITION_TAGS.index(position)
    except:
        raise messaging.ProtocolError("Invalid position: %r" % position)
    return POSITION_TAGS[n:] + POSITION_TAGS[:n]

def _draw_image(painter, rect, image, shift=False):
    if shift:
        rect = rect.adjusted(0, -_MARGIN, 0, -_MARGIN)
    painter.drawImage(rect, image)
    painter.drawRect(rect)


def asCard(card):
    """Convert serialized card representation into Card object

    The serialized representation is a dictionary containing rank and suit keys
    (see bridge protocol specification). This function converts the serialized
    representation into the representation used by this module, which is a named
    tuple containing rank and suit fields (in this order).

    This function also does validation of the serialized card, raising
    ProtocolError if the format is not valid.

    This function is idempotent, meaning that it is safe to call it with Card
    object.

    Keyword Arguments:
    card -- card object
    """
    if isinstance(card, Card):
        return card
    try:
        card = Card(card[RANK_TAG], card[SUIT_TAG])
    except Exception:
        raise messaging.ProtocolError("Invalid card: %r" % card)
    if card.rank not in RANK_TAGS or card.suit not in SUIT_TAGS:
        raise messaging.ProtocolError("Invalid rank or suit in card: %r" % card)
    return card


class HandPanel(QWidget):
    """Widget for presenting hand"""

    cardPlayed = pyqtSignal(Card)

    _VERTICAL_SIZE = QSize(_IMAGE_WIDTH + 1, _IMAGE_HEIGHT + 12 * _MARGIN + 1)
    _HORIZONTAL_SIZE = QSize(
        _IMAGE_WIDTH + 12 * _MARGIN + 1, _IMAGE_HEIGHT + _MARGIN + 1)

    def __init__(self, parent=None, vertical=False):
        """Initialize hand panel

        Keyword Arguments:
        parent    -- the parent widget
        vecrtical -- if True, the cards should be laid out vertically
        """
        super().__init__(parent)
        self.setMouseTracking(True)
        self.setMinimumSize(
            self._VERTICAL_SIZE if vertical else self._HORIZONTAL_SIZE)
        self._vertical = vertical
        self._cards = []
        self._allowed_cards = set()
        self._selected_card_n = None

    def setCards(self, cards):
        """Set the cards held

        This method accepts an iterable containing cards as its argument. The
        iterable may return the cards either in serialized representation or
        Card objects. After the call, the cards held by the panel are the ones
        in the argument, sorted by suit and rank.
        """
        def get_key(card):
            if card:
                return (SUIT_TAGS.index(card.suit), RANK_TAGS.index(card.rank))
            else:
                return (len(SUIT_TAGS), len(RANK_TAGS))
        try:
            cards = list(card and asCard(card) for card in cards)
        except Exception:
            raise messaging.ProtocolError("Invalid cards: %r" % cards)
        cards.sort(key=get_key)
        new_cards = []
        for card, x in zip(cards, itertools.count(0, _MARGIN)):
            image = CARD_IMAGES[card] if card else BACK_IMAGE
            point = (0, x) if self._vertical else (x, _MARGIN)
            rect = QRectF(*point, _IMAGE_WIDTH, _IMAGE_HEIGHT)
            new_cards.append((card, rect, image))
        self._cards, self._allowed_cards = new_cards, set()
        self.repaint()

    def setAllowedCards(self, cards):
        """Set cards that are allowed to be played"""
        try:
            self._allowed_cards = {asCard(card) for card in cards}
            self.repaint()
        except:
            raise messaging.ProtocolError("Invalid allowed cards: %r" % cards)

    def playCard(self, card):
        """Confirm that card has been played

        Clicking a card that can be played emits a signal but does not play card
        directly. This method confirms that a card has indeed been played (after
        receiving play event from the server). The effect of the method is to
        remove the card from the panel, or if the card is not visible, one of
        the hidden cards.

        Keyword Arguments:
        card -- the card played
        """
        card = asCard(card)
        pop_n = None
        for n, card_ in enumerate(self._cards):
            if card_[0] == card:
                pop_n = n
                break
            elif card_[0] is None:
                pop_n = n
        if pop_n is not None:
            self._cards.pop(pop_n)
            self.repaint()

    def cards(self):
        """Return list of cards and their rectangles

        This method generates list of tuples containing cards and their
        corresponding rectangles, respectively, inside the panel.
        """
        return [(card, rect) for (card, rect, _) in self._cards]

    def paintEvent(self, event):
        """Paint cards"""
        painter = QPainter()
        painter.begin(self)
        for n, (card, rect, image) in enumerate(self._cards):
            if self._allowed_cards and card not in self._allowed_cards:
                painter.setOpacity(0.8)
            else:
                painter.setOpacity(1)
            _draw_image(painter, rect, image, n == self._selected_card_n)
        painter.end()

    def mouseMoveEvent(self, event):
        """Handle mouse move event"""
        self._determine_selected_card(event)
        self.repaint()

    def mousePressEvent(self, event):
        """Handle mouse press event"""
        self._determine_selected_card(event)
        if self._selected_card_n is not None:
            self.cardPlayed.emit(self._cards[self._selected_card_n][0])
            self._allowed_cards = set()
            self._selected_card_n = None

    def _determine_selected_card(self, event):
        self._selected_card_n = None
        pos = event.pos()
        for n, (card, rect, image) in enumerate(reversed(self._cards)):
            if rect.contains(pos):
                if card in self._allowed_cards:
                    self._selected_card_n = len(self._cards) - n - 1
                    break


class TrickPanel(QWidget):
    """Widget for presenting trick"""

    _CARD_RECTS = (
        QRectF(
            _IMAGE_WIDTH + _MARGIN, _IMAGE_HEIGHT + _MARGIN,
            _IMAGE_WIDTH, _IMAGE_HEIGHT),
        QRectF(
            0, _IMAGE_HEIGHT / 2 + _MARGIN, _IMAGE_WIDTH, _IMAGE_HEIGHT),
        QRectF(
            _IMAGE_WIDTH + _MARGIN, 0, _IMAGE_WIDTH, _IMAGE_HEIGHT),
        QRectF(
            2 * _IMAGE_WIDTH + 2 * _MARGIN, _IMAGE_HEIGHT / 2 + _MARGIN,
            _IMAGE_WIDTH, _IMAGE_HEIGHT),
    )

    _SIZE = QSize(
        _CARD_RECTS[3].x() + _IMAGE_WIDTH + 1,
        _CARD_RECTS[0].y() + _IMAGE_HEIGHT + 1)

    def __init__(self, parent=None):
        """Initialize trick panel

        Keyword Arguments:
        parent -- the parent widget
        """
        super().__init__(parent)
        self.setMinimumSize(self._SIZE)
        self._rect_map = {}
        self._cards = {}
        self._card_rects = []
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(2000)
        self._timer.timeout.connect(self._clear_cards)

    def setPlayerPosition(self, position):
        """Set position of the current player

        The player must be set before setting cards in the trick. The card
        played by the current player is laid at the bottom of the panel.
        """
        self._rect_map = dict(zip(_rotate_positions(position), self._CARD_RECTS))

    def setCards(self, cards):
        """Set cards in the trick

        This method accepts as argument a mapping from positions to the cards
        played by the player in given position. See bridge protocol
        specification.

        If the argument is empty (i.e. clearing the trick), the cards are
        removed after a delay to give the players an opportunity to see the last
        card played to the trick.
        """
        new_cards = {}
        new_card_rects = []
        for position in POSITION_TAGS:
            if position in cards and position in self._rect_map:
                card = asCard(cards[position])
                new_cards[position] = card
                new_card_rects.append(self._get_card_rect(position, card))
        if new_cards:
            self._cards, self._card_rects = new_cards, new_card_rects
            self._timer.stop()
            self.repaint()
        elif self._cards:
            self._timer.start()

    def playCard(self, position, card):
        """Play single card to the trick

        This method puts card to given position, given that the position is
        empty or the timer period for clearing cards is ongoing.

        Keyword Arguments:
        position -- the position of the player who plays the card
        card     -- the card played
        """
        if position not in self._cards or self._timer.isActive():
            if self._timer.isActive():
                self._clear_cards()
                self._timer.stop()
            card = asCard(card)
            self._cards[position] = card
            self._card_rects.append(self._get_card_rect(position, card))
            self._timer.stop()
            self.repaint()

    def cards(self):
        """Return dict containing mapping between positions and players"""
        return dict(self._cards)

    def paintEvent(self, event):
        """Paint cards"""
        painter = QPainter()
        painter.begin(self)
        for (rect, image) in self._card_rects:
            _draw_image(painter, rect, image)

    def _get_card_rect(self, position, card):
        try:
            rect = self._rect_map[position]
        except Exception:
            raise messaging.ProtocolError("Invalid position: %r" % position)
        return (rect, CARD_IMAGES[card])

    def _clear_cards(self):
        self._cards.clear()
        del self._card_rects[:]
        self.repaint()


class CardArea(QWidget):
    """Widget for aggregating hands and trick

    CardArea lays out and manages collectively widgets related to managing
    cards.
    """

    _HAND_POSITIONS = (
        QPoint(
            HandPanel._VERTICAL_SIZE.width(),
            (HandPanel._HORIZONTAL_SIZE.height() +
             HandPanel._VERTICAL_SIZE.height() + _MARGIN)),
        QPoint(0, HandPanel._HORIZONTAL_SIZE.height() + _MARGIN),
        QPoint(HandPanel._VERTICAL_SIZE.width(), 0),
        QPoint(
            (HandPanel._HORIZONTAL_SIZE.width() +
             HandPanel._VERTICAL_SIZE.width()),
            HandPanel._HORIZONTAL_SIZE.height() + _MARGIN)
    )

    _SIZE = QSize(
        _HAND_POSITIONS[3].x() + HandPanel._VERTICAL_SIZE.width(),
        _HAND_POSITIONS[0].y() + HandPanel._HORIZONTAL_SIZE.height())

    def __init__(self, parent=None):
        """Initialize card area

        Keyword Arguments:
        parent  -- the parent widget"""
        super().__init__(parent)
        self.setMinimumSize(self._SIZE)
        self._hand_panels = []
        self._hand_map = {}
        for n, (point, position) in enumerate(
                zip(self._HAND_POSITIONS, POSITION_TAGS)):
            hand_panel = HandPanel(self, n % 2 == 1)
            hand_panel.move(point)
            self._hand_panels.append(hand_panel)
        self._trick_panel = TrickPanel(self)
        self._trick_panel.move(
            (self._SIZE.width() - TrickPanel._SIZE.width()) / 2,
            (self._SIZE.height() - TrickPanel._SIZE.height()) / 2)

    def setPlayerPosition(self, position):
        """Set position of the current player

        The player must be set before setting cards. Cards for the current
        player are laid at the bottom of the area.
        """
        self._position = position
        hand_map = {}
        for pos, hand in zip(_rotate_positions(position), self._hand_panels):
            hand_map[pos] = hand
        self._hand_map = hand_map
        self._trick_panel.setPlayerPosition(position)

    def setCards(self, cards):
        """Set cards for all players

        This method accepts as its argument a mapping between positions and
        lists of cards held by the players in those positions. The effect of the
        method is to set the cards for all players based on the mapping.
        """
        if not isinstance(cards, dict):
            raise messaging.ProtocolError("Invalid cards format: %r", cards)
        for position, cards_for_position in cards.items():
            if position in self._hand_map:
                self._hand_map[position].setCards(cards_for_position)

    def setAllowedCards(self, cards):
        """Set allowed cards for current player

        This method accepts as its argument a list of allowed cards. It is
        assumed that the player is only allowed to play cards that are in his
        hand or (given that the partner is dummy) or the hand of his partner.
        """
        for position in (self._position, positions.partner(self._position)):
            if position in self._hand_map:
                self._hand_map[position].setAllowedCards(cards)

    def setTrick(self, cards):
        self._trick_panel.setCards(cards)

    def playCard(self, position, card):
        """Confirm that a card has been played by the player in the position

        The effect of this method is to call playCard() for the correct
        HandPanel (as determined by position).

        Keyword Arguments:
        position -- the position of the player
        card     -- the card played by the player
        """
        if position in self._hand_map:
            self._hand_map[position].playCard(card)
        self._trick_panel.playCard(position, card)

    def hands(self):
        """Return list containing all HandPanel objects"""
        return list(self._hand_panels)
