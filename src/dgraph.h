#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace coder_compiler
{
  struct coder_file;
  
  using coder_file_ptr = std::shared_ptr<coder_file>;
  
  using dgraph = std::unordered_map<coder_file_ptr,std::unordered_set<coder_file_ptr>>;
    
  dgraph
  extract_subgraph(const dgraph& dependency_graph, const coder_file_ptr& node, dgraph result= dgraph());
 
  dgraph
  conn_comp(const dgraph& node_list, size_t min_component_size = 1) ;
 
  dgraph 
  merge_nodes(dgraph G, const dgraph&  components);

  std::vector<coder_file_ptr>
  top_sort(const dgraph& node_list, const coder_file_ptr& start_node);
  
  std::vector<coder_file_ptr>
  extract_all_sources (const dgraph& dependency_graph);
}