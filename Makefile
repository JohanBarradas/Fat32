

# *****************************************************
# Variables to control Makefile operation

CXX = g++
CXXFLAGS = -Wall -g
OBJDIR = obj
# ****************************************************
# Targets needed to bring the executable up to date

mfs: obj/mfs.o obj/helper.o
	$(CXX) $(CXXFLAGS) -o mfs obj/mfs.o obj/helper.o

$(OBJDIR)/mfs.o: src/mfs.cpp include/helper.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJDIR)/helper.o: src/helper.cpp include/helper.h
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f helper.o mfs mfs.o


#Rectangle.o: Rectangle.h Point.h
