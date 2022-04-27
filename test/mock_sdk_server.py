#
# Mock/test server to accept SDK Client connections and respond with properly
# formatted messages. Intended to use with unit tests for the SDK classes. Runs
# in two modes, either forever or until the command supplied as the first
# argument completes.
#
# Usage:
#   python mock_sdk_server.py
#   python mock_sdk_server.py ./test_MotionSDK
#
# Runs 5 of the Motion data services, Configurable, Preview, Sensor, Raw and
# Console each on their own port. We reuse the ports that the Motion Service
# deploys with so this test server will conflict with installed software.
#
# @file    test/mock_sdk_server.py
# @author  Luke Tokheim, luke@motionshadow.com
# @version 4.0
#
# Copyright (c) 2022, Motion Workshop
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
import random
import socketserver
import struct
import subprocess
import sys
import threading
import time

HOST = "localhost"
PORTS = (32075, 32076, 32077, 32078, 32079, 32000)


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        """Called from the socketserver to handle a single TCP request/session.

        Handle the handshaking sequence and then start streaming data as if
        there is one device named "Node" plugged in and acquiring measurements.

        Session stream:
            write(XML header)
            if conf: read(XML channel list)
            write(XML device list)
            loop:
                write(binary message)
        """
        port = self.server.server_address[1]

        is_configurable = False
        if port == 32079:
            name = "preview"
        elif port == 32078:
            name = "sensor"
        elif port == 32077:
            name = "raw"
        elif port == 32076:
            name = "configurable"
            is_configurable = True
        elif port == 32075:
            name = "console"
        else:
            name = "test"
            is_configurable = True

        # Data service always prints out its identity as an XML string.
        self.write_message(
            f'<?xml version="1.0"?><service name="{name}"/>'.encode())

        if is_configurable:
            # Configurable service requires a channel list.
            msg = self.read_message()

        # Before any sample data, the service always sends the device key to
        # string id list.
        self.write_message(
            b'<?xml version="1.0"?>'
            b'<node id="default" key="0"><node id="Node" key="1"/></node>')

        # One device. Fill out a canned message based on which data service
        # we are implementing.
        if port == 32079:
            data = struct.pack(
                "I14f", 1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
                23)
        elif port == 32078:
            data = struct.pack("I9f", 1, 10, 11, 12, 13, 14, 15, 16, 17, 18)
        elif port == 32077:
            data = struct.pack("I9h", 1, 10, 11, 12, 13, 14, 15, 16, 17, 18)
        elif port == 32076:
            data = struct.pack("2I8f", 1, 8, 10, 11, 12, 13, 14, 15, 16, 17)
        elif port == 32075:
            data = struct.pack("6s", b'\0true\n')
        else:
            if msg.find(b"header") != -1:
                buf = struct.pack("h", 1)
            elif msg.find(b"payload") != -1:
                buf = struct.pack("!I2I3f", 40, 1, 8, 10, 11, 12)
            elif msg.find(b"length") != -1:
                buf = struct.pack("!I2I3f", 65536, 1, 8, 10, 11, 12)
            elif msg.find(b"xml") != -1:
                buf = self.make_message(b'<?xm')

            self.request.sendall(buf)
            if msg.find(b"timeout") != -1:
                self.read_message()
            else:
                time.sleep(0.25)
            return

        for i in range(0, 100):
            # Binary message for one sample. Prefixed with length.
            msg = self.make_message(data)

            # Make 1-5 copies to send at random intervals. Try to test the
            # Client ability to receive messages broken into strange chunks.
            msg = b''.join([msg] * random.randint(1, 5))

            dt = 0
            j = 0
            while j < len(msg):
                n = random.randint(j, len(msg))

                try:
                    self.request.sendall(msg[j:n])
                except BrokenPipeError:
                    return
                except ConnectionAbortedError:
                    return
                j = n

                time.sleep(0.001)
                dt = dt + 0.001

            dt = 0.01 - dt
            if dt > 0:
                time.sleep(dt)

    def read_message(self):
        """Read a variable length binary message from the socket.

        All Motion SDK messages are prefixed with an unsigned int in network
        byte order, aka big-endian.

        [uint32 = N] [N bytes ...]

        Returns a byte string if sucessful, otherwise returns None.
        """
        header = self.request.recv(4)
        if header is None or len(header) != 4:
            return None

        length = struct.unpack("!I", header)[0]
        if length <= 0 or length > 65535:
            return None

        message = self.request.recv(length)
        if message is None or len(message) != length:
            return None

        return message

    def write_message(self, data):
        """Write a binary message to the socket.

        Convert the byte string data to a Motion SDK length prefixed message.

        Returns true is all of the data was sucessfully sent, otherwise returns
        false.
        """
        try:
            self.request.sendall(self.make_message(data))
            return True
        except BrokenPipeError:
            return False
        except ConnectionAbortedError:
            return False

    def make_message(self, data):
        """Convert a byte string to a Motion SDK binary message.

        Returns a byte string starting with the 4 header bytes followed by the
        data payload.
        """
        length = len(data)
        message = struct.pack("!I" + str(length) + "s", length, data)

        return message
#
# class Handler
#


class SDKServer:
    def __init__(self, address):
        self.server = socketserver.TCPServer(address, Handler)

    def start_thread(self):
        self.thread = threading.Thread(target=self.server.serve_forever)
        # Exit the server thread when the main thread terminates
        self.thread.daemon = True
        self.thread.start()

    def stop_thread(self):
        self.server.shutdown()
        self.server.server_close()

        self.thread.join()

    def join_thread(self, timeout=None):
        self.thread.join(timeout)
#
# class SDKServer
#


def main():
    random.seed()

    socketserver.TCPServer.allow_reuse_address = True

    server_list = [
        SDKServer((HOST, port))
        for port in PORTS
    ]

    for s in server_list:
        s.start_thread()

    rc = 0
    if len(sys.argv) > 1:
        rc = subprocess.call(sys.argv[1])
    else:
        while True:
            for s in server_list:
                s.join_thread(1)

    for s in server_list:
        s.stop_thread()

    sys.exit(rc)


if __name__ == "__main__":
    main()
