CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
LDFLAGS = -lsqlite3


server: server.cpp server.h database.cpp database.h
	$(CXX) $(CXXFLAGS) server.cpp database.cpp -o server $(LDFLAGS)

client: client.cpp database.h
	$(CXX) client.cpp database.cpp -o client $(LDFLAGS)
	
all: server client

clean:
	rm -f server client
