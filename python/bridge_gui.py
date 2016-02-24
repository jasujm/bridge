#!/usr/bin/env python3

import json
from collections import namedtuple

from kivy.app import App
from kivy.clock import Clock
from kivy.uix.button import Button
from kivy.uix.boxlayout import BoxLayout
from kivy.uix.gridlayout import GridLayout
from kivy.uix.label import Label
import zmq

POSITIONS = "North", "East", "South", "West"
RANKS = "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
STRAINS = "C", "D", "H", "S", "NT"
SUITS = "C", "D", "H", "S"
NORTH, EAST, SOUTH, WEST = POSITIONS
NORTH_SOUTH, EAST_WEST = "NS", "EW"
PASS, DOUBLE, REDOUBLE = "PASS", "DBL", "RDBL"
DOUBLINGS = "", "X", "XX"

BID_TAG, PASS_TAG, DBL_TAG, RDBL_TAG = "bid", "pass", "double", "redouble"
NORTH_SOUTH_TAG, EAST_WEST_TAG = "northSouth", "eastWest"
POSITION_TAGS = "north", "east", "south", "west"
RANK_TAGS = ("2", "3", "4", "5", "6", "7", "8", "9", "10", "jack", "queen",
             "king", "ace")
STRAIN_TAGS = "clubs", "diamonds", "hearts", "spades", "notrump"
SUIT_TAGS = "clubs", "diamonds", "hearts", "spades"
CLUBS_TAG, DIAMONDS_TAG, HEARTS_TAG, SPADES_TAG, NOTRUMP_TAG = STRAIN_TAGS
NORTH_TAG, EAST_TAG, SOUTH_TAG, WEST_TAG = POSITION_TAGS
DOUBLING_TAGS = "undoubled", "doubled", "redoubled"

DOUBLING_MAP = {key: text for (key, text) in zip(DOUBLING_TAGS, DOUBLINGS)}
POSITION_MAP = {key: text for (key, text) in zip(POSITION_TAGS, POSITIONS)}
RANK_MAP = {key: text for (key, text) in zip(RANK_TAGS, RANKS)}
STRAIN_MAP = {key: text for (key, text) in zip(STRAIN_TAGS, STRAINS)}
SUIT_MAP = {key: text for (key, text) in zip(SUIT_TAGS, SUITS)}

CALL_COMMAND = b"call"
PLAY_COMMAND = b"play"
REPLY_SUCCESS = b"success"
SCORE_COMMAND = b"score"
STATE_COMMAND = b"state"

BID_KEY = "bid"
CALLS_KEY = "calls"
CALL_KEY = "call"
CARDS_KEY = "cards"
CARD_KEY = "card"
CURRENT_TRICK_KEY = "currentTrick"
CONTRACT_KEY = "contract"
DECLARER_KEY = "declarer"
DOUBLING_KEY = "doubling"
LEVEL_KEY = "level"
PARTNERSHIP_KEY = "partnership"
POSITION_IN_TURN_KEY = "positionInTurn"
POSITION_KEY = "position"
RANK_KEY = "rank"
SCORE_KEY = "score"
STRAIN_KEY = "strain"
SUIT_KEY = "suit"
TRICKS_WON_KEY = "tricksWon"
TYPE_KEY = "type"
VULNERABILITY_KEY = "vulnerability"

Bid = namedtuple("Bid", (LEVEL_KEY, STRAIN_KEY))
Call = namedtuple("Call", ("type_", BID_KEY))
CallEntry = namedtuple("CallEntry", (POSITION_KEY, CALL_KEY))
Card = namedtuple("Card", (RANK_KEY, SUIT_KEY))
Contract = namedtuple("Contract", (BID_KEY, DOUBLING_KEY))
ScoreEntry = namedtuple("ScoreEntry", (PARTNERSHIP_KEY, SCORE_KEY))
TrickEntry = namedtuple("TrickEntry", (POSITION_KEY, CARD_KEY))
TricksWon = namedtuple("TricksWon", (NORTH_SOUTH_TAG, EAST_WEST_TAG))
Vulnerability = namedtuple("Vulnerability", (NORTH_SOUTH_TAG, EAST_WEST_TAG))


def check_command_success(socket):
    reply = socket.recv_multipart()
    assert len(reply) == 1 and reply[0] == REPLY_SUCCESS


def parse_hand(obj):
    return [Card(**card) for card in obj]


def parse_call_entry(obj):
    position = obj[POSITION_KEY]
    call = obj[CALL_KEY]
    call_type = call[TYPE_KEY]
    call_bid = Bid(**call[BID_KEY]) if BID_KEY in call else None
    return CallEntry(position, Call(call_type, call_bid))


def parse_contract(obj):
    return Contract(Bid(**obj[BID_KEY]), obj[DOUBLING_KEY])


