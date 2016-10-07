#!/usr/bin/env python3

"""Bridge frontend

This script starts the frontend which is part of the bridge project. The usage
is documented when the script is run with the -h argument.
"""

import argparse
import json
import logging
import sys

from PyQt5.QtCore import QSocketNotifier
from PyQt5.QtWidgets import QApplication, QMainWindow, QMessageBox
import zmq

EMPTY_FRAME = b''
REPLY_SUCCESS_PREFIX = [EMPTY_FRAME, b'success']
HELLO_COMMAND = b'bridgehlo'
GET_COMMAND = b'get'


def send_command(socket, command, **kwargs):
    """Send command to the backend application using the bridge protocol

    Keyword Arguments:
    socket   -- the socket used for sending the command
    command  -- (str) the command to be sent
    **kwargs -- The arguments of the command (the values are serialized as JSON)
    """
    parts = [EMPTY_FRAME, command]
    for (key, value) in kwargs.items():
        parts.extend((key.encode(), json.dumps(value).encode()))
    logging.debug("Sending command: %s", str(parts))
    socket.send_multipart(parts)


class ProtocolError(Exception):
    """Error indicating unexpected message from bridge server"""
    pass


class BridgeWindow(QMainWindow):
    """The main window of the birdge frontend"""
    
    def __init__(self, args):
        """Initialize BridgeWindow"""
        super().__init__()
        self._init_sockets(args.endpoint)
        self._init_message_handlers()
        self._init_widgets()
        self.setWindowTitle('Bridge')
        self.show()

    def _init_sockets(self, endpoint):
        logging.info("Initializing sockets")
        zmqctx = zmq.Context.instance()
        self._control_socket = zmqctx.socket(zmq.DEALER)
        self._socket_notifier = QSocketNotifier(
            self._control_socket.fd, QSocketNotifier.Read)
        self._socket_notifier.activated.connect(
            self._handle_control_socket_message)
        self._control_socket.connect(endpoint)
        send_command(self._control_socket, HELLO_COMMAND)

    def _init_message_handlers(self):
        logging.info("Inititializing message handlers")
        self._message_handlers = {
            HELLO_COMMAND: self._handle_hello_reply,
            GET_COMMAND: self._handle_get_reply,
        }

    def _init_widgets(self):
        logging.info("Initializing widgets")

    def _handle_control_socket_message(self):
        logging.debug("Ready to receive reply")
        while True:
            try:
                parts = self._control_socket.recv_multipart(flags=zmq.NOBLOCK)
            except zmq.Again:
                return
            try:
                self._handle_control_reply(parts)
            except ProtocolError as e:
                logging.warning(str(e))
                QMessageBox.warning(
                    self, "Server error",
                    "Unexpected message was encountered while handling message from the server. Warning has been logged to aid diagnostics.")

    def _handle_control_reply(self, parts):
        logging.debug("Received reply: %s", str(parts))
        if parts[:2] != REPLY_SUCCESS_PREFIX or len(parts) < 3:
            self._protocol_error("Unexpected reply")
        command = self._message_handlers.get(parts[2], None)
        if not command:
            self._protocol_error(
                "Unrecognized command: %s", parts[2].decode())
        kwargs = {}
        for n in range(3, len(parts)-1, 2):
            key = parts[n].decode()
            value = parts[n+1].decode()
            try:
                value = json.loads(value)
            except json.decoder.JSONDecodeError as e:
                self._protocol_error(
                    "Error while parsing '%s': %s", value, str(e))
            kwargs[key] = value
        command(**kwargs)

    def _handle_hello_reply(self, position=None, **kwargs):
        if not position:
            self._protocol_error("Expected server to assign position")
        logging.info("Handshake complete, position is %s", position)
        send_command(
            self._control_socket, GET_COMMAND,
            keys=["state", "allowedCalls", "allowedCards"])

    def _handle_get_reply(self, **kwargs):
        pass

    def _protocol_error(self, msg, *args, **kwargs):
        raise ProtocolError(msg % args)

if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(levelname)-8s %(message)s')

    parser = argparse.ArgumentParser(description='Run bridge frontend')
    parser.add_argument(
        'endpoint',
        help='''Base endpoint of the bridge backend. Follows ZeroMQ transmit
             protocol syntax. For example: tcp://bridge.example.com:5555''')
    args = parser.parse_args()

    app = QApplication(sys.argv)
    window = BridgeWindow(args)
    sys.exit(app.exec_())
