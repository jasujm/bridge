"""Card widgets for bridge frontend

This module contains widgets that are used to represent cards in hands and
tricks.

Functions:
asCard -- convert serialized card into internal representation

Classes:
HandPanel -- widget for presenting hand
"""

import itertools
from collections import namedtuple

from PyQt5.QtCore import QRectF, Qt
from PyQt5.QtGui import QPainter
from PyQt5.QtWidgets import QWidget

import bridgegui.messaging as messaging
import bridgegui.util as util

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

    _IMAGE_WIDTH = next(iter(CARD_IMAGES.values())).width()
    _IMAGE_HEIGHT = next(iter(CARD_IMAGES.values())).height()
    _MARGIN = _IMAGE_WIDTH / 4

    def __init__(self):
        """Initialize hand panel"""
        super().__init__()
        self.setMinimumSize(4 * self._IMAGE_WIDTH, self._IMAGE_HEIGHT)
        self._cards = []

    def addCards(self, cards):
        """Add cards

        This method accepts an iterable containing cards as its argument. The
        iterable may return the cards either in serialized representation or
        Card objects. After the call, the cards held by the panel are the ones
        in the argument, sorted by suit and rank.
        """
        def get_key(card):
            return (SUIT_TAGS.index(card.suit), RANK_TAGS.index(card.rank))
        cards = list(asCard(card) for card in cards)
        cards.sort(key=get_key)
        del self._cards[:]
        for card, x in zip(cards, itertools.count(0, self._MARGIN)):
            image = CARD_IMAGES[card]
            rect = QRectF(x, 0, self._IMAGE_WIDTH, self._IMAGE_HEIGHT)
            self._cards.append((rect, image))
        self.repaint()

    def paintEvent(self, event):
        """Paint cards"""
        painter = QPainter()
        painter.begin(self)
        for rect, image in self._cards:
            self._draw_image(painter, rect, image)
        painter.end()

    def _draw_image(self, painter, rect, image):
        painter.fillRect(rect, Qt.white)
        painter.drawRect(rect)
        painter.drawImage(rect, image)
