"""Card widgets for bridge frontend

This module contains widgets that are used to represent cards in hands and
tricks.

Functions:
asCard -- convert serialized card into internal representation

Classes:
HandPanel -- widget for presenting hand
CardArea  -- widget that holds HandPanel objects for all players
"""

import itertools
from collections import namedtuple

from PyQt5.QtCore import pyqtSignal, QPoint, QRectF, Qt
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

    _IMAGE_WIDTH = next(iter(CARD_IMAGES.values())).width()
    _IMAGE_HEIGHT = next(iter(CARD_IMAGES.values())).height()
    _MARGIN = _IMAGE_WIDTH / 4

    def __init__(self, parent=None, vertical=False):
        """Initialize hand panel

        Keyword Arguments:
        parent    -- the parent widget
        vecrtical -- if True, the cards should be laid out vertically
        """
        super().__init__(parent)
        self.setMouseTracking(True)
        if vertical:
            self.setMinimumSize(
                self._IMAGE_WIDTH, 3 * self._IMAGE_WIDTH + self._IMAGE_HEIGHT)
        else:
            self.setMinimumSize(
                4 * self._IMAGE_WIDTH, self._IMAGE_HEIGHT + self._MARGIN)
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
        for card, x in zip(cards, itertools.count(0, self._MARGIN)):
            image = CARD_IMAGES[card] if card else BACK_IMAGE
            if self._vertical:
                rect = QRectF(
                    0, x, self._IMAGE_WIDTH, self._IMAGE_HEIGHT)
            else:
                rect = QRectF(
                    x, self._MARGIN, self._IMAGE_WIDTH, self._IMAGE_HEIGHT)
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
            self._draw_image(painter, rect, image, n == self._selected_card_n)
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
                    rect = QRectF(
                        (self.width() - self._IMAGE_WIDTH)/2, 0,
                        self._IMAGE_WIDTH, self._IMAGE_HEIGHT)
                    self._selected_card_n = len(self._cards) - n - 1
                    break

    def _draw_image(self, painter, rect, image, shift):
        if shift:
            rect = rect.adjusted(0, -self._MARGIN, 0, -self._MARGIN)
        painter.drawImage(rect, image)
        painter.drawRect(rect)


class CardArea(QWidget):
    """Widget for aggregating hands and trick

    CardArea lays out and manages collectively widgets related to managing
    cards.
    """

    _HAND_POSITIONS = (
        QPoint(HandPanel._IMAGE_WIDTH, 3 * HandPanel._IMAGE_WIDTH + 2 * HandPanel._IMAGE_HEIGHT + HandPanel._MARGIN),
        QPoint(0, HandPanel._IMAGE_HEIGHT + HandPanel._MARGIN),
        QPoint(HandPanel._IMAGE_WIDTH, HandPanel._MARGIN),
        QPoint(5 * HandPanel._IMAGE_WIDTH, HandPanel._IMAGE_HEIGHT + HandPanel._MARGIN),
    )

    def __init__(self):
        """Initlalize card area"""
        super().__init__()
        self.setMinimumSize(
            6 * HandPanel._IMAGE_WIDTH,
            3 * HandPanel._IMAGE_WIDTH + 3 * HandPanel._IMAGE_HEIGHT + 2 * HandPanel._MARGIN)
        self._hand_panels = []
        self._hand_map = {}
        for n, (point, position) in enumerate(
                zip(self._HAND_POSITIONS, POSITION_TAGS)):
            hand_panel = HandPanel(self, n % 2 == 1)
            hand_panel.move(point)
            self._hand_panels.append(hand_panel)

    def setPlayerPosition(self, position):
        """Set position of the current player

        The player must be set before setting cards. Cards for the current
        player are laid at the bottom of the area.
        """
        self._position = position
        n = POSITION_TAGS.index(position)
        self._hand_map = {}
        for hand in self._hand_panels:
            self._hand_map[POSITION_TAGS[n]] = hand
            n = (n + 1) % len(self._hand_panels)

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

    def hands(self):
        """Return tuple containing all HandPanel objects"""
        return tuple(self._hand_panels)
