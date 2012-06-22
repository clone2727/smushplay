#TODO: Make a saner build environment

INCLUDES = -I. `sdl-config --cflags`
LIBS = `sdl-config --libs`

all:
	g++ $(INCLUDES) -Wall -g -c smushplay.cpp -o smushplay.o
	g++ $(INCLUDES) -Wall -g -c graphicsman.cpp -o graphicsman.o
	g++ $(INCLUDES) -Wall -g -c fileutil.cpp -o fileutil.o
	g++ $(INCLUDES) -Wall -g -c smushvideo.cpp -o smushvideo.o
	g++ $(INCLUDES) -Wall -g -c codec37.cpp -o codec37.o
	g++ $(INCLUDES) -Wall -g -c codec47.cpp -o codec47.o
	g++ $(INCLUDES) -Wall -g -c blocky16.cpp -o blocky16.o
	g++ $(INCLUDES) -Wall -g -c util.cpp -o util.o
	g++ $(INCLUDES) -Wall -g -c audioman.cpp -o audioman.o
	g++ $(INCLUDES) -Wall -g -c audiostream.cpp -o audiostream.o
	g++ $(INCLUDES) -Wall -g -c rate.cpp -o rate.o
	g++ $(INCLUDES) -Wall -g -c pcm.cpp -o pcm.o
	g++ $(INCLUDES) -Wall -g -c vima.cpp -o vima.o
	g++ $(LIBS) -o smushplay smushplay.o graphicsman.o fileutil.o smushvideo.o codec37.o codec47.o blocky16.o util.o audioman.o audiostream.o rate.o pcm.o vima.o

clean:
	rm -f *.o
	rm -f smushplay
