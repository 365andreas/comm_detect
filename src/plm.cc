#include "../include/plm.h"
#include "../include/graph.h"
#include "../include/modularity.h"
#include <omp.h>
#include <chrono>

#include <map>
#include <unordered_set>

#define NUM_SPLIT 100

//TODO: 
// 1) experiment with number of threads for local move at 2 levels + search for nested parallelism
// 2) check modularity
// 3) speed up modularity computations

 void print(std::vector<int> const &input)
{
	for (int i = 0; i < ((int) input.size()); i++) {
		std::cout << input.at(i) << ' ';
	}
	std::cout << '\n';
}

 void print_f(std::vector<float> const &input)
{
        for (int i = 0; i <(float)(input.size()); i++) {
                std::cout << input.at(i) << ' ';
        }
        std::cout << '\n';
}


void print_map(std::map<int,std::vector<int>> myMap) {
for(map<int, std::vector<int>>::const_iterator it = myMap.begin(); it != myMap.end(); ++it)
{
    std::cout << "key:" << it->first << endl;
    print(it-> second);
}
}


GraphComm *PLM::coarsen(GraphComm* g_initial) {


	GraphComm *g = new GraphComm;
	std::vector<int> comm = (*g_initial).communities;
	int n = *max_element(std::begin(comm), std::end(comm)) + 1;
	g->n = n;
		
	// create the new graph

	int threads=omp_get_max_threads();
    std::vector<int> new_volumes(n, 0);

	//cout << "Inside Coarsen with " << threads << " threads, create a new graph with " << g.n << " nodes " << endl;

	network new_net(n, std::vector<pair<node_id, weight>>());

	std::vector<std::vector<int>> new_net_array(std::vector<std::vector<int>>(n, std::vector<int>(n, 0))); 

	#pragma omp parallel for num_threads(threads)
	for (int i=0; i<(*g_initial).n; i++) {
		// int tid = omp_get_thread_num();
		int c_i = comm[i];
		vector<pair<node_id, weight>> neighbors = g_initial->net[i];
		for (auto it=neighbors.begin(); it<neighbors.end(); ++it) {
			int c_j = comm[it->first];
			#pragma omp atomic update
			new_net_array[c_i][c_j] += it->second;
		}
	}

	int threads_c = std::min(n, omp_get_max_threads());

	#pragma omp parallel for num_threads(threads_c)
	for (int i=0; i<n; i++) {
		std::vector<pair<node_id, weight>> v;
		weight i_volume = 0; 
		weight w_i_k;
		for (int k=0; k<n; k++) {
			w_i_k = new_net_array[i][k];
			if (w_i_k > 0) {
				v.push_back(make_pair(k, w_i_k));
				i_volume += w_i_k;
			}
			if (k==i) 
				i_volume += w_i_k; //edge to self counts double
		}
		new_volumes[i] = i_volume;
		new_net[i]=v;
	}

	g->volumes = new_volumes;
	g->net = new_net;
	g->weight_net = (*g_initial).weight_net; //the sum of all edges remains the same

	//cout << "exit coarsen" << endl;
	return g;

}


std::vector<int> PLM::prolong(GraphComm *g_initial, std::vector<int> coarsened_comm) {

	std::vector<int> init_comm = g_initial->communities;
	std::vector<int> new_comm(g_initial->n, 0);

	#pragma omp parallel for schedule(static, NUM_SPLIT)
	for (int i=0; i<g_initial->n; i++) {
		int i_comm = init_comm[i];
		new_comm[i] = coarsened_comm[i_comm];
	}
	return new_comm;
}
std::pair <int, float> max_pair_arg (std::pair <int, float> r, std::pair <int, float> n) {
        return (n.second >= r.second) ? n : r;
}



std::pair<int, float> PLM::ReturnCommunity(int i, GraphComm *g) {

    int threads = std::min(g->n, omp_get_max_threads());
	std::vector<pair<node_id, weight>> n_i = g->net[i];
	std::vector<std::unordered_map<int,int>> weights_per_thread(threads, unordered_map<int, int>());
	int j, c=g->communities[i], c_n;	
	int comm_size = g->n;

	/* shared */
	std::vector<int> volumes(comm_size, 0);
	std::vector<pair<int, float>> results(threads, std::make_pair(c, 0.0)); /* each thread will write the best result it will find*/

	/* iterate once over all neighbors and compute weights from i to all communities.
	   Update the weights for all threads */
	for (auto neighbor_it = n_i.begin(); neighbor_it < n_i.end(); ++neighbor_it) {
		if ((int) neighbor_it->first != i) {
				c_n = g->communities[neighbor_it->first]; 
				volumes[c_n] = g->comm_volumes[c_n];
				#pragma omp parallel for num_threads(threads)
				for (j=0; j<threads; j++) 
					weights_per_thread[j][c_n] += neighbor_it->second;
		}
    }


	
	#pragma omp parallel num_threads(threads)
	{   
	    int tid = omp_get_thread_num();

	    //int tid=0;
	    std::unordered_map<int,int> t_weights = weights_per_thread[tid];
	    //std::vector<float> t_mod(comm_size, 0.0);
	    weight weight_c = weights_per_thread[tid][c];
	    weight volume_c = volumes[c] - g->volumes[i];
	    weight i_vol = g->volumes[i];
        weight n_w = g->weight_net;
        float n_w_float = n_w;
	    float double_sqr_n_w = 2 * n_w * n_w;
	    float i_vol_divided = i_vol / double_sqr_n_w;
	    float weight_c_divided = weight_c / n_w_float;

	    std::pair<int, float> max_pair = std::make_pair(c, 0.0);
		std::pair<int, float> pair;	
	    float a, b, dmod;
	
		for (size_t c = 0; c < t_weights.bucket_count(); c++) {
			for (auto bi = t_weights.begin(c); bi != t_weights.end(c); bi++) {
				pair = *bi;
				a =  (pair.second / n_w_float) - weight_c_divided;
			    b = (volume_c - volumes[pair.first]) * i_vol_divided;
				dmod = a + b;
				if (dmod > max_pair.second) {
					max_pair.first=pair.first;
					max_pair.second=dmod;
				}
		
			}
		}
		results[tid] = max_pair;
	}
	
	std::pair<int, float> max_p = std::make_pair(c, 0.0);

    for (j=0; j<threads; j++) {
		if (results[j].second > max_p.second) 
			max_p = results[j];
	}
	return max_p;
}


