all: fifo-to-network

fifo-to-network: cmdline.o fifo-to-network.o
	$(CXX) -o $@ $^

cmdline.c cmdline.h: fifo-to-network.ggo
	gengetopt -i $^

cmdline.o: cmdline.c cmdline.h
