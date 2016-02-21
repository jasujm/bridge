#!/usr/bin/env python3

from kivy.app import App
from kivy.clock import Clock
from kivy.uix.button import Button
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label

from collections import namedtuple

SUITS = "C", "D", "H", "S"
RANKS = "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
STRAINS = "C", "D", "H", "S", "NT"
PASS, DOUBLE, REDOUBLE = "PASS", "DBL", "RDBL"
POSITIONS = "North", "East", "South", "West"
NORTH, EAST, SOUTH, WEST = POSITIONS
NORTH_SOUTH, EAST_WEST = "NS", "EW"

SUIT_TAGS = "clubs", "diamonds", "hearts", "spades"
RANK_TAGS = ("2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen",
             "king", "ace")
STRAIN_TAGS = "clubs", "diamonds", "hearts", "spades", "notrump"
CLUBS_TAG, DIAMONDS_TAG, HEARTS_TAG, SPADES_TAG, NOTRUMP_TAG = STRAIN_TAGS
POSITION_TAGS = "north", "east", "south", "west"
NORTH_TAG, EAST_TAG, SOUTH_TAG, WEST_TAG = POSITION_TAGS
BID_TAG, PASS_TAG, DBL_TAG, RDBL_TAG = "bid", "pass", "double", "redouble"
NORTH_SOUTH_TAG, EAST_WEST_TAG = "northSouth", "eastWest"

SUIT_MAP = {key: text for (key, text) in zip(SUIT_TAGS, SUITS)}
RANK_MAP = {key: text for (key, text) in zip(RANK_TAGS, RANKS)}
STRAIN_MAP = {key: text for (key, text) in zip(STRAIN_TAGS, STRAINS)}
POSITION_MAP = {
    key: text for (key, text) in zip(POSITION_TAGS, POSITIONS)}

Card = namedtuple("Card", ("rank", "suit"))
TricksWon = namedtuple("TricksWon", (NORTH_SOUTH_TAG, EAST_WEST_TAG))
CallEntry = namedtuple("CallEntry", ("position", "call"))
Call = namedtuple("Call", ("call", "bid"))
Bid = namedtuple("Bid", ("level", "strain"))
TrickEntry = namedtuple("TrickEntry", ("position", "card"))
ScoreEntry = namedtuple("ScoreEntry", ("partnership", "score"))

class CallPanel(GridLayout):
    def __init__(self, **kwargs):
        super(CallPanel, self).__init__(**kwargs)
        self.cols = 5
        for level in range(1, 8):
            for strain in STRAINS:
                self.add_widget(Button(text="%d%s" % (level, strain)))
        self.add_widget(Button(text=PASS))
        self.add_widget(Button(text=DOUBLE))
        self.add_widget(Button(text=REDOUBLE))

class BiddingPanel(GridLayout):
    def __init__(self, **kwargs):
        super(BiddingPanel, self).__init__(**kwargs)
        self.cols = 4
        self.set_calls([])
    def set_calls(self, calls):
        self.clear_widgets()
        self.add_widget(Label(text=NORTH))
        self.add_widget(Label(text=EAST))
        self.add_widget(Label(text=SOUTH))
        self.add_widget(Label(text=WEST))
        if not calls:
            return
        # TODO: Error handling
        n_skip = POSITION_TAGS.index(calls[0].position)
        for i in range(n_skip):
            self.add_widget(Label())
        for call in calls:
            if call.call.call == BID_TAG:
                level = call.call.bid.level
                strain = STRAIN_MAP[call.call.bid.strain]
                self.add_widget(Label(text="%d%s" % (level, strain)))
            elif call.call.call == PASS_TAG:
                self.add_widget(Label(text=PASS))
            elif call.call.call == DBL_TAG:
                self.add_widget(Label(text=DOUBLE))
            elif call.call.call == RDBL_TAG:
                self.add_widget(Label(text=REDOUBLE))

class HandPanel(BoxLayout):
    def __init__(self, position, **kwargs):
        super(HandPanel, self).__init__(**kwargs)
        self.orientation = "vertical"
        self._buttons = {}
        self.add_widget(Label(text=str(position)))
        for suit_tag, suit in zip(SUIT_TAGS, SUITS):
            row = BoxLayout(orientation="horizontal")
            row.add_widget(Label(text=suit))
            for rank_tag, rank in zip(RANK_TAGS, RANKS):
                button = Button(text=rank, disabled=True)
                self._buttons[(rank_tag, suit_tag)] = button
                row.add_widget(button)
            self.add_widget(row)
    def set_hand(self, hand):
        for button in self._buttons.values():
            button.disabled = True
        # TODO: Error handling
        for card in hand:
            self._buttons[(card.rank, card.suit)].disabled = False

class TrickPanel(GridLayout):
    _POSITIONS = { 1: NORTH_TAG, 3: WEST_TAG, 5: EAST_TAG, 7: SOUTH_TAG }
    def __init__(self, **kwargs):
        super(TrickPanel, self).__init__(**kwargs)
        self.rows = 3
        self.cols = 3
        self._cards = {position: Label() for position in POSITION_TAGS}
        for i in range(9):
            if i in self._POSITIONS:
                self.add_widget(self._cards[self._POSITIONS[i]])
            else:
                self.add_widget(Label())
    def set_trick(self, trick):
        for label in self._cards.values():
            label.text = ""
        # TODO: Error handling
        for entry in trick:
            rank = RANK_MAP[entry.card.rank]
            suit = SUIT_MAP[entry.card.suit]
            self._cards[entry.position].text = "%s%s" % (rank, suit)

