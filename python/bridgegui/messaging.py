"""Messaging utilities for the bridge frontend"""

import re
import logging

import json
import zmq

EMPTY_FRAME = b''
REPLY_SUCCESS_PREFIX = [EMPTY_FRAME, b'success']

ENDPOINT_REGEX = re.compile(r"tcp://(.+):(\d+)")


def endpoints(base):
    """Generate successive endpoints starting from given base

    This generator consumes a base endpoint as its only argument. Each element
    is generated by keeping the address of the endpoint and increasing the port
    number by one.
    """
    match = ENDPOINT_REGEX.match(base)
    address = match.group(1)
    port = int(match.group(2))
    while True:
        yield "tcp://%s:%d" % (address, port)
        port += 1


def sendCommand(socket, command, **kwargs):
    """Send command to the backend application using the bridge protocol

    Keyword Arguments:
    socket   -- the socket used for sending the command
    command  -- (bytes) the command to be sent
    **kwargs -- The arguments of the command (the values are serialized as JSON)
    """
    parts = [b'', command]
    for (key, value) in kwargs.items():
        parts.extend((key.encode(), json.dumps(value).encode()))
    logging.debug("Sending command: %r", parts)
    socket.send_multipart(parts)


class ProtocolError(Exception):
    """Error indicating unexpected message from bridge server"""
    pass


class MessageQueue:
    """Object for handling messages coming from the bridge server"""

    def __init__(self, socket, name, prefix, handlers):
        """Initialize message queue

        Message queue keeps a reference to the given socket and wraps it into a
        QSocketNotifier to whose signals it connects to.

        Message handlers are provided as an argument to the initialized. The
        mapping is between commands (bytes) to handler functions. The handlers
        must accept the command parameters as keyword arguments. See bridge
        protocol specification for more information about the command and
        argument concepts.

        When message is received, the first frames are first compared to the
        expected prefix. The prefix is different e.g. for control message
        replies and published event. Only if the prefix is correct, the handling
        proceeds to deserializing the arguments.

        Keyword Arguments:
        socket   -- the ZMQ socket the message queue is backed by
        name     -- the name of the queue (for logging)
        prefix   -- expected prefix for successful message
        handlers -- mapping between commands and message handlers
        """
        self._socket = socket
        self._name = str(name)
        self._prefix = list(prefix)
        self._handlers = dict(handlers)

    def handleMessages(self):
        """Notify the message queue that messages can be handled

        The message queue handles messages from its socket until there are no
        messages to receive or handling of a message results in an error. If
        handling messages stops due to an error, False is returned. Otherwise
        True is returned.
        """
        #logging.debug("Ready to receive message from %s", self._name)
        while True:
            try:
                parts = self._socket.recv_multipart(flags=zmq.NOBLOCK)
            except zmq.Again:
                return True
            try:
                self._handle_message(parts)
            except ProtocolError as e:
                logging.warning(
                    "Error while handling message from %s: %r", self._name, e)
                return False

    def _handle_message(self, parts):
        logging.debug("Received message: %r", parts)
        n_prefix = len(self._prefix)
        if parts[:n_prefix] != self._prefix:
            raise ProtocolError(
                "Unexpected message prefix in %r. Expected: %r" % (
                parts, self._prefix))
        if len(parts) < n_prefix + 1:
            raise ProtocolError(
                "Message too short: %r. Expected at least %d parts" % (
                    parts, n_prefix + 1))
        command = self._handlers.get(parts[n_prefix], None)
        if not command:
            raise ProtocolError("Unrecognized command: %r" % parts[n_prefix])
        kwargs = {}
        for n in range(n_prefix+1, len(parts)-1, 2):
            key = parts[n].decode()
            value = parts[n+1].decode()
            try:
                value = json.loads(value)
            except json.decoder.JSONDecodeError as e:
                raise ProtocolError("Error while parsing %r: %r" % (value, e))
            kwargs[key] = value
        command(**kwargs)
