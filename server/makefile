server.a : myServer.o myServerEntry.o
	g++ -o server.a myServerEntry.o myServer.o 
myServerEntry.o : myServerEntry.cpp myServer.h
	g++ -c myServerEntry.cpp  
myServer.o : myServer.cpp myServer.h
	g++ -c myServer.cpp
clean :
	rm myServer.o myServerEntry.o server.a