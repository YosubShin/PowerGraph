/*
 * Copyright (c) 2009 Carnegie Mellon University.
 *     All rights reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing,
 *  software distributed under the License is distributed on an "AS
 *  IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 *  express or implied.  See the License for the specific language
 *  governing permissions and limitations under the License.
 *
 * For more about this software visit:
 *
 *      http://www.graphlab.ml.cmu.edu
 *
 */

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <vector>
#include <map>
#include <time.h>

#include <graphlab.hpp>

//helper function
float myrand() {
  return static_cast<float>(rand() / (RAND_MAX + 1.0));
}

//helper function to return a hash value for Flajolet & Martin bitmask
size_t hash_value() {
  size_t ret = 0;
  while (myrand() < 0.5) {
    ret++;
  }
  return ret;
}

const size_t DUPULICATION_OF_BITMASKS = 10;

struct vdata {
  //use two bitmasks for consistency
  std::vector<std::vector<bool> > bitmask1;
  std::vector<std::vector<bool> > bitmask2;
  //indicate which is the bitmask for reading (or writing)
  bool odd_iteration;
  vdata() :
      bitmask1(), bitmask2(), odd_iteration(true) {
  }
  //for exact counting (but needs large memory)
  void create_bitmask(size_t id) {
    std::vector<bool> mask1(id + 2, 0);
    mask1[id] = 1;
    bitmask1.push_back(mask1);
    std::vector<bool> mask2(id + 2, 0);
    mask2[id] = 1;
    bitmask2.push_back(mask2);
  }
  //for approximate Flajolet & Martin counting
  void create_hashed_bitmask(size_t id) {
    for (size_t i = 0; i < DUPULICATION_OF_BITMASKS; ++i) {
      size_t hash_val = hash_value();
      std::vector<bool> mask1(hash_val + 2, 0);
      mask1[hash_val] = 1;
      bitmask1.push_back(mask1);
      std::vector<bool> mask2(hash_val + 2, 0);
      mask2[hash_val] = 1;
      bitmask2.push_back(mask2);
    }
  }

  void save(graphlab::oarchive& oarc) const {
    size_t num = bitmask1.size();
    oarc << num;
    for (size_t a = 0; a < num; ++a) {
      size_t size = bitmask1[a].size();
      oarc << size;
      for (size_t i = 0; i < size; ++i)
        oarc << (bool)bitmask1[a][i];
      for (size_t i = 0; i < size; ++i)
        oarc << (bool)bitmask2[a][i];
    }
    oarc << odd_iteration;
  }
  void load(graphlab::iarchive& iarc) {
    bitmask1.clear();
    bitmask2.clear();
    size_t num = 0;
    iarc >> num;
    for (size_t a = 0; a < num; ++a) {
      size_t size = 0;
      iarc >> size;
      std::vector<bool> mask1;
      for (size_t i = 0; i < size; ++i) {
        bool element = true;
        iarc >> element;
        mask1.push_back(element);
      }
      bitmask1.push_back(mask1);
      std::vector<bool> mask2;
      for (size_t i = 0; i < size; ++i) {
        bool element = true;
        iarc >> element;
        mask2.push_back(element);
      }
      bitmask2.push_back(mask2);
    }
    iarc >> odd_iteration;
  }
};

typedef graphlab::distributed_graph<vdata, graphlab::empty> graph_type;

//initialize bitmask
void initialize_vertex(graph_type::vertex_type& v) {
  v.data().create_bitmask(v.id());
}
//initialize bitmask
void initialize_vertex_with_hash(graph_type::vertex_type& v) {
  v.data().create_hashed_bitmask(v.id());
}

//helper function to compute bitwise-or
void bitwise_or(std::vector<std::vector<bool> >& v1,
    const std::vector<std::vector<bool> >& v2) {
  for (size_t a = 0; a < v1.size(); ++a) {
    while (v1[a].size() < v2[a].size()) {
      v1[a].push_back(false);
    }
    for (size_t i = 0; i < v2[a].size(); ++i) {
      v1[a][i] = v1[a][i] || v2[a][i];
    }
  }
}

struct bitmask_gatherer {
  std::vector<std::vector<bool> > bitmask;

  bitmask_gatherer() :
    bitmask() {
  }
  explicit bitmask_gatherer(const std::vector<std::vector<bool> > & in_b) :
    bitmask(){
    for(size_t i=0;i<in_b.size();++i){
      bitmask.push_back(in_b[i]);
    }
  }

  //bitwise-or
  bitmask_gatherer& operator+=(const bitmask_gatherer& other) {
    bitwise_or(bitmask, other.bitmask);
    return *this;
  }

