
all: knn-enumerate knn-kdtree knn-parallel

knn-enumerate: knn-enumerate.cpp Makefile
	g++ -o knn-enumerate -std=gnu++11 -pthread -Wall -ggdb -O3 knn-enumerate.cpp

knn-kdtree: knn-kdtree.cpp Makefile
	g++ -o knn-kdtree -std=gnu++11 -pthread -Wall -ggdb -O3 knn-kdtree.cpp

knn-parallel: knn-parallel.cpp Makefile
	g++ -o knn-parallel -std=gnu++11 -pthread -Wall -ggdb -O3 knn-parallel.cpp
