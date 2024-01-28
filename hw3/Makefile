CC = g++
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv4`

AGENT_SRC = agent.cpp
SENDER_SRC = sender.cpp
RECEIVER_SRC = receiver.cpp
CRC32_SRC = crc32.cpp
AGENT_EXEC = agent
SENDER_EXEC = sender
RECEIVER_EXEC = receiver
CRC32_EXEC = crc32

all: agent sender receiver crc32

agent:
	$(CC) $(AGENT_SRC) -lz -o $(AGENT_EXEC)
sender:
	$(CC) $(SENDER_SRC) -lz -o $(SENDER_EXEC) $(INCLUDE_OPENCV)
receiver:
	$(CC) $(RECEIVER_SRC) -lz -o $(RECEIVER_EXEC) $(INCLUDE_OPENCV)
crc32:
	$(CC) $(CRC32_SRC) -lz -o $(CRC32_EXEC)

.PHONY: clean

clean:
	rm $(AGENT_EXEC) $(SENDER_EXEC) $(RECEIVER_EXEC) $(CRC32_EXEC)