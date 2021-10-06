all: ls

ls: main.c monitor_neighbors.h
	gcc -pthread -o ls_router main.c monitor_neighbors.h -g

.PHONY: clean
clean:
	rm *.o vec_router ls_router
