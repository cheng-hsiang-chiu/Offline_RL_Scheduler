# Offline-RL-Scheduler

## Repository Structure
- CMakeLists.txt : cmake file
- inputs : input files
- src : source code

## Input Format
The input files have the following format.
The first line specify the number of vertices(V) and the number of edges(E).
The next V lines denote the information (id, m, n) of each vertex,
where id is the ID of the vertex, m and n are the dimensions of the matrix
processed in that vertex.
The next E lines denote the edges.
The next V lines deonte the offline worker assignment to each vertex.
For example, in the following, there are 4 vertices and 3 edges.
Vertex 0 has m = 6 and n = 7. Vertex 1 has m = 4 and n = 5.
Then the edge (0, 2) denotes that vertex 0 connecting to vertex 2.    
And vertex 0 is assigned to worker ID 39.
```
4 3           ---> V E
0 6 7         ---> for vertex 0, m is 6 and n is 7
1 4 5         ---> for vertex 1, m is 4 and n is 5
2 3 3         ---> for vertex 2, m is 3 and n is 3
3 9 5         ---> for vertex 3, m is 9 and n is 5
0 2           ---> vertex 0 points to vertex 2
1 2           ---> vertex 1 points to vertex 2
2 3           ---> vertex 2 points to vertex 3
0 39          ---> vertex 0 is assigned to worker 39
1 9           ---> vertex 1 is assigned to worker 9
2 16          ---> vertex 2 is assigned to worker 16
3 27          ---> vertex 3 is assigned to worker 27
```


## Build
To build the executable, please follow the instructions below. The default compiler is clang++.
```
mkdir build
cd build
cmake ../
make
```

## Run
To run the executable, please follow the instruction below.
```
cd build
./main < ../inputs/7.in
```


