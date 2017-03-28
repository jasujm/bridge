"""Bridge frontend

This module contains the startpoint for the frontend which is part of the bridge
project. The usage is documented when the script is run with the -h argument.
"""

import argparse
import json
import logging
import re
import sys

from PyQt5.QtCore import QSocketNotifier, QTimer
from PyQt5.QtWidgets import (
    QApplication, QHBoxLayout, QMainWindow, QMessageBox, QVBoxLayout, QWidget)
import zmq

import bridgegui.bidding as bidding
import bridgegui.cards as cards
import bridgegui.config as config
import bridgegui.messaging as messaging
from bridgegui.messaging import sendCommand
from bridgegui.positions import POSITION_TAGS
import bridgegui.score as score
import bridgegui.tricks as tricks

HELLO_COMMAND = b'bridgehlo'
GAME_COMMAND = b'game'
GET_COMMAND = b'get'
DEAL_COMMAND = b'deal'
CALL_COMMAND = b'call'
BIDDING_COMMAND = b'bidding'
PLAY_COMMAND = b'play'
DUMMY_COMMAND = b'dummy'
TRICK_COMMAND = b'trick'
DEALEND_COMMAND = b'dealend'

CLIENT_TAG = "client"
POSITION_TAG = "position"
POSITION_IN_TURN_TAG = "positionInTurn"
ALLOWED_CALLS_TAG = "allowedCalls"
CALLS_TAG = "calls"
DECLARER_TAG = "declarer"
CONTRACT_TAG = "contract"
ALLOWED_CARDS_TAG = "allowedCards"
CARDS_TAG = "cards"
TRICK_TAG = "trick"
TRICKS_WON_TAG = "tricksWon"
VULNERABILITY_TAG = "vulnerability"
SCORE_TAG = "score"


