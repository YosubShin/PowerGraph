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


#include <cstdio>
#include <cstdlib>
#include <string>
#include <graphlab/rpc/dc.hpp>
#include <graphlab/rpc/dc_init_from_env.hpp>
#include <graphlab/util/stl_util.hpp>
#include <graphlab/logger/logger.hpp>
namespace graphlab {

bool init_param_from_env(dc_init_param& param) {
  char* nodeid = getenv("SPAWNID");
  if (nodeid == NULL) {
    return false;
  }
  param.curmachineid = atoi(nodeid);

  char* nodes = getenv("SPAWNNODES");
  std::string nodesstr = nodes;
  if (nodes == NULL) {
    return false;
  }

  param.machines = strsplit(nodesstr, ",");
  for (size_t i = 0;i < param.machines.size(); ++i) {
    param.machines[i] = param.machines[i] + ":" + tostr(10000 + i);
  }

  char* topologies_file = getenv("TOPOLOGIES_FILE");
  if (topologies_file != NULL) {
    std::string topologies_file_string = topologies_file;
    std::ifstream ifs;
    ifs.open(topologies_file_string.c_str());
    if (!ifs.is_open()) {
      std::cout << "Failed to open topologies file.\n";
      return false;
    }

    param.topologies.resize(param.machines.size());
    for (size_t i = 0; i < param.machines.size(); ++i) {
      std::string line;
      std::getline(ifs, line);
      std::vector<const std::string> str_coord =  strsplit(line, " ");
      std::vector<int> coord(3);
      for (size_t j = 0; j < 3; ++j) {
        coord[j] = fromstr(str_coord[j]);
      }
      param.topologies.push_back(coord);
    }

    ifs.close();
  }

  // set defaults
  param.numhandlerthreads = RPC_DEFAULT_NUMHANDLERTHREADS;
  param.commtype = RPC_DEFAULT_COMMTYPE;
  return true;
}

} // namespace graphlab

