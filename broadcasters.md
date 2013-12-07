One or more broadcasters (linux arm boards broadcasting sound data using RTP) are needed.

The broadcasters need access to the media source (file system, radio streams) and need 
software to play the media to a pulseaudio sink.

pulseaudio will be configured to send the sound data using RTP multicast to the local net.

More than one RTP stream may be produced by a single machine.

Example Pulseaudio setup on cubieboard running Linaro/Ubuntu
------------------------------------------------------------

sudo apt-get install pulseaudio

edit file /etc/init/pulseaudio.conf (see broadcasters/pulseaudio.conf)
