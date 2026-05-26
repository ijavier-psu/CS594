CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
LDFLAGS = -lsqlite3

server:
	$(CXX) $(CXXFLAGS) server.cpp -o server $(LDFLAGS)

client: server
	$(CXX) client.cpp -o client
	

all: server

clean:
	rm -f $(TARGET)
