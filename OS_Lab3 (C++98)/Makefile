CXX = g++

CXXFLAGS = -Wall -Wextra -std=c++98 -D_CRT_SECURE_NO_WARNINGS

TARGETS = Main.exe

SRC_MAIN = Main.cpp

OBJ_MAIN = Main.o

all: $(TARGETS)

Main.exe: $(OBJ_MAIN)
	$(CXX) $(CXXFLAGS) -o $@ $^

Main.o: $(SRC_MAIN)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Очистка
clean:
	rm -f $(OBJ_MAIN) $(TARGETS)

.PHONY: all clean