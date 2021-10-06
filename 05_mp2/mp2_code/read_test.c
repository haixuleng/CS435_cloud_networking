#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_to_x(int* array, int s, int x){
    // initialize an array of size s to value x
    // return: none, update array
    for(int i = 0; i < s; i++){
        array[i] = x;
    }
}

void read_cost(char* file_name, int* cost){
    // arguments: file_name, pointer to the array saving cost for nodes
    // return: none, update the cost array
    // read the cost values from a file and assign them to cost
    FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    const char s[2] = " ";
    char *token;
    fp = fopen(file_name, "r");
    if (fp == NULL){
        init_to_x(cost, 256, 1);
        return;
    }
    while ((read = getline(&line, &len, fp)) != -1) {
        //split the line by a space, the first value is the node index
        // the second value is the cost
        token = strtok(line, s);
        int node = atoi(token);
        token = strtok(NULL, s);
        int node_cost = atoi(token);
        cost[node] = node_cost;
    }
    fclose(fp);
    if (line)
        free(line);
}

int main(){
    int cost[256] = {1};
    init_to_x(cost, 256, 1);
    // there are up to 255 different routers in this case
    char file_name[255] = "example_topology/example_topology/testinitcosts10";
    read_cost(file_name, cost);
    for(int i = 0; i < 256; i++){
        printf("%d", cost[i]);
        printf(" ");
    }
    printf("\n");
    //printf()
    return 0;
}