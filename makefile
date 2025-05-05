all:
	g++ -o start -std=c++11 -I$(SUMO_HOME)/src -I$(SUMO_HOME)/build/src start.cpp -L$(SUMO_HOME)/bin -ltracicpp
	gcc -o app app.c
	gcc -o auto light.c