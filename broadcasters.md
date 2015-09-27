One or more broadcasters (linux arm boards broadcasting sound data using RTP) are needed.

The broadcasters need access to the media source (file system, radio streams) and need 
software to play the media (mpd).

mpd will be configured to send the sound data to a fifo. Self-developed software
listening on the fifo will send the sound data to the local net.

More than one network stream may be produced by a single machine.
