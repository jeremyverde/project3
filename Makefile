#Makefile

INCLUDES=-I. -O3
CXX=g++ -std=c++11

manager: manager.o router.o
	$(CXX) $(INCLUDES) manager.o router.o -o manager

manager.o: manager.cpp router.cpp
	$(CXX) $(INCLUDES) -c manager.cpp router.cpp

clean:
	rm -f manager.o manage *.out
