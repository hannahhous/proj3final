gomoku_server: main.cpp User.h Game.h Message.h TelnetServer.h TelnetClientHandler.h SocketUtils.h
	g++ -Wall -ansi -pedantic -std=c++17 -pthread -o gomoku_server main.cpp

clean:
	rm -f gomoku_server *.o