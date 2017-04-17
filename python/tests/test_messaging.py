import unittest

from PyQt5.QtTest import QSignalSpy
import zmq.decorators
import zmq

from bridgegui.messaging import (
    endpoints, sendCommand, MessageQueue, validateControlReply)

ENDPOINT = 'inproc://testing'
COMMAND = b'command'

EMPTY_FRAME = b''
REPLY_SUCCESS_PREFIX = [EMPTY_FRAME, b'\0\0\0\0']


class MessagingTest(unittest.TestCase):
    """Unit test suite for messaging"""

    def testEndpoints(self):
        gen = endpoints("tcp://127.0.0.1:5555")
        self.assertEqual(next(gen), "tcp://127.0.0.1:5555")
        self.assertEqual(next(gen), "tcp://127.0.0.1:5556")


class MessageQueueTest(unittest.TestCase):
    """Unit test suite for message queue"""

    def setUp(self):
        self._zmqctx = zmq.Context()
        self._back_socket = self._zmqctx.socket(zmq.PAIR)
        self._back_socket.bind(ENDPOINT)
        self._front_socket = self._zmqctx.socket(zmq.PAIR)
        self._front_socket.connect(ENDPOINT)
        self._message_queue = MessageQueue(
            self._back_socket, "test message queue",
            validateControlReply, { COMMAND: self._handle_command })
        self._command_handled = False

    def tearDown(self):
        self._zmqctx.destroy()

    def testSendCommand(self):
        sendCommand(self._front_socket, COMMAND, arg=1)
        self.assertEqual(
            self._back_socket.recv_multipart(flags=zmq.NOBLOCK),
            [b'', COMMAND, b'arg', b'1'])

    def testIncorrectPrefix(self):
        self._front_socket.send_multipart((b'this', b'is', b'incorrect'))
        self.assertFalse(self._message_queue.handleMessages())

    def testFailedStatusCode(self):
        self._front_socket.send_multipart(
            (b'', b'\xff\xff\xff\xff', COMMAND, b'arg', b'123'))
        self.assertFalse(self._message_queue.handleMessages())

    def testMissingCommand(self):
        self._front_socket.send_multipart(REPLY_SUCCESS_PREFIX)
        self.assertFalse(self._message_queue.handleMessages())

    def testIncorrectCommand(self):
        self._front_socket.send_multipart(REPLY_SUCCESS_PREFIX + [b'incorrect'])
        self.assertFalse(self._message_queue.handleMessages())

    def testIncorrectSerialization(self):
        self._front_socket.send_multipart(REPLY_SUCCESS_PREFIX + [b'incorrect'])
        self.assertFalse(self._message_queue.handleMessages())

    def testIncorrectArgument(self):
        self._front_socket.send_multipart(
            REPLY_SUCCESS_PREFIX + [COMMAND, b'arg', b'not json'])
        self.assertFalse(self._message_queue.handleMessages())

    def testHandleMessage(self):
        self._front_socket.send_multipart(
            REPLY_SUCCESS_PREFIX + [COMMAND, b'arg', b'123'])
        self.assertTrue(self._message_queue.handleMessages())
        self.assertTrue(self._command_handled)

    def _handle_command(self, arg):
        self.assertEqual(arg, 123)
        self._command_handled = True