class BridgeWindow(QMainWindow):
    """The main window of the birdge frontend"""

    def __init__(self, control_socket, event_socket, position):
        """Initialize BridgeWindow

        Keyword Arguments:
        control_socket -- the control socket used to send commands
        event_socket   -- the event socket used to subscribe events
        """
        super().__init__()
        self._position = None
        self._preferred_positions = self._make_preferred_positions(position)
        self._timer = QTimer(self)
        self._timer.setInterval(1000)
        self._init_sockets(control_socket, event_socket)
        self._init_widgets()
        self.setWindowTitle("Bridge") # TODO: Localization
        self.show()
        self._timer.start()

    def _make_preferred_positions(self, position):
        all_positions = list(POSITION_TAGS)
        try:
            n = all_positions.index(position)
            return [position] + all_positions[:n] + all_positions[n+1:]
        except ValueError:
            return all_positions

    def _init_sockets(self, control_socket, event_socket):
        logging.info("Initializing message handlers")
        zmqctx = zmq.Context.instance()
        self._socket_notifiers = []
        self._control_socket = control_socket
        self._control_socket_queue = messaging.MessageQueue(
            control_socket, "control socket queue",
            messaging.REPLY_SUCCESS_PREFIX,
            {
                HELLO_COMMAND: self._handle_hello_reply,
                GAME_COMMAND: self._handle_game_reply,
                GET_COMMAND: self._handle_get_reply,
                CALL_COMMAND: self._handle_call_reply,
                PLAY_COMMAND: self._handle_play_reply,
            })
        self._connect_socket_to_notifier(
            control_socket, self._control_socket_queue)
        self._event_socket = event_socket
        self._event_socket_queue = messaging.MessageQueue(
            event_socket, "event socket queue", [],
            {
                DEAL_COMMAND: self._handle_deal_event,
                CALL_COMMAND: self._handle_call_event,
                BIDDING_COMMAND: self._handle_bidding_event,
                PLAY_COMMAND: self._handle_play_event,
                DUMMY_COMMAND: self._handle_dummy_event,
                TRICK_COMMAND: self._handle_trick_event,
                DEALEND_COMMAND: self._handle_dealend_event,
            })
        self._connect_socket_to_notifier(event_socket, self._event_socket_queue)
        sendCommand(control_socket, HELLO_COMMAND, version=[0], role=CLIENT_TAG)

    def _init_widgets(self):
        logging.info("Initializing widgets")
        self._central_widget = QWidget(self)
        self._layout = QHBoxLayout(self._central_widget)
        self._bidding_layout = QVBoxLayout()
        self._call_panel = bidding.CallPanel(self._central_widget)
        self._call_panel.callMade.connect(self._send_call_command)
        self._bidding_layout.addWidget(self._call_panel)
        self._call_table = bidding.CallTable(self._central_widget)
        self._bidding_layout.addWidget(self._call_table)
        self._bidding_result_label = bidding.ResultLabel(self._central_widget)
        self._bidding_layout.addWidget(self._bidding_result_label)
        self._tricks_won_label = tricks.TricksWonLabel(self._central_widget)
        self._bidding_layout.addWidget(self._tricks_won_label)
        self._layout.addLayout(self._bidding_layout)
        self._card_area = cards.CardArea(self._central_widget)
        for hand in self._card_area.hands():
            hand.cardPlayed.connect(self._send_play_command)
        self._layout.addWidget(self._card_area)
        self._score_table = score.ScoreTable(self._central_widget)
        self._layout.addWidget(self._score_table)
        self.setCentralWidget(self._central_widget)

    def _connect_socket_to_notifier(self, socket, message_queue):
        def _handle_message_to_queue():
            if not message_queue.handleMessages():
                # TODO: Localization
                QMessageBox.warning(
                    self, "Server error",
                    "Error while receiving message from server. Please see logs.")
                self._timer.timeout.disconnect(_handle_message_to_queue)
        socket_notifier = QSocketNotifier(socket.fd, QSocketNotifier.Read, self)
        socket_notifier.activated.connect(_handle_message_to_queue)
        self._timer.timeout.connect(_handle_message_to_queue)
        self._socket_notifiers.append(socket_notifier)

    def _request(self, *args):
        sendCommand(self._control_socket, GET_COMMAND, keys=args)

    def _send_call_command(self, call):
        sendCommand(self._control_socket, CALL_COMMAND, call=call)

    def _send_play_command(self, card):
        sendCommand(self._control_socket, PLAY_COMMAND, card=card._asdict())

    def _handle_hello_reply(self, **kwargs):
        logging.info("Handshake successful")
        sendCommand(
            self._control_socket, GAME_COMMAND,
            positions=self._preferred_positions)

    def _handle_game_reply(self, **kwargs):
        logging.info("Joined game")
        self._request(
            POSITION_TAG, POSITION_IN_TURN_TAG, ALLOWED_CALLS_TAG, CALLS_TAG,
            DECLARER_TAG, CONTRACT_TAG, ALLOWED_CARDS_TAG, CARDS_TAG,
            TRICK_TAG, TRICKS_WON_TAG, VULNERABILITY_TAG, SCORE_TAG)

    def _handle_get_reply(
            self, position=None, allowedCalls=None, calls=None,
            allowedCards=None, cards=None, trick=None, tricksWon=None,
            score=None, **kwargs):
        if position:
            self._position = position
            self._card_area.setPlayerPosition(position)
        if POSITION_IN_TURN_TAG in kwargs:
            self._card_area.setPositionInTurn(kwargs[POSITION_IN_TURN_TAG])
        if allowedCalls is not None:
            self._call_panel.setAllowedCalls(allowedCalls)
        if calls is not None:
            self._call_table.setCalls(calls)
        if DECLARER_TAG in kwargs and CONTRACT_TAG in kwargs:
            self._bidding_result_label.setBiddingResult(
                kwargs[DECLARER_TAG], kwargs[CONTRACT_TAG])
        if cards is not None:
            self._card_area.setCards(cards)
        if allowedCards is not None:
            self._card_area.setAllowedCards(allowedCards)
        if trick is not None:
            self._card_area.setTrick(trick)
        if tricksWon is not None:
            self._tricks_won_label.setTricksWon(tricksWon)
        if VULNERABILITY_TAG in kwargs:
            self._call_table.setVulnerability(kwargs[VULNERABILITY_TAG])
        if score is not None:
            self._score_table.setScoreSheet(score)

    def _handle_call_reply(self, **kwargs):
        logging.debug("Call successful")

    def _handle_play_reply(self, **kwargs):
        logging.debug("Play successful")

    def _handle_deal_event(self, **kwargs):
        logging.debug("Cards dealt")
        self._request(POSITION_IN_TURN_TAG, ALLOWED_CALLS_TAG, CARDS_TAG)

    def _handle_call_event(self, position=None, call=None, **kwargs):
        logging.debug("Call made. Position: %r, Call: %r", position, call)
        self._call_table.addCall(position, call)
        self._request(POSITION_IN_TURN_TAG, ALLOWED_CALLS_TAG, CALLS_TAG)

    def _handle_bidding_event(self, **kwargs):
        logging.debug("Bidding completed")
        self._request(ALLOWED_CARDS_TAG, DECLARER_TAG, CONTRACT_TAG)

    def _handle_play_event(self, position=None, card=None, **kwargs):
        logging.debug("Card played. Position: %r, Card: %r", position, card)
        self._card_area.playCard(position, card)
        self._request(
            POSITION_IN_TURN_TAG, CARDS_TAG, ALLOWED_CARDS_TAG, TRICK_TAG)

    def _handle_dummy_event(self, **kwargs):
        logging.debug("Dummy hand revealed")
        self._request(CARDS_TAG, ALLOWED_CARDS_TAG)

    def _handle_trick_event(self, winner, **kwargs):
        logging.debug("Trick completed. Winner: %r", winner)
        self._tricks_won_label.addTrick(winner)
        self._request(TRICKS_WON_TAG)

    def _handle_dealend_event(self, **kwargs):
        logging.debug("Deal ended")
        self._request(
            CALLS_TAG, CARDS_TAG, VULNERABILITY_TAG, SCORE_TAG, DECLARER_TAG,
            CONTRACT_TAG)

