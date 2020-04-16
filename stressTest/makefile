testEntry : stressTest.o testEntry.o 
	g++ -o testEntry stressTest.o testEntry.o
testEntry.o : testEntry.cpp stressTest.h
	g++ -c testEntry.cpp
stressTest.o : stressTest.cpp stressTest.h
	g++ -c stressTest.cpp
clean :
	rm testEntry stressTest.o testEntry.o