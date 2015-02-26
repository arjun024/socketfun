CC = gcc

CFLAGS  = -lpthread -std=c90 -Wall

SERVER_TARGET = chatserver
CLIENT_TARGET = chatclient

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(SERVER_TARGET): $(SERVER_TARGET).c
	$(CC) $(CFLAGS) -o $(SERVER_TARGET) $(SERVER_TARGET).c

$(CLIENT_TARGET): $(CLIENT_TARGET).c
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_TARGET).c

clean:
	rm $(SERVER_TARGET) $(CLIENT_TARGET)
