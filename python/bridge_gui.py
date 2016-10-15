#!/usr/bin/env python3

"""Bridge frontend

This script starts the frontend which is part of the bridge project. The usage
is documented when the script is run with the -h argument.
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
import bridgegui.messaging as messaging
from bridgegui.messaging import sendCommand

HELLO_COMMAND = b'bridgehlo'
GET_COMMAND = b'get'
CALL_COMMAND = b'call'
PLAY_COMMAND = b'play'
BIDDING_COMMAND = b'bidding'

STATE_TAG = "state"
ALLOWED_CALLS_TAG = "allowedCalls"
ALLOWED_CARDS_TAG = "allowedCards"
CARDS_TAG = "cards"
CURRENT_TRICK_TAG = "currentTrick"


class BridgeWindow(QMainWindow):
    """The main window of the birdge frontend"""
    
    def __init__(self, args):
        """Initialize BridgeWindow"""
        super().__init__()
        self._position = None
        self._timer = QTimer(self)
        self._timer.setInterval(1000)
        self._init_sockets(args.endpoint)
        self._init_widgets()
        self.setWindowTitle('Bridge')
        self.show()
        self._timer.start()

    def _init_sockets(self, endpoint):
        logging.info("Initializing sockets")
        zmqctx = zmq.Context.instance()
        endpoint_generator = messaging.endpoints(endpoint)
        self._socket_notifiers = []
        self._control_socket = zmqctx.socket(zmq.DEALER)
        self._control_socket.connect(next(endpoint_generator))
        self._control_socket_queue = messaging.MessageQueue(
            self._control_socket, "control socket queue",
            messaging.REPLY_SUCCESS_PREFIX,
            {
                HELLO_COMMAND: self._handle_hello_reply,
                GET_COMMAND: self._handle_get_reply,
                CALL_COMMAND: self._handle_call_reply,
                PLAY_COMMAND: self._handle_play_reply,
            })
        self._connect_socket_to_notifier(
            self._control_socket, self._control_socket_queue)
        self._event_socket = zmqctx.socket(zmq.SUB)
        self._event_socket.setsockopt(zmq.SUBSCRIBE, CALL_COMMAND)
        self._event_socket.setsockopt(zmq.SUBSCRIBE, BIDDING_COMMAND)
        self._event_socket.setsockopt(zmq.SUBSCRIBE, PLAY_COMMAND)
        self._event_socket.connect(next(endpoint_generator))
        self._event_socket_queue = messaging.MessageQueue(
            self._event_socket, "event socket queue", [],
            {
                CALL_COMMAND: self._handle_call_event,
                BIDDING_COMMAND: self._handle_bidding_event,
                PLAY_COMMAND: self._handle_play_event,
            })
        self._connect_socket_to_notifier(
            self._event_socket, self._event_socket_queue)
        sendCommand(self._control_socket, HELLO_COMMAND)

    def _init_widgets(self):
        logging.info("Initializing widgets")
        self._central_widget = QWidget()
        self._layout = QHBoxLayout(self._central_widget)
        self._bidding_layout = QVBoxLayout()
        self._call_panel = bidding.CallPanel()
        self._call_panel.callMade.connect(self._send_call_command)
        self._bidding_layout.addWidget(self._call_panel)
        self._call_table = bidding.CallTable()
        self._bidding_layout.addWidget(self._call_table)
        self._layout.addLayout(self._bidding_layout)
        self._card_area = cards.CardArea()
        for hand in self._card_area.hands():
            hand.cardPlayed.connect(self._send_play_command)
        self._layout.addWidget(self._card_area)
        self.setCentralWidget(self._central_widget)

    def _connect_socket_to_notifier(self, socket, message_queue):
        def _handle_message_to_queue():
            if not message_queue.handleMessages():
                # TODO: Localization
                QMessageBox.warning(
                    self, "Server error",
                    "Unexpected message received from the server")
        socket_notifier = QSocketNotifier(socket.fd, QSocketNotifier.Read)
        socket_notifier.activated.connect(_handle_message_to_queue)
        self._timer.timeout.connect(_handle_message_to_queue)
        self._socket_notifiers.append(socket_notifier)

    def _request(self, *args):
        sendCommand(self._control_socket, GET_COMMAND, keys=args)

    def _send_call_command(self, call):
        sendCommand(self._control_socket, CALL_COMMAND, call=call)

    def _send_play_command(self, card):
        sendCommand(self._control_socket, PLAY_COMMAND, card=card._asdict())

    def _handle_hello_reply(self, position=None, **kwargs):
        if not position:
            raise messaging.ProtocolError("Expected server to assign position")
        logging.info("Successfully joined, position is %s", position)
        self._position = position
        self.setWindowTitle("Bridge: %s" % position)
        self._card_area.setPlayerPosition(position)
        self._request(
            ALLOWED_CALLS_TAG, ALLOWED_CARDS_TAG, CARDS_TAG, CURRENT_TRICK_TAG)

    def _handle_get_reply(
            self, allowedCalls=(), allowedCards=None, cards=None,
            currentTrick=None, **kwargs):
        self._call_panel.setAllowedCalls(allowedCalls)
        if cards is not None:
            self._card_area.setCards(cards)
        if allowedCards is not None:
            self._card_area.setAllowedCards(allowedCards)
        if currentTrick is not None:
            self._card_area.setTrick(currentTrick)

    def _handle_call_reply(self, **kwargs):
        logging.debug("Call successful")

    def _handle_play_reply(self, **kwargs):
        logging.debug("Play successful")

    def _handle_call_event(self, position=None, call=None, **kwargs):
        logging.debug("Call made. Position: %r, Call: %r", position, call)
        self._call_table.addCall(position, call)
        self._request(ALLOWED_CALLS_TAG)

    def _handle_bidding_event(self, **kwargs):
        logging.debug("Bidding completed")
        self._request(ALLOWED_CARDS_TAG)

    def _handle_play_event(self, position=None, card=None, **kwargs):
        logging.debug("Card played. Position: %r, Card: %r", position, card)
        self._card_area.playCard(position, card)
        self._request(CARDS_TAG, ALLOWED_CARDS_TAG, CURRENT_TRICK_TAG)

if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(levelname)-8s %(message)s', level=logging.DEBUG)

    parser = argparse.ArgumentParser(description='Run bridge frontend')
    parser.add_argument(
        'endpoint',
        help='''Base endpoint of the bridge backend. Follows ZeroMQ transmit
             protocol syntax. For example: tcp://bridge.example.com:5555''')
    args = parser.parse_args()

    app = QApplication(sys.argv)
    window = BridgeWindow(args)
    sys.exit(app.exec_())
