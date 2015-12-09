/**  
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
#ifndef GRAPHLAB_DISTRIBUTED_INGRESS_EDGE_DECISION_HPP
#define GRAPHLAB_DISTRIBUTED_INGRESS_EDGE_DECISION_HPP

#include <graphlab/graph/distributed_graph.hpp>
#include <graphlab/graph/graph_basic_types.hpp>
#include <graphlab/graph/graph_hash.hpp>
#include <graphlab/rpc/distributed_event_log.hpp>
#include <graphlab/util/dense_bitset.hpp>
#include <boost/random/uniform_int_distribution.hpp>

namespace graphlab {
  template<typename VertexData, typename EdgeData>
  class distributed_graph;

  static boost::unordered_map<uint32_t, size_t> coords_pair2dist;

  static size_t coords_pair_dist(const uint16_t& src, const uint16_t& dst) {
    uint32_t key = (src << 16) | dst;
    if (coords_pair2dist.find(key) == coords_pair2dist.end()) {
      size_t dist = 0;
      for (size_t j = 0; j < 2; ++j) {
	size_t src_val = (src >> (10 - 5 * j)) & 0x1F;
	size_t dst_val = (dst >> (10 - 5 * j)) & 0x1F;
	size_t abs = std::abs(src_val - dst_val);
	dist += std::min(abs, 23 - abs);
      }
      coords_pair2dist[key] = dist;
      return dist;
    } else {
      return coords_pair2dist[key];
    }
  }
 
 template<typename VertexData, typename EdgeData>
 class ingress_edge_decision {

    public:
      typedef graphlab::vertex_id_type vertex_id_type;
      typedef distributed_graph<VertexData, EdgeData> graph_type;
      typedef fixed_dense_bitset<RPC_MAX_N_PROCS> bin_counts_type; 
 
 private:
     distributed_control& dc_;
     boost::unordered_map<uint64_t, double> coords2score;
 
    public:
      /** \brief A decision object for computing the edge assingment. */
     ingress_edge_decision(distributed_control& dc) : dc_(dc) {
         double epsilon = 1.0;
         for (size_t i = 0; i < dc.numprocs(); ++i) {
             const uint64_t src_coord = dc.topologies()[i];
             for (size_t j = 0; j < dc.numprocs(); ++j) {
                 const uint64_t dst_coord = dc.topologies()[j];
                 const size_t src_dst_dist = coords_pair_dist(src_coord, dst_coord);
                 for (size_t k = 0; k < dc.numprocs(); ++k) {
                     const uint64_t candidate_coord = dc.topologies()[k];
                     const uint64_t key = (src_coord << 32) | (dst_coord << 16) | candidate_coord;
                     const size_t src_can_dist = coords_pair_dist(src_coord, candidate_coord);
                     const size_t dst_can_dist = coords_pair_dist(src_coord, candidate_coord);
                     coords2score[key] = ((2.0 * src_dst_dist - (src_can_dist + dst_can_dist)) / (epsilon + src_dst_dist)
                                          + (src_dst_dist - std::abs(src_can_dist - dst_can_dist)) / (epsilon + src_dst_dist)) / 30.0;
                 }
	 }
       }
     }

      /** Random assign (source, target) to a machine p in {0, ... numprocs-1} */
      procid_t edge_to_proc_random (const vertex_id_type source, 
          const vertex_id_type target,
          size_t numprocs) {
        typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
        const edge_pair_type edge_pair(std::min(source, target), 
            std::max(source, target));
        return graph_hash::hash_edge(edge_pair) % (numprocs);
      };

      /** Random assign (source, target) to a machine p in a list of candidates */
      procid_t edge_to_proc_random (const vertex_id_type source, 
          const vertex_id_type target,
          const std::vector<procid_t> & candidates) {
        typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
        const edge_pair_type edge_pair(std::min(source, target), 
            std::max(source, target));

        return candidates[graph_hash::hash_edge(edge_pair) % (candidates.size())];
      };


      /** Greedy assign (source, target) to a machine using: 
       *  bitset<MAX_MACHINE> src_degree : the degree presence of source over machines
       *  bitset<MAX_MACHINE> dst_degree : the degree presence of target over machines
       *  vector<size_t>      proc_num_edges : the edge counts over machines
       * */
      procid_t edge_to_proc_greedy (const vertex_id_type source, 
          const vertex_id_type target,
          bin_counts_type& src_degree,
          bin_counts_type& dst_degree,
          std::vector<size_t>& proc_num_edges,
          bool usehash = false,
          bool userecent = false) {
        size_t numprocs = proc_num_edges.size();

        // Compute the score of each proc.
        procid_t best_proc = -1; 
        double maxscore = 0.0;
        double epsilon = 1.0; 
        std::vector<double> proc_score(numprocs); 
        size_t minedges = *std::min_element(proc_num_edges.begin(), proc_num_edges.end());
        size_t maxedges = *std::max_element(proc_num_edges.begin(), proc_num_edges.end());

        for (size_t i = 0; i < numprocs; ++i) {
          size_t sd = src_degree.get(i) + (usehash && (source % numprocs == i));
          size_t td = dst_degree.get(i) + (usehash && (target % numprocs == i));
          double bal = (maxedges - proc_num_edges[i])/(epsilon + maxedges - minedges);
          proc_score[i] = bal + ((sd > 0) + (td > 0));
        }
        maxscore = *std::max_element(proc_score.begin(), proc_score.end());

        std::vector<procid_t> top_procs; 
        for (size_t i = 0; i < numprocs; ++i)
          if (std::fabs(proc_score[i] - maxscore) < 1e-5)
            top_procs.push_back(i);

        // Hash the edge to one of the best procs.
        typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
        const edge_pair_type edge_pair(std::min(source, target), 
            std::max(source, target));
        best_proc = top_procs[graph_hash::hash_edge(edge_pair) % top_procs.size()];

        ASSERT_LT(best_proc, numprocs);
        if (userecent) {
          src_degree.clear();
          dst_degree.clear();
        }
        src_degree.set_bit(best_proc);
        dst_degree.set_bit(best_proc);
        ++proc_num_edges[best_proc];
        return best_proc;
      };

     /** Greedy assign (source, target) to a machine using:
      *  bitset<MAX_MACHINE> src_degree : the degree presence of source over machines
      *  bitset<MAX_MACHINE> dst_degree : the degree presence of target over machines
      *  vector<size_t>      proc_num_edges : the edge counts over machines
      * */
     procid_t edge_to_proc_greedy_and_topology (const vertex_id_type source,
                                   const vertex_id_type target,
                                   bin_counts_type& src_degree,
                                   bin_counts_type& dst_degree,
                                   std::vector<size_t>& proc_num_edges,
                                   bool usehash = false,
                                   bool userecent = false) {
         size_t numprocs = proc_num_edges.size();

         // Compute the score of each proc.
         procid_t best_proc = -1;
         double maxscore = 0.0;
         double epsilon = 1.0;
         std::vector<double> proc_score(numprocs);
         size_t minedges = *std::min_element(proc_num_edges.begin(), proc_num_edges.end());
         size_t maxedges = *std::max_element(proc_num_edges.begin(), proc_num_edges.end());

         const uint64_t src_dst_coords_shftd = (dc_.topologies()[graph_hash::hash_vertex(source) % numprocs] << 32) |
             (dc_.topologies()[graph_hash::hash_vertex(target) % numprocs] << 16);

         for (size_t i = 0; i < numprocs; ++i) {
	   size_t sd = src_degree.get(i);
	   size_t td = dst_degree.get(i);
             double bal = (maxedges - proc_num_edges[i])/(epsilon + maxedges - minedges);
             proc_score[i] = bal + ((sd > 0) + (td > 0)) // original terms (load balance + greedy)
                 + coords2score[src_dsdt_coords_shftd | dc.topologies()[i]];
	       
         }
         maxscore = *std::max_element(proc_score.begin(), proc_score.end());

         std::vector<procid_t> top_procs;
         for (size_t i = 0; i < numprocs; ++i)
             if (std::fabs(proc_score[i] - maxscore) < 1e-5)
                 top_procs.push_back(i);

         // Hash the edge to one of the best procs.
         typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
         const edge_pair_type edge_pair(std::min(source, target),
                                        std::max(source, target));
         best_proc = top_procs[graph_hash::hash_edge(edge_pair) % top_procs.size()];

         ASSERT_LT(best_proc, numprocs);
         if (userecent) {
             src_degree.clear();
             dst_degree.clear();
         }
         src_degree.set_bit(best_proc);
         dst_degree.set_bit(best_proc);
         ++proc_num_edges[best_proc];
         return best_proc;
     };

      /** Greedy assign (source, target) to a machine using: 
       *  bitset<MAX_MACHINE> src_degree : the degree presence of source over machines
       *  bitset<MAX_MACHINE> dst_degree : the degree presence of target over machines
       *  vector<size_t>      proc_num_edges : the edge counts over machines
       * */
      procid_t edge_to_proc_greedy (const vertex_id_type source, 
          const vertex_id_type target,
          bin_counts_type& src_degree,
          bin_counts_type& dst_degree,
          std::vector<procid_t>& candidates,
          std::vector<size_t>& proc_num_edges,
          bool usehash = false,
          bool userecent = false
          ) {
        size_t numprocs = proc_num_edges.size();

        // Compute the score of each proc.
        procid_t best_proc = -1; 
        double maxscore = 0.0;
        double epsilon = 1.0; 
        std::vector<double> proc_score(candidates.size()); 
        size_t minedges = *std::min_element(proc_num_edges.begin(), proc_num_edges.end());
        size_t maxedges = *std::max_element(proc_num_edges.begin(), proc_num_edges.end());

        for (size_t j = 0; j < candidates.size(); ++j) {
          size_t i = candidates[j];
          size_t sd = src_degree.get(i) + (usehash && (source % numprocs == i));
          size_t td = dst_degree.get(i) + (usehash && (target % numprocs == i));
          double bal = (maxedges - proc_num_edges[i])/(epsilon + maxedges - minedges);
          proc_score[j] = bal + ((sd > 0) + (td > 0));
        }
        maxscore = *std::max_element(proc_score.begin(), proc_score.end());

        std::vector<procid_t> top_procs; 
        for (size_t j = 0; j < candidates.size(); ++j)
          if (std::fabs(proc_score[j] - maxscore) < 1e-5)
            top_procs.push_back(candidates[j]);

        // Hash the edge to one of the best procs.
        typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
        const edge_pair_type edge_pair(std::min(source, target), 
            std::max(source, target));
        best_proc = top_procs[graph_hash::hash_edge(edge_pair) % top_procs.size()];

        ASSERT_LT(best_proc, numprocs);
        if (userecent) {
          src_degree.clear();
          dst_degree.clear();
        }
        src_degree.set_bit(best_proc);
        dst_degree.set_bit(best_proc);
        ++proc_num_edges[best_proc];
        return best_proc;
      };
      
     /** HDRF greedy assign (source, target) to a machine using: 
      *  bitset<MAX_MACHINE> src_degree : the degree presence of source over machines
      *  bitset<MAX_MACHINE> dst_degree : the degree presence of target over machines
      *  size_t              src_true_degree : the degree of source vertex over machines
      *  size_t              dst_true_degree : the degree of target vertex over machines
      *  vector<size_t>      proc_num_edges : the edge counts over machines
      *
      *  author : Fabio Petroni [www.fabiopetroni.com]
	  *           Giorgio Iacoboni [g.iacoboni@gmail.com]
      *
      *  Based on the publication:	
      *  F. Petroni, L. Querzoni, K. Daudjee, S. Kamali and G. Iacoboni: 
      *  "HDRF: Stream-Based Partitioning for Power-Law Graphs". 
      *  CIKM, 2015.
      * */
     procid_t edge_to_proc_hdrf (const vertex_id_type source, 
          const vertex_id_type target,
          bin_counts_type& src_degree,
          bin_counts_type& dst_degree,
          size_t& src_true_degree,
          size_t& dst_true_degree,
          std::vector<size_t>& proc_num_edges,
          bool usehash = false,
          bool userecent = false) {
        
        size_t numprocs = proc_num_edges.size();
        
        size_t degree_u = src_true_degree;
        degree_u = degree_u +1;
        size_t degree_v = dst_true_degree;
        degree_v = degree_v +1;
        size_t SUM = degree_u + degree_v;
        double fu = degree_u;
        fu /= SUM;
        double fv = degree_v;
        fv /= SUM;
        
        // Compute the score of each proc.
        procid_t best_proc = -1; 
        double maxscore = 0.0;
        double epsilon = 1.0; 
        std::vector<double> proc_score(numprocs); 
        size_t minedges = *std::min_element(proc_num_edges.begin(), proc_num_edges.end());
        size_t maxedges = *std::max_element(proc_num_edges.begin(), proc_num_edges.end());
        
        for (size_t i = 0; i < numprocs; ++i) {
		  double new_sd = 0;
		  double new_td = 0;
		  size_t sd = src_degree.get(i) + (usehash && (source % numprocs == i));
		  size_t td = dst_degree.get(i) + (usehash && (target % numprocs == i));
		  if (sd > 0){
		    new_sd = 1+(1-fu);
		  }
		  if (td > 0){
		    new_td = 1+(1-fv);
		  }
         double bal = (maxedges - proc_num_edges[i])/(epsilon + maxedges - minedges);

         proc_score[i] = bal + new_sd + new_td;
        }
        
        maxscore = *std::max_element(proc_score.begin(), proc_score.end());
        
        std::vector<procid_t> top_procs; 
        for (size_t i = 0; i < numprocs; ++i)
          if (std::fabs(proc_score[i] - maxscore) < 1e-5)
            top_procs.push_back(i);
        
        // Hash the edge to one of the best procs.
        typedef std::pair<vertex_id_type, vertex_id_type> edge_pair_type;
        const edge_pair_type edge_pair(std::min(source, target), std::max(source, target));
        best_proc = top_procs[graph_hash::hash_edge(edge_pair) % top_procs.size()];
        
        ASSERT_LT(best_proc, numprocs);
        if (userecent) {
          src_degree.clear();
          dst_degree.clear();
        }
        src_degree.set_bit(best_proc);
        dst_degree.set_bit(best_proc);
        ++proc_num_edges[best_proc];
        ++src_true_degree;
        ++dst_true_degree;
        return best_proc;
     };
  };// end of ingress_edge_decision
}

#endif
