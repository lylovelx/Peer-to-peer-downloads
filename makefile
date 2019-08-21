all:Server Client
flag=-lboost_system -lboost_filesystem -lpthread

Server:Server.cpp 
	g++  -std=c++0x -I . $^ -o $@ $(flag) 
Client:Client.cpp
	g++  -std=c++0x -I . $^ -o  $@ $(flag) 
