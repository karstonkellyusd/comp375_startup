CXX = g++
CXXFLAGS = -g -Wall -Wextra -std=c++17 -pthread

TARGETS = producer-consumer cv_example
PC_SRC = producer-consumer.cpp BoundedBuffer.cpp

all: $(TARGETS)

producer-consumer: $(PC_SRC) BoundedBuffer.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(PC_SRC)

clean:
	$(RM) $(TARGETS)
