#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# HTTP Server Sent Events client
# This program connects to a HTTP server and subscribes to a stream of events.
# It receives the events and logs them to a file. It automatically retries
# the connection if it is lost.
# Author: Paul Abbott

import socket
import argparse
import re
import logging
import logging.handlers
import time  # Add this import for retry delays

DEF_PORT = 8080
DEF_PATH = "/log-events"

SSE_FIELD_SEPARATOR = ':'

def escape_ansi(line):
	ansi_escape = re.compile(r'(?:\x1B[@-_]|[\x80-\x9F])[0-?]*[ -/]*[@-~]')
	return ansi_escape.sub('', line)

def log_setup():
	logfile_handler = logging.handlers.RotatingFileHandler('my.log', maxBytes=1000000, backupCount=99)
	logfile_formatter = logging.Formatter('%(asctime)s %(message)s')
	logfile_handler.setFormatter(logfile_formatter)
	logger = logging.getLogger()
	logger.addHandler(logfile_handler)
	logger.setLevel(logging.DEBUG)
	logfile_handler.doRollover() # force rollover to create a new log file at each run
	# Also send logs to console
	consoleHandler = logging.StreamHandler()
	consoleHandler.setFormatter(logfile_formatter)
	logger.addHandler(consoleHandler)

def run_sse_client(host, port, path):
	# Create a TCP socket
	sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

	# Connect to the server
	sock.connect((host, port))

	# Send the HTTP GET request
	request = f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\nAccept: text/event-stream\r\n\r\n"
	sock.sendall(request.encode('utf-8'))
	logging.info("***************  Connected!  ***************")

	while True:
		# Set a timeout for the socket, since server will send keepalive messages every 10 seconds.
		sock.settimeout(15)
		recvstr = sock.recv(1024)
		if not recvstr:
			break # timeout or connection closed
		recvstr = recvstr.decode('utf-8')
		event_type = None
		event_data = None
		for line in recvstr.splitlines():
			# Lines starting with a separator are comments and are to be ignored.
			if not line.strip() or line.startswith(SSE_FIELD_SEPARATOR):
				continue
			parts = line.split(SSE_FIELD_SEPARATOR, 1)
			field = parts[0].strip()
			if len(parts) < 2:
				continue
			if "event" == field:
				event_type = parts[1].strip()
			elif "data" == field:
				event_data = parts[1].strip()

		if "keepalive" == event_type:
			continue
		if "log-line" == event_type:
			if event_data:
				event_data = escape_ansi(event_data) # remove ANSI color codes
				event_data = event_data.rstrip() # remove trailing whitespace and newlines
				logging.info(event_data)


def run_sse_client_with_retries(host, port, path, retry_delay=5):
	while True:
		try:
			run_sse_client(host, port, path)
		except (socket.error, ConnectionError) as e:
			logging.error(f"***************  Connection error: {e}. Retrying in {retry_delay} seconds...")
			time.sleep(retry_delay)
		except Exception as e:
			logging.error(f"***************  Unexpected error: {e}. Retrying in {retry_delay} seconds...")
			time.sleep(retry_delay)

if __name__ == "__main__":
	parser = argparse.ArgumentParser()
	parser.add_argument('addr', type=str, help='http server address')
	parser.add_argument("-p", "--port", type=int, help='http server port', default=DEF_PORT)
	parser.add_argument("--path", type=str, help='http server path', default=DEF_PATH)

	args = parser.parse_args()
	log_setup()
	logging.debug("+==========================+")
	logging.debug("| HTTP SSE Logging Client  |")
	logging.debug("+==========================+")
	logging.debug("Connecting to {}:{}".format(args.addr, args.port))
	logging.debug("Logging started. Press Ctrl-C to stop.")
	logging.debug("")
	logging.debug("")

	run_sse_client_with_retries(host = args.addr, port = args.port, path = args.path)
