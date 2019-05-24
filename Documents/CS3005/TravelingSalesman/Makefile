CXX := g++
# debuGging
#CXXFLAGS := -std=c++11 -g
CXXFLAGS := -std=c++11 
#CXXFLAGS := -std=c++11 -O

# $@ == target (left hand side of colon)
# $^ == all dependencies (right hand side of colon)
TSP-3: TSP.o graph.o
	$(CXX) $(CXXFLAGS) -o $@ $^

# $< == first dependency (first on the right side of the colon)
TSP.o: TSP.cpp graph.h
	$(CXX) $(CXXFLAGS) -c $<

graph.o: graph.cpp graph.h
	$(CXX) $(CXXFLAGS) -c $<

memtest: TSP
	valgrind --leak-check=full ./TSP 

clean:
	-rm -f *.o
	-rm -f TSP
	-rm -f *~