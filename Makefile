#Makefile

INCLUDES=-I. -O3
CXX=g++ -std=c++11

manager: manager.o
	$(CXX) $(INCLUDES) manager.o -o manager

manager.o: manager.cpp
	$(CXX) $(INCLUDES) -c manager.cpp

clean:
	rm -f manager.o manager
