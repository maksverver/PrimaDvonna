#!/bin/bash

# Run the player as a client connecting to a TCP socket.
# Accepts optional hostname and port arguments. 

${1:-./player} <>/dev/tcp/${2:-hell.student.utwente.nl}/${3:-2727} >&0
