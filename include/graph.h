#ifndef GRAPH_H
#define GRAPH_H

#include <assert.h>

#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>

#include <map>

#include "../include/network.h"

class GraphComm {

	public:
		GraphComm(): n(0), m(0) {};

		~GraphComm() {}

		/* initialize graph based on a given input file */
		void Init(const std::string &in_file);

		/* write community assigned to each node in a file */
		void ProduceResult(const std::string &out_file);

		/* print the adjacency list (debugging purposes) */
		void PrintGraph();

		/* print the network structure (debugging purposes) */
		void PrintNetwork();

		/* initialize graph based on the input file. Create a network structure */
		void Net_init(const std::string &file_name);

		/* number of vertices */
		int n;
		/* number of edges */
		unsigned int m;

		/* adjacency list of graph */
		std::vector<std::vector<int>> adj_list;

		/* adjacency matrix graph */
		int *adj_matrix;

		/* community of each node */
		std::vector<int> communities;

		/* volumes of communities */
		std::vector<int> comm_volumes;

		/* used by MPI - community of each node */
		int *communities_array;

		/* compute the volume of each node only once*/
		std::vector<int> volumes;

		/* used by MPI - mapping of communities during 'coarsen' stage */
		int *volumes_array;

		/* turn graph into a network */
		network CreateNetwork();

		/* contains elements for each community */
		std::map<int,std::vector<int>> comm_population;

		/* compute the total weight of the network only once + save it */
		long long weight_net;

		/* 2 * weight_net * weight_net */
		long long weight_sq;

		/* 0 -> plp, 1 -> plm */ 
		int algo;

		float weight_devided;

		network net;


};



#endif 			// GRAPH_H
