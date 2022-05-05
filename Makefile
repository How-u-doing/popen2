CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra
EXTRAFLAGS := -fsanitize=thread -DPOPEN_SIMPLIFIED

.PHONY: clean all

TESTS := popen_simplified_test popen2_test
TEST_OBJS := $(addsuffix .o, $(TESTS))

all: $(TESTS)

popen_simplified.o: popen_simplified.cc popen_simplified.h
	$(CXX) $(CXXFLAGS) $(EXTRAFLAGS) -g -c $<

popen_simplified_test.o: popen_simplified_test.cc popen_simplified.h
	$(CXX) $(CXXFLAGS) $(EXTRAFLAGS) -g -c $<

popen_simplified_test: popen_simplified_test.o popen_simplified.o
	$(CXX) $(CXXFLAGS) $(EXTRAFLAGS) -o $@ $^

popen2.o: popen2.cc popen2.h
	$(CXX) $(CXXFLAGS) -g -c $<

popen2_test.o: popen2_test.cc popen2.h
	$(CXX) $(CXXFLAGS) -g -c $<

popen2_test: popen2_test.o popen2.o
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TESTS) *.o
