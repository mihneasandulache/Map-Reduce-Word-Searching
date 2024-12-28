# build:
# 		gcc main.c -o tema1 -lpthread
# clean:
# 		rm tema1

build:
	g++ -g -o tema1 tema1.cpp -lpthread
clean:
	rm tema1