  void save(graphlab::oarchive& oarc) const {
    size_t num = bitmask.size();
    oarc << num;
    for (size_t a = 0; a < num; ++a) {
      size_t size = bitmask[a].size();
      oarc << size;
      for (size_t i = 0; i < size; ++i)
        oarc << (bool)bitmask[a][i];
    }
  }
  void load(graphlab::iarchive& iarc) {
    bitmask.clear();
    size_t num = 0;
    iarc >> num;
    for (size_t a = 0; a < num; ++a) {
      size_t size = 0;
      iarc >> size;
      std::vector<bool> mask1;
      for (size_t i = 0; i < size; ++i) {
        bool element = true;
        iarc >> element;
        mask1.push_back(element);
      }
      bitmask.push_back(mask1);
    }
  }
};

//The next bitmask b(h + 1; i) of i at the hop h + 1 is given as:
//b(h + 1; i) = b(h; i) BITWISE-OR {b(h; k) | source = i & target = k}.
class one_hop: public graphlab::ivertex_program<graph_type, bitmask_gatherer>,
    public graphlab::IS_POD_TYPE {
public:
  //gather on out edges
  edge_dir_type gather_edges(icontext_type& context,
      const vertex_type& vertex) const {
    return graphlab::OUT_EDGES;
  }

  //for each edge gather the bitmask of the edge
  bitmask_gatherer gather(icontext_type& context, const vertex_type& vertex,
      edge_type& edge) const {
    if (vertex.data().odd_iteration) {
      return bitmask_gatherer(edge.target().data().bitmask2);
    } else {
      return bitmask_gatherer(edge.target().data().bitmask1);
    }
  }

  //get bitwise-ORed bitmask and switch bitmasks
  void apply(icontext_type& context, vertex_type& vertex,
      const gather_type& total) {
    if (vertex.data().odd_iteration) {
      if (total.bitmask.size() > 0)
        bitwise_or(vertex.data().bitmask1, total.bitmask);
      vertex.data().odd_iteration = false;
    } else {
      if (total.bitmask.size() > 0)
        bitwise_or(vertex.data().bitmask2, total.bitmask);
      vertex.data().odd_iteration = true;
    }
  }

  edge_dir_type scatter_edges(icontext_type& context,
      const vertex_type& vertex) const {
    return graphlab::NO_EDGES;
  }
  void scatter(icontext_type& context, const vertex_type& vertex,
      edge_type& edge) const {
  }
};

//copy the updated bitmask to the other
void copy_bitmasks(graph_type::vertex_type& vdata) {
  if (vdata.data().odd_iteration == false) { //odd_iteration has just finished
    vdata.data().bitmask2 = vdata.data().bitmask1;
  } else {
    vdata.data().bitmask1 = vdata.data().bitmask2;
  }
}

//count the number of vertices reached in the current hop
size_t absolute_vertex_data(const graph_type::vertex_type& vertex) {
    size_t count = 0;
    for (size_t i = 0; i < vertex.data().bitmask1[0].size(); ++i)
      if (vertex.data().bitmask1[0][i])
        count++;
    return count;
}

//count the number of vertices reached in the current hop with Flajolet & Martin counting method
size_t approximate_pair_number(std::vector<std::vector<bool> > bitmask) {
  float sum = 0.0;
  for (size_t a = 0; a < bitmask.size(); ++a) {
    for (size_t i = 0; i < bitmask[a].size(); ++i) {
      if (bitmask[a][i] == 0) {
        sum += (float) i;
        break;
      }
    }
  }
  return (size_t) (pow(2.0, sum / (float) (bitmask.size())) / 0.77351);
}
//count the number of notes reached in the current hop
size_t absolute_vertex_data_with_hash(
    const graph_type::vertex_type& vertex) {
    size_t count = approximate_pair_number(vertex.data().bitmask1);
    return count;
}

