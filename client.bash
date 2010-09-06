#!/bin/bash

# Run the player as a client connecting to a TCP socket.
# Accepts optional hostname and port arguments. 

./player <>/dev/tcp/${1:-hell.student.utwente.nl}/${2:-2727} >&0