std::unordered_map<int, int> PLM::Map_communities(GraphComm *g) {

	std::unordered_set<int> comms;
	std::unordered_map<int,int> com_map;
	for (int i=0; i<g->n; i++) {
		comms.insert(g->communities[i]);
	}
	// cout << "comms.size(): " << comms.size() << endl;

	int i = 0;
	for (auto it:comms) {
		com_map.insert(std::pair<int,int>(it, i));
		i++;
	}

	return com_map;

}


void  PLM::Local_move(GraphComm* graph) {


	int unstable = 1;
	int iterations=0;
	int threads = std::min(graph->n, omp_get_max_threads());
	cout << "Inside LM with " << threads << " threads, and " << graph->n << " nodes " << endl;


	// when Local Move is called, each node alone is a community
	std::vector<int> volumes(graph->n, 0);	
	#pragma omp parallel for
	for (int j=0; j<graph->n; j++) 
              volumes[j] = graph->volumes[j];

	graph->comm_volumes = volumes;


	auto start = std::chrono::system_clock::now();	
	while (unstable) {
		cout << "iteration:" << iterations++ << endl;
		//print(graph->communities);
		//cout << "----------------------------" << endl;
		unstable = 0;
		#pragma omp parallel for
		for (int i=0; i<graph->n; i++) {
			int i_comm = graph->communities[i];
			std::pair<int, float> res = ReturnCommunity(i, graph);
			if (res.first != i_comm) { 
				graph->communities[i] = res.first;
				#pragma omp atomic write
				unstable=1;
				#pragma omp critical 
				{
					graph->comm_volumes[res.first] += graph->volumes[i];
					graph->comm_volumes[i_comm] -= graph->volumes[i]; 
				
				}
			}

		}


	}
	auto end = std::chrono::system_clock::now();
	auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
	std::cout << "comm detection took time (in us) : " << total_time << std::endl;

    if (iterations > 1) {
		start = std::chrono::system_clock::now();
		std::unordered_map<int, int> com_map = Map_communities(graph);
		#pragma omp parallel for
		for (int i=0; i<(*graph).n; i++)
			(*graph).communities[i] = com_map[(*graph).communities[i]];
		end = std::chrono::system_clock::now();
		total_time = std::chrono::duration_cast<
				std::chrono::microseconds>(end - start).count();
		std::cout << "map comm took time (in us) : " << total_time << std::endl;
	}
}


std::vector<int> PLM::Recursive_comm_detect(GraphComm *g) {


	std::vector<int> c_singleton(g->n, 0);
	# pragma omp parallel for schedule(static, NUM_SPLIT)
	for (int i=0; i<g->n; i++) {
		c_singleton[i] = i;
	}
	g->communities = c_singleton;
	Local_move(g);


	/*auto start = std::chrono::system_clock::now();
	GraphComm *g_new = coarsen(g);
	auto end = std::chrono::system_clock::now();

	auto total_time = std::chrono::duration_cast<
			std::chrono::microseconds>(end - start).count();
	std::cout << "coarsen took time (in us) : " << total_time << std::endl;*/

    
	if (g->communities != c_singleton) {
		GraphComm *g_new = coarsen(g);
		std::vector<int> c_coarsened = Recursive_comm_detect(g_new);
		g->communities = prolong(g, c_coarsened);
	}
	return g->communities;

}

void get_weight_and_volumes(GraphComm* g) {
	weight sum = 0;
	std::vector<int> volumes(g->n, 0);
	node_id u = 0;
	# pragma omp parallel for schedule(static, NUM_SPLIT)
	for (u = 0; u < (node_id) g->n; u++) {
		vector<pair<node_id, weight>> neighbors = g->net[u];
		weight vol = 0;
		weight my_weight=0;
		//add the weight of the neighbors to the total sum
		for (vector<pair<node_id, weight>>::iterator j = neighbors.begin(); j != neighbors.end(); ++j) {
			vol += j->second;
			if (j->first == u)
				my_weight = j->second;
		}
		volumes[u]=vol + my_weight;
		#pragma omp atomic update
		sum += vol;
    	}
    	g->weight_net = sum;
	g->volumes = volumes;
}


void PLM::DetectCommunities() {

	get_weight_and_volumes(&graph);
	//cout << "weight: " << graph.weight_net << endl;
	graph.communities = Recursive_comm_detect(&graph);
	cout << "final communities: ";
	//print(graph.communities);
	int found_comm = *std::max_element(std::begin(graph.communities), std::end(graph.communities)) + 1;
	cout << "found " << found_comm << " communities" << endl;
}

// the same for LP and Louvain. TODO: defined once
void PLM::PrintCommunities(const std::string &file_name) {
	std::ofstream ofs;
	ofs.open(file_name, std::ios_base::out | std::ios_base::trunc);
	if (ofs.fail()) {
		std::cerr << "Error opening file " << file_name << std::endl;
		std::exit(2);
	}

	for (int i = 0; i < graph.n; i++) {
		ofs << graph.communities[i] << std::endl;
	}
}
