CC=			g++
CFLAGS=		-lncurses -lpthread -std=c++11
TARGETS=	netpong

all: $(TARGETS)
	@echo "-*-*-*- Make completed succesfully -*-*-*-"

netpong: netpong.cpp
	@echo "Compiling $@..."
	$(CC) $(CFLAGS) $^ -o $@

clean:
	@echo "Cleaning..."
	@rm -vf $(TARGETS)
