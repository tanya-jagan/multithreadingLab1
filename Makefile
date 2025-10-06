CXX = g++
CXXFLAGS = -std=c++17 -pthread -O2 -Wall

all: driver

driver: driver.cpp
	$(CXX) $(CXXFLAGS) -o driver driver.cpp

clean:
	rm -f driver