class InfoPanel(BoxLayout):
    def __init__(self, **kwargs):
        super(InfoPanel, self).__init__(**kwargs)
        self.orientation = "vertical"
        self._label = Label()
        self._position_in_turn = None
        self._tricks_won = None
        self.add_widget(self._label)
    def set_position_in_turn(self, position_in_turn):
        self._position_in_turn = position_in_turn
        self._update_text()
    def set_tricks_won(self, tricks_won):
        self._tricks_won = tricks_won
        self._update_text()
    def _update_text(self):
        texts = []
        # TODO: Error handling
        if self._position_in_turn:
            texts.append(
                "Position in turn: %s" % POSITION_MAP[self._position_in_turn])
        if self._tricks_won:
            texts.append(
                "Tricks won\nNorth-South: %d\nEast-West: %d" %
                (self._tricks_won[0], self._tricks_won[1]))
        self._label.text = "\n".join(texts)

class PlayAreaPanel(GridLayout):
    _POSITIONS = { 1: NORTH_TAG, 3: WEST_TAG, 5: EAST_TAG, 7: SOUTH_TAG }
    def __init__(self, info_label, trick_panel, **kwargs):
        super(PlayAreaPanel, self).__init__(**kwargs)
        self.rows = 3
        self.cols = 3
        self._hands = {
            position: HandPanel(POSITION_MAP[position]) for
            position in POSITION_TAGS}
        for i in range(9):
            if i == 0:
                self.add_widget(info_label)
            elif i == 4:
                self.add_widget(trick_panel)
            elif i in self._POSITIONS:
                tag = self._POSITIONS[i]
                self.add_widget(self._hands[tag])
            else:
                self.add_widget(Label())
    def set_cards(self, cards):
        # TODO: Error handling
        for (position, hand) in cards.items():
            self._hands[position].set_hand(hand)

class ScoreSheetPanel(GridLayout):
    def __init__(self, **kwargs):
        super(ScoreSheetPanel, self).__init__(**kwargs)
        self.cols = 2
        self.add_widget(Label(text=NORTH_SOUTH))
        self.add_widget(Label(text=EAST_WEST))
        self._score_sheet = []
    def set_score_sheet(self, score_sheet):
        # TODO: Now we assume that new score extends old one
        first_new_entry = len(self._score_sheet)
        for entry in score_sheet[first_new_entry:]:
            self._set_score_entry(entry)
        self._score_sheet = list(score_sheet)
    def _set_score_entry(self, entry):
        # TODO: Error handling
        if not entry:
            north_south_score = "-"
            east_west_score = "-"
        elif entry.partnership == NORTH_SOUTH_TAG:
            north_south_score = str(entry.score)
            east_west_score = ""
        elif entry.partnership == EAST_WEST_TAG:
            north_south_score = ""
            east_west_score = str(entry.score)
        self.add_widget(Label(text=north_south_score))
        self.add_widget(Label(text=east_west_score))

class BridgeApp(App):
    def build(self):
        view = BoxLayout(orientation="horizontal")
        panel = BoxLayout(orientation="vertical", size_hint_x=0.35)
        self._call_panel = CallPanel()
        panel.add_widget(self._call_panel)
        self._bidding_panel = BiddingPanel()
        self._bidding_panel.set_calls([
            CallEntry(EAST_TAG, Call(PASS_TAG, None)),
            CallEntry(SOUTH_TAG, Call(BID_TAG, Bid(7, NOTRUMP_TAG))),
            CallEntry(WEST_TAG, Call(DBL_TAG, None)),
            CallEntry(NORTH_TAG, Call(RDBL_TAG, None)),
        ])
        panel.add_widget(self._bidding_panel)
        view.add_widget(panel)
        self._info_label = InfoPanel()
        self._info_label.set_position_in_turn(NORTH_TAG)
        self._info_label.set_tricks_won(TricksWon(0, 0))
        self._trick_panel = TrickPanel()
        self._trick_panel.set_trick([
            TrickEntry(NORTH_TAG, Card(RANK_TAGS[9], SUIT_TAGS[0])),
            TrickEntry(EAST_TAG, Card(RANK_TAGS[10], SUIT_TAGS[1])),
        ])
        self._play_area_panel = PlayAreaPanel(
            self._info_label, self._trick_panel)
        self._play_area_panel.set_cards({
            NORTH_TAG: [Card(RANK_TAGS[0], SUIT_TAGS[0])],
            EAST_TAG: [Card(RANK_TAGS[1], SUIT_TAGS[1])],
            SOUTH_TAG: [Card(RANK_TAGS[2], SUIT_TAGS[2])],
            WEST_TAG: [Card(RANK_TAGS[3], SUIT_TAGS[3])],
        })
        view.add_widget(self._play_area_panel)
        self._score_sheet_panel = ScoreSheetPanel(size_hint_x=0.2)
        self._score_sheet_panel.set_score_sheet([
            ScoreEntry(NORTH_SOUTH_TAG, 100),
            ScoreEntry(EAST_WEST_TAG, 420),
            None,
        ])
        view.add_widget(self._score_sheet_panel)
        return view

if __name__ == '__main__':
    BridgeApp().run()