def parse_trick_entry(obj):
    position = obj[POSITION_KEY]
    card = Card(**obj[CARD_KEY])
    return TrickEntry(position, card)


def parse_score_entry(obj):
    if obj is None:
        return obj
    partnership = obj[PARTNERSHIP_KEY]
    score = obj[SCORE_KEY]
    return ScoreEntry(partnership, score)


def format_bid(bid):
    return "%d%s" % (bid.level, STRAIN_MAP[bid.strain])

class CallPanel(GridLayout):

    def __init__(self, socket, **kwargs):
        super(CallPanel, self).__init__(**kwargs)
        self._socket = socket
        self.cols = 5
        for level in range(1, 8):
            for strain_tag, strain in zip(STRAIN_TAGS, STRAINS):
                bid = Bid(level, strain_tag)
                self._add_button(format_bid(bid), BID_TAG, bid)
        self._add_button(PASS, PASS_TAG)
        self._add_button(DOUBLE, DBL_TAG)
        self._add_button(REDOUBLE, RDBL_TAG)

    def _add_button(self, text, call_type, call_bid=None):
        button = Button(text=text)
        button.call_type = call_type
        button.call_bid = call_bid
        button.bind(on_press=self._make_call)
        self.add_widget(button)

    def _make_call(self, button):
        # TODO: Should not wait reply in GUI thread
        call = {TYPE_KEY: button.call_type}
        if button.call_bid:
            call[BID_KEY] = button.call_bid._asdict()
        self._socket.send(CALL_COMMAND, flags=zmq.SNDMORE)
        self._socket.send_string(json.dumps(call))
        check_command_success(self._socket)


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
            if call.call.type_ == BID_TAG:
                level = call.call.bid.level
                strain = STRAIN_MAP[call.call.bid.strain]
                self.add_widget(Label(text="%d%s" % (level, strain)))
            elif call.call.type_ == PASS_TAG:
                self.add_widget(Label(text=PASS))
            elif call.call.type_ == DBL_TAG:
                self.add_widget(Label(text=DOUBLE))
            elif call.call.type_ == RDBL_TAG:
                self.add_widget(Label(text=REDOUBLE))


class HandPanel(BoxLayout):

    def __init__(self, socket, position, **kwargs):
        super(HandPanel, self).__init__(**kwargs)
        self._socket = socket
        self.orientation = "vertical"
        self._buttons = {}
        self._position_label = Label(text=str(position))
        self.add_widget(self._position_label)
        for suit_tag, suit in zip(SUIT_TAGS, SUITS):
            row = BoxLayout(orientation="horizontal")
            row.add_widget(Label(text=suit))
            for rank_tag, rank in zip(RANK_TAGS, RANKS):
                button = Button(text=rank, disabled=True)
                button.card_rank = rank_tag
                button.card_suit = suit_tag
                button.bind(on_press=self._make_play)
                self._buttons[(rank_tag, suit_tag)] = button
                row.add_widget(button)
            self.add_widget(row)

    def set_in_turn(self, in_turn):
        self._position_label.bold = in_turn

    def set_vulnerable(self, vulnerable):
        if vulnerable:
            self._position_label.color = (1, 0, 0, 1)
        else:
            self._position_label.color = (1, 1, 1, 1)

    def set_hand(self, hand):
        for button in self._buttons.values():
            button.disabled = True
        # TODO: Error handling
        for card in hand:
            self._buttons[(card.rank, card.suit)].disabled = False

    def _make_play(self, button):
        # TODO: Should not wait reply in GUI thread
        card = {RANK_KEY: button.card_rank, SUIT_KEY: button.card_suit}
        self._socket.send(PLAY_COMMAND, flags=zmq.SNDMORE)
        self._socket.send_string(json.dumps(card))
        check_command_success(self._socket)


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
        self._contract = None
        self._declarer = None
        self._tricks_won = None
        self.add_widget(self._label)

    def set_declarer(self, declarer):
        self._declarer = declarer
        self._update_text()

    def set_contract(self, contract):
        self._contract = contract
        self._update_text()

    def set_tricks_won(self, tricks_won):
        self._tricks_won = tricks_won
        self._update_text()

    def _update_text(self):
        texts = []
        # TODO: Error handling
        if self._declarer:
            texts.append("Declarer: %s" % POSITION_MAP[self._declarer])
        if self._contract:
            texts.append("Contract: %s%s" % (
                format_bid(self._contract.bid),
                DOUBLING_MAP[self._contract.doubling]))
        if self._tricks_won:
            texts.append(
                "Tricks won\nNorth-South: %d\nEast-West: %d" %
                (self._tricks_won[0], self._tricks_won[1]))
        self._label.text = "\n".join(texts)


