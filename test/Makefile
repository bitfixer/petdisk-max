CC = g++
CXX = g++

CXXFLAGS = -O -Wall

SRCDIR     = ../src

clean:
	rm -f d64_test
	rm -f ../src/*.o

d64_test: $(SRCDIR)/d64_test.o $(SRCDIR)/D64DataSource.o $(SRCDIR)/fsDataSource.o $(SRCDIR)/ConsoleLogger.o
	$(CXX) $(SRCDIR)/d64_test.o $(SRCDIR)/D64DataSource.o $(SRCDIR)/fsDataSource.o $(SRCDIR)/ConsoleLogger.o -o d64_test