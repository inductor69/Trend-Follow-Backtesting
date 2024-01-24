# Define your compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11

# Target executable name
TARGET = main

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
