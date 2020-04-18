stressTest : stressTest.o testEntry.o 
	g++ -o stressTest stressTest.o testEntry.o
testEntry.o : testEntry.cpp stressTest.h
	g++ -c testEntry.cpp
stressTest.o : stressTest.cpp stressTest.h
	g++ -c stressTest.cpp
clean :
	rm stressTest stressTest.o testEntry.o