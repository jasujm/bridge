"""Messaging utilities for the bridge frontend"""

import re
import logging
import struct

import json
import zmq

EMPTY_FRAME = b''
REPLY_SUCCESS_PREFIX = [EMPTY_FRAME, b'success']

ENDPOINT_REGEX = re.compile(r"tcp://(.+):(\d+)")


def _failed_status_code(code):
    PACK_FMT = '>l'
    if len(code) != struct.calcsize(PACK_FMT):
        return False
    code = struct.unpack(PACK_FMT, code)
    return code[0] < 0


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
    try:
        socket.send_multipart(parts)
    except zmq.ZMQError as e:
        logging.error(
            "Error %d while sending message %r: %s", e.errno, parts, str(e))


def validateControlReply(parts):
    """Validate control message reply

    The function checks if the parts given as argument contain one empty frame
    and successful (nonnegative) status code. If yes, the prefix is stripped and
    the rest of the parts returned. Otherwise None is returned.

    Keyword Arguments:
    parts -- the message frames (list of bytes)
    """
    if (len(parts) < 2 or parts[0] != EMPTY_FRAME or
        _failed_status_code(parts[1])):
        return None
    return parts[2:]


def validateEventMessage(parts):
    """Validate event message

    This function just returns its arguments as is (there is nothing to validate
    about event message).
    """
    return parts


class ProtocolError(Exception):
    """Error indicating unexpected message from bridge server"""
    pass


class MessageQueue:
    """Object for handling messages coming from the bridge server"""

    def __init__(self, socket, name, validator, handlers):
        """Initialize message queue

        Message queue keeps a reference to the given socket and wraps it into a
        QSocketNotifier to whose signals it connects to.

        Message handlers are provided as an argument to the initialized. The
        mapping is between commands (bytes) to handler functions. The handlers
        must accept the command parameters as keyword arguments. See bridge
        protocol specification for more information about the command and
        argument concepts.

        When message is received, it is first validated using the validator
        given as argument. Replies to control messages can be validated using
        validateControlReply and events can be validated using the (trivial)
        validateEventMessage.

        Keyword Arguments:
        socket    -- the ZMQ socket the message queue is backed by
        name      -- the name of the queue (for logging)
        validator -- Function for validating successful message
        handlers  -- mapping between commands and message handlers
        """
        self._socket = socket
        self._name = str(name)
        self._validator = validator
        self._handlers = dict(handlers)

    def handleMessages(self):
        """Notify the message queue that messages can be handled

        The message queue handles messages from its socket until there are no
        messages to receive or handling of a message results in an error. If
        handling messages stops due to an error, False is returned. Otherwise
        True is returned.
        """
        ret = True
        while self._socket.events & zmq.POLLIN:
            try:
                parts = self._socket.recv_multipart()
            except zmq.ContextTerminated: # It's okay as we're about to exit
                return True
            except zmq.ZMQError as e:
                logging.error(
                    "Error %d while receiving message from %s: %s",
                    e.errno, self._name, str(e))
                ret = False
            else:
                try:
                    self._handle_message(parts)
                except ProtocolError as e:
                    logging.warning(
                        "Unexpected event while handling message %r from %s: %s",
                        parts, self._name, str(e))
                    ret = False
        return ret

    def _handle_message(self, parts):
        logging.debug("Received message: %r", parts)
        parts = self._validator(parts)
        if not parts:
            raise ProtocolError("Invalid message parts: %r" % parts)
        command = self._handlers.get(parts[0], None)
        if not command:
            raise ProtocolError("Unrecognized command: %r" % parts[0])
        kwargs = {}
        for n in range(1, len(parts)-1, 2):
            key = parts[n].decode()
            value = parts[n+1].decode()
            try:
                value = json.loads(value)
            except json.decoder.JSONDecodeError as e:
                raise ProtocolError("Error while parsing %r: %r" % (value, e))
            kwargs[key] = value
        command(**kwargs)
