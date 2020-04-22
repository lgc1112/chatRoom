stressTest.a : stressTest.o testEntry.o 
	g++ -o stressTest.a stressTest.o testEntry.o
testEntry.o : testEntry.cpp stressTest.h
	g++ -c testEntry.cpp
stressTest.o : stressTest.cpp stressTest.h
	g++ -c stressTest.cpp
clean :
	rm stressTest.a stressTest.o testEntry.o