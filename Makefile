CXX = g++
CXXFLAGS = -O2 -Wall -Wextra -std=c++17

TARGET = sender
SRCS = sender.cc netsim_lib.cc

all: $(TARGET)

$(TARGET): $(SRCS) netsim.h
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET) *.o