class PlayAreaPanel(GridLayout):

    _POSITIONS = { 1: NORTH_TAG, 3: WEST_TAG, 5: EAST_TAG, 7: SOUTH_TAG }

    def __init__(self, socket, info_panel, trick_panel, **kwargs):
        super(PlayAreaPanel, self).__init__(**kwargs)
        self.rows = 3
        self.cols = 3
        self._hands = {
            position: HandPanel(socket, POSITION_MAP[position]) for
            position in POSITION_TAGS}
        for i in range(9):
            if i == 0:
                self.add_widget(info_panel)
            elif i == 4:
                self.add_widget(trick_panel)
            elif i in self._POSITIONS:
                tag = self._POSITIONS[i]
                self.add_widget(self._hands[tag])
            else:
                self.add_widget(Label())

    def set_position_in_turn(self, position):
        for (tag, hand) in self._hands.items():
            hand.set_in_turn(tag == position)

    def set_vulnerability(self, vulnerability):
        self._hands[NORTH_TAG].set_vulnerable(
            vulnerability and vulnerability.northSouth)
        self._hands[SOUTH_TAG].set_vulnerable(
            vulnerability and vulnerability.northSouth)
        self._hands[EAST_TAG].set_vulnerable(
            vulnerability and vulnerability.eastWest)
        self._hands[WEST_TAG].set_vulnerable(
            vulnerability and vulnerability.eastWest)

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
        # Socket
        zmqctx = zmq.Context.instance()
        control_socket = zmqctx.socket(zmq.REQ)
        control_socket.connect("tcp://localhost:5555")
        self._data_socket = zmqctx.socket(zmq.SUB)
        self._data_socket.setsockopt(zmq.SUBSCRIBE, STATE_COMMAND)
        self._data_socket.setsockopt(zmq.SUBSCRIBE, SCORE_COMMAND)
        self._data_socket.connect("tcp://localhost:5556")
        control_socket.send(STATE_COMMAND)
        check_command_success(control_socket)
        control_socket.send(SCORE_COMMAND)
        check_command_success(control_socket)
        Clock.schedule_interval(self._poll_data, 0)
        # UI
        view = BoxLayout(orientation="horizontal")
        panel = BoxLayout(orientation="vertical", size_hint_x=0.35)
        self._call_panel = CallPanel(control_socket)
        panel.add_widget(self._call_panel)
        self._bidding_panel = BiddingPanel()
        panel.add_widget(self._bidding_panel)
        view.add_widget(panel)
        self._info_panel = InfoPanel()
        self._trick_panel = TrickPanel()
        self._play_area_panel = PlayAreaPanel(
            control_socket, self._info_panel, self._trick_panel)
        view.add_widget(self._play_area_panel)
        self._score_sheet_panel = ScoreSheetPanel(size_hint_x=0.2)
        view.add_widget(self._score_sheet_panel)
        return view

    def _poll_data(self, dt):
        while True:
            try:
                msg = self._data_socket.recv_multipart(flags=zmq.NOBLOCK)
            except zmq.Again:
                break
            self._handle_message(msg)

    def _handle_message(self, msg):
        if msg[0] == STATE_COMMAND:
            # TODO: Error handling
            state = json.loads(msg[1].decode("utf-8"))
            position_in_turn = state.get(POSITION_IN_TURN_KEY)
            self._play_area_panel.set_position_in_turn(position_in_turn)
            vulnerability = (Vulnerability(**state[VULNERABILITY_KEY]) if
                             VULNERABILITY_KEY in state else None)
            self._play_area_panel.set_vulnerability(vulnerability)
            cards = {position: parse_hand(hand) for (position, hand) in
                     state[CARDS_KEY].items()} if CARDS_KEY in state else {}
            self._play_area_panel.set_cards(cards)
            calls = ([parse_call_entry(obj) for obj in state[CALLS_KEY]] if
                     CALLS_KEY in state else [])
            self._bidding_panel.set_calls(calls)
            declarer = state.get(DECLARER_KEY)
            self._info_panel.set_declarer(declarer)
            contract = (parse_contract(state[CONTRACT_KEY]) if
                        CONTRACT_KEY in state else None)
            self._info_panel.set_contract(contract)
            trick = ([parse_trick_entry(obj) for obj in
                      state[CURRENT_TRICK_KEY]] if
                     CURRENT_TRICK_KEY in state else [])
            self._trick_panel.set_trick(trick)
            tricks_won = (TricksWon(**state[TRICKS_WON_KEY]) if
                          TRICKS_WON_KEY in state else None)
            self._info_panel.set_tricks_won(tricks_won)
        elif msg[0] == SCORE_COMMAND:
            score_sheet = json.loads(msg[1].decode("utf-8"))
            scores = [parse_score_entry(entry) for entry in score_sheet]
            self._score_sheet_panel.set_score_sheet(scores)
            

if __name__ == '__main__':
    BridgeApp().run()
