#include <cassert>
#include <functional>
#include <set>
#include <stack>

#include "coder_file.h"
#include "dgraph.h"

namespace coder_compiler
{
  dgraph
  extract_subgraph(const dgraph& dependency_graph, const coder_file_ptr& node, dgraph result)
  {
    if(node && !result.count(node) && dependency_graph.count(node))
      {
        const auto& sub = dependency_graph.at(node);

        result[node] = sub;

        for(const auto& s: sub)
          {
            result = extract_subgraph(dependency_graph, s, std::move(result));
          }
      }

    return result;
  }

 	dgraph
	conn_comp(const dgraph& node_list, size_t min_component_size)
  {
		//https://en.wikipedia.org/wiki/Tarjan's_strongly_connected_components_algorithm
		struct Node
    {
			Node():index(-1), lowlink(-1) , onStack(false){}

      Node(int index, int lowlink, bool onStack):index(index), lowlink(lowlink) , onStack(onStack){}

      int index;

      int lowlink;

      bool onStack;
		};

    std::unordered_map<coder_file_ptr,Node> nodes;

		dgraph groups;

    std::stack<coder_file_ptr> S;

    const auto& V = node_list;

    int next = 0;

		std::function<void(const coder_file_ptr&)>
    strongconnect = [&]( const coder_file_ptr& v)->void
    {
			nodes[v] = {next,next,true};

			next++;

      S.push(v);

      if (V.count(v))
        {
          for (auto& w: V.at(v))
            {
              if (nodes[w].index == -1)
                {
                  strongconnect(w);

                  nodes[v].lowlink = std::min(nodes[v].lowlink, nodes[w].lowlink);
                }
              else if (nodes[w].onStack)
                {
                  nodes[v].lowlink = std::min(nodes[v].lowlink, nodes[w].index);
                }
            }
        }

			if (nodes[v].lowlink == nodes[v].index)
        {
          std::unordered_set<coder_file_ptr> com;

          auto w = S.top();

          do
            {
              w = S.top();

              S.pop();

              nodes[w].onStack = false;

              com.insert(w);
            }
          while (w != v);

          if(com.size() >= min_component_size)
            {
              coder_file_ptr component_root(new coder_file());

              groups[component_root]=com;
            }
        }
		};

		for (auto& v : V)
      {
        if (nodes[v.first].index == -1)
          {
            strongconnect(v.first);
          }
      }

		return groups;
	}

  dgraph
  merge_nodes(dgraph G, const dgraph&  components)
  {
    dgraph new_nodes;

    for (auto& comp: components)
      {
        auto& new_node = new_nodes[comp.first];

        for (auto& s : comp.second)
          {
            auto& current_node = G.at(s);

            for (auto& t : comp.second)
              current_node.erase(t); // break cycles

            new_node.insert(current_node.begin(), current_node.end()); //add callee of a cyclic node to list of the callee of the component's header node

            G.erase(s);//remove all cyclic nodes from the graph
          }
      }

    for(auto& val : new_nodes)
      G.insert(std::move(val)); // each componenet  reduced to a  node

    for (auto& node: G)
      {
        for (auto& comp: components)
          {
            size_t num_erased = 0;

            for (auto& s : comp.second)
              {
                num_erased+=node.second.erase(s);// remove cyclic nodes from callee of non-cyclic nodes
              }

            if (num_erased > 0)
              node.second.insert(comp.first); // if a non-cyclic node calls a cyclic node add header of component to callee of the non-cyclic node
          }
      }

    return G;
  }

  std::vector<coder_file_ptr>
  top_sort(const dgraph& node_list, const coder_file_ptr& start_node)
  {
    //Tarjan https://en.wikipedia.org/wiki/Topological_sorting

    assert(node_list.size() > 0);

    std::vector<coder_file_ptr> L(node_list.size());

    std::unordered_map<coder_file_ptr,bool> visited;

    int i = 0;

    std::function<void(const coder_file_ptr&)> topsort = [&](const coder_file_ptr& n) ->void
    {
      if (n)
        {
          visited[n] = true;

          if (node_list.count(n))
            for (const auto& m : node_list.at(n))
              if (!visited[m])
                topsort(m);

          L.at(i++) = n;
        }
    };

    topsort(start_node);

    return L;
  }

  std::vector<coder_file_ptr>
  extract_all_sources (const dgraph& dependency_graph)
  {
    std::vector<coder_file_ptr> result;

    std::set<coder_file_ptr> keys;

    for (const auto& file: dependency_graph)
      {
        keys.insert (file.first);
      }

    for (const auto& file: dependency_graph)
      {
        const auto& dep = file.second;

        for (const auto& k : dep)
          keys.erase (k);
      }

    result.insert (result.begin (), keys.begin (), keys.end ());

    return result;
  }
}