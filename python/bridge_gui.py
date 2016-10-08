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

from PyQt5.QtCore import QSocketNotifier, pyqtSignal
from PyQt5.QtWidgets import (
    QApplication, QGridLayout, QMainWindow,QMessageBox, QPushButton, QWidget)
import zmq

from bridgegui.util import freeze
from bridgegui.messaging import (
    endpoints, sendCommand, MessageQueue, REPLY_SUCCESS_PREFIX)

HELLO_COMMAND = b'bridgehlo'
GET_COMMAND = b'get'
CALL_COMMAND = b'call'

STATE_TAG = "state"
ALLOWED_CALLS_TAG = "allowedCalls"
ALLOWED_CARDS_TAG = "allowedCards"
TYPE_TAG = "type"
PASS_TAG = "pass"
BID_TAG = "bid"
DOUBLE_TAG = "double"
REDOUBLE_TAG = "redouble"
LEVEL_TAG = "level"
STRAIN_TAG = "strain"
CLUBS_TAG = "clubs"
DIAMONDS_TAG = "diamonds"
HEARTS_TAG = "hearts"
SPADES_TAG = "spades"
NOTRUMP_TAG = "notrump"

CALL_TYPE_FORMATS = { PASS_TAG: 'PASS', DOUBLE_TAG: 'X', REDOUBLE_TAG: 'XX' }
STRAIN_FORMATS = {
    CLUBS_TAG: 'C', DIAMONDS_TAG: 'D', HEARTS_TAG: 'H', SPADES_TAG: 'S',
    NOTRUMP_TAG: 'NT'
}


def format_call(call):
    """Return text representation of call

    Keyword Arguments:
    call -- object representing the call in call serialization format
    """
    s = CALL_TYPE_FORMATS.get(call[TYPE_TAG], None)
    if s:
        return s
    bid = call[BID_TAG]
    return "%d%s" % (bid[LEVEL_TAG], STRAIN_FORMATS[bid[STRAIN_TAG]])


class CallPanel(QWidget):
    """Panel used to select calls to be made"""

    callMade = pyqtSignal(dict)

    def __init__(self):
        """Initialize call panel"""
        super().__init__()
        self.setLayout(QGridLayout())
        self._buttons = {}
        suits = (CLUBS_TAG, DIAMONDS_TAG, HEARTS_TAG, SPADES_TAG, NOTRUMP_TAG)
        for row, level in enumerate(range(1, 8)):
            for col, strain in enumerate(suits):
                self._init_call(
                    row, col,
                    {
                        TYPE_TAG: BID_TAG,
                        BID_TAG: { LEVEL_TAG: level, STRAIN_TAG: strain }
                    })
        self._init_call(8, 0, { TYPE_TAG: PASS_TAG })
        self._init_call(8, 1, { TYPE_TAG: DOUBLE_TAG })
        self._init_call(8, 2, { TYPE_TAG: REDOUBLE_TAG })

    def set_allowed_calls(self, calls):
        for button in self._buttons.values():
            button.setEnabled(False)
        for call in calls:
            button = self._buttons.get(freeze(call), None)
            if not button:
                return False
            button.setEnabled(True)
        return True

    def _init_call(self, row, col, call):
        button = QPushButton(format_call(call))
        button.clicked.connect(lambda: self.callMade.emit(call))
        self.layout().addWidget(button, row, col)
        self._buttons[freeze(call)] = button


class BridgeWindow(QMainWindow):
    """The main window of the birdge frontend"""
    
    def __init__(self, args):
        """Initialize BridgeWindow"""
        super().__init__()
        self._init_sockets(args.endpoint)
        self._init_widgets()
        self.setWindowTitle('Bridge')
        self.show()

    def _init_sockets(self, endpoint):
        logging.info("Initializing sockets")
        zmqctx = zmq.Context.instance()
        endpoint_generator = endpoints(endpoint)
        self._socket_notifiers = []
        self._control_socket = zmqctx.socket(zmq.DEALER)
        self._control_socket.connect(next(endpoint_generator))
        self._control_socket_queue = MessageQueue(
            self._control_socket, "control socket queue",
            REPLY_SUCCESS_PREFIX,
            {
                HELLO_COMMAND: self._handle_hello_reply,
                GET_COMMAND: self._handle_get_reply,
                CALL_COMMAND: self._handle_call_reply,
            })
        self._connect_socket_to_notifier(
            self._control_socket, self._control_socket_queue)
        self._event_socket = zmqctx.socket(zmq.SUB)
        self._event_socket.setsockopt(zmq.SUBSCRIBE, CALL_COMMAND)
        self._event_socket.connect(next(endpoint_generator))
        self._event_socket_queue = MessageQueue(
            self._event_socket, "event socket queue", [],
            {
                CALL_COMMAND: self._handle_call_event
            })
        self._connect_socket_to_notifier(
            self._event_socket, self._event_socket_queue)
        sendCommand(self._control_socket, HELLO_COMMAND)

    def _init_widgets(self):
        logging.info("Initializing widgets")
        self._call_panel = CallPanel()
        self._call_panel.callMade.connect(self._send_call_command)
        self.setCentralWidget(self._call_panel)

    def _connect_socket_to_notifier(self, socket, message_queue):
        def _handle_message_to_queue():
            if not message_queue.handleMessages():
                QMessageBox.warning(
                    "Server error",
                    "Unexpected message received from the server")
        socket_notifier = QSocketNotifier(socket.fd, QSocketNotifier.Read)
        socket_notifier.activated.connect(_handle_message_to_queue)
        self._socket_notifiers.append(socket_notifier)

    def _request(self, *args):
        sendCommand(self._control_socket, GET_COMMAND, keys=args)
        
    def _send_call_command(self, call):
        sendCommand(self._control_socket, CALL_COMMAND, call=call)

    def _handle_hello_reply(self, position=None, **kwargs):
        if not position:
            messaging.protocolError("Expected server to assign position")
        logging.info("Successfully joined, position is %s", position)
        self.setWindowTitle("Bridge: %s" % position)
        self._request(STATE_TAG, ALLOWED_CALLS_TAG, ALLOWED_CARDS_TAG)

    def _handle_get_reply(self, allowedCalls=(), **kwargs):
        self._call_panel.set_allowed_calls(allowedCalls)

    def _handle_call_reply(self, **kwargs):
        pass

    def _handle_call_event(self, position=None, call=None, **kwargs):
        logging.debug("Call made by %s: %s", position, str(call))
        self._request(ALLOWED_CALLS_TAG)

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