def main():
    parser = argparse.ArgumentParser(
        description="A lightweight bridge application")
    parser.add_argument(
        "endpoint",
        help="""Base endpoint of the bridge backend. Follows ZeroMQ transmit
             protocol syntax. For example: tcp://bridge.example.com:5555""")
    parser.add_argument(
        "--config",
        help="""Configuration file. The file tracks the identity of the player
             withing a single bridge session. By using the same configuration
             file between runs, the game session can be preserved.

             If the file does not exist, it will be created with a new random
             identity. It is possible to play without config file but in that
             case preserving the session is not possible.""")
    parser.add_argument(
        '--position',
        help="""If provided, the application requests the server to assign the
             given position. If the position is not available (or if the option
             is not given), any position is requested.""")
    parser.add_argument(
        "--verbose", "-v", action="count", default=0,
        help="""Increase logging levels. Repeat for even more logging.""")
    args = parser.parse_args()

    logging_level = logging.WARNING
    if args.verbose == 1:
        logging_level = logging.INFO
    elif args.verbose >= 2:
        logging_level = logging.DEBUG
    logging.basicConfig(
        format='%(asctime)s %(levelname)-8s %(message)s', level=logging_level)

    logging.info("Initializing sockets")
    zmqctx = zmq.Context.instance()
    endpoint_generator = messaging.endpoints(args.endpoint)
    control_socket = zmqctx.socket(zmq.DEALER)
    identity = config.getIdentity(args.config)
    control_socket.identity = identity.bytes
    control_socket.connect(next(endpoint_generator))
    event_socket = zmqctx.socket(zmq.SUB)
    event_socket.setsockopt(zmq.SUBSCRIBE, DEAL_COMMAND)
    event_socket.setsockopt(zmq.SUBSCRIBE, CALL_COMMAND)
    event_socket.setsockopt(zmq.SUBSCRIBE, BIDDING_COMMAND)
    event_socket.setsockopt(zmq.SUBSCRIBE, PLAY_COMMAND)
    event_socket.setsockopt(zmq.SUBSCRIBE, DUMMY_COMMAND)
    event_socket.setsockopt(zmq.SUBSCRIBE, TRICK_COMMAND)
    event_socket.setsockopt(zmq.SUBSCRIBE, DEALEND_COMMAND)
    event_socket.connect(next(endpoint_generator))

    logging.info("Starting main window")
    app = QApplication(sys.argv)
    window = BridgeWindow(control_socket, event_socket, args.position)
    code = app.exec_()

    logging.info("Main window closed. Closing sockets.")
    zmqctx.destroy(linger=0)
    return code


if __name__ == "__main__":
    main()