int main(int argc, char** argv) {
  std::cout << "Approximate graph diameter\n\n";
  graphlab::distributed_control dc;

  std::string datafile;
  float termination_criteria = 0.0001;
  //parse command line
  graphlab::command_line_options clopts(
                "Approximate graph diameter. "
                "Directions of edges are considered.");
  std::string graph_dir;
  std::string format = "adj";
  bool use_sketch = true;
  std::string exec_type = "synchronous";
  clopts.attach_option("graph", graph_dir,
                       "The graph file. This is not optional");
  clopts.add_positional("graph");
  clopts.attach_option("format", format,
                       "The graph file format");
  clopts.attach_option("tol", termination_criteria,
                       "The permissible change at convergence.");
  clopts.attach_option("use-sketch", use_sketch,
                       "If true, will use Flajolet & Martin bitmask, "
                       "which is more compact and faster.");

  size_t powerlaw = 0;
  clopts.attach_option("powerlaw", powerlaw,
                       "Generate a synthetic powerlaw out-degree graph. ");
  double alpha = 2.1;
  clopts.attach_option("alpha", alpha,
                       "Generate a synthetic powerlaw graph with alpha parameter. ");

  size_t indegree = 0;
  clopts.attach_option("indegree", indegree,
                       "Generate a synthetic powerlaw in-degree graph if set to 1. ");
  bool is_indegree = indegree > 0;

  if (!clopts.parse(argc, argv)){
    dc.cout() << "Error in parsing command line arguments." << std::endl;
    return EXIT_FAILURE;
  }
  // if (graph_dir == "") {
  //   std::cout << "--graph is not optional\n";
  //   return EXIT_FAILURE;
  // }

  graphlab::timer timer;

  //load graph
  graph_type graph(dc, clopts);
  if(powerlaw > 0) { // make a synthetic graph
      dc.cout() << "Loading synthetic Powerlaw graph." << std::endl;
      graph.load_synthetic_powerlaw(powerlaw, is_indegree, alpha, 100000000);
  }
  else if (graph_dir.length() > 0) { // Load the graph from a file
      dc.cout() << "Loading graph in format: "<< format << std::endl;
      graph.load_format(graph_dir, format);
  }

  double load_time = timer.current_time();
  timer.start();

  graph.finalize();

  double finalize_time = timer.current_time();
  double ingress_time = load_time + finalize_time;

  if (use_sketch == false)
    graph.transform_vertices(initialize_vertex);
  else
    graph.transform_vertices(initialize_vertex_with_hash);

  graphlab::omni_engine<one_hop> engine(dc, graph, exec_type, clopts);

  timer.start();

  //main iteration
  size_t previous_count = 0;
  size_t diameter = 0;
  for (size_t iter = 0; iter < 100; ++iter) {
    engine.signal_all();
    engine.start();

    graph.transform_vertices(copy_bitmasks);

    size_t current_count = 0;
    if (use_sketch == false)
      current_count = graph.map_reduce_vertices<size_t>(absolute_vertex_data);
    else
      current_count = graph.map_reduce_vertices<size_t>(
          absolute_vertex_data_with_hash);
    dc.cout() << iter + 1 << "-th hop: " << current_count
        << " vertex pairs are reached\n";
    if (iter > 0
        && (float) current_count
            < (float) previous_count * (1.0 + termination_criteria)) {
      diameter = iter;
      dc.cout() << "converge\n";
      break;
    }
    previous_count = current_count;
  }
  const double runtime = timer.current_time();

  dc.cout() << "The approximate diameter is " << diameter << "\n";

  // dc.cout() << "graph calculation time is " << (end - start) << " sec\n";  

  // const std::string output_filename = "/home/yosub_shin_0/output.csv";
  // std::ofstream ofs;
  // ofs.open(output_filename.c_str(), std::ios::out | std::ios::app);
  // if (!ofs.is_open()) {
  //   std::cout << "Failed to open output file.\n";
  //   return EXIT_FAILURE;
  // }
  // std::string ingress_method = "";
  // clopts.get_graph_args().get_option("ingress", ingress_method);

  // // algorithm, partitioning_strategy, num_iterations, loading_time, partitioning_time, computation_time, total_time
  // ofs << "approximate_diameter," << ingress_method << "," << tolerance << "," << load_elapsed_secs << ",0," << runtime << "," << (load_elapsed_secs + runtime) << std::endl;

  // ofs.close();

  if (dc.procid() == 0) {
    char* username = getenv("USER");
    std::string username_str = username;
    const std::string output_filename = "/projects/sciteam/jsb/" + username + "/output.csv";
    bool file_exists = boost::filesystem::exists(output_filename);

    std::ofstream ofs;
    ofs.open(output_filename.c_str(), std::ios::out | std::ios::app);
    if (!ofs.is_open()) {
      std::cout << "Failed to open output file.\n";
      return EXIT_FAILURE;
    }
    std::string ingress_method = "";
    clopts.get_graph_args().get_option("ingress", ingress_method);

    const double replication_factor = (double)graph.num_replicas()/graph.num_vertices();
    bool topology_aware = dc.topology_aware();
    double num_master2mirror_hops = (double) graph.num_master2mirror_hops() / graph.num_replicas();
    double average_local_own_nverts = graph.average_num_local_own_vertices();
    double variance_local_own_nverts = graph.variance_num_local_own_vertices();
    double average_local_edges = graph.average_num_local_edges();
    double variance_local_edges = graph.variance_num_local_edges();

    if (!file_exists) {
      ofs << "algorithm,partitioning_strategy,num_iterations,replication_factor,load_time,finalize_time,ingress_time,computation_time,total_time,topology_aware,master2mirror_hops,average_local_own_nverts,variance_local_own_nverts,average_local_edges,variance_local_edges" << std::endl;
    }
    ofs << "approximate_diameter," << ingress_method << "," << termination_criteria << "," << replication_factor << "," << load_time << "," << finalize_time << "," << ingress_time << "," << runtime << "," << (ingress_time + runtime) << "," << topology_aware << "," << num_master2mirror_hops << "," << average_local_own_nverts<< "," << variance_local_own_nverts << "," << average_local_edges << "," << variance_local_edges << std::endl;

    ofs.close();
  }  

  return EXIT_SUCCESS;
}

