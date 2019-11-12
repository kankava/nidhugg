/* Copyright (C) 2018 Alexis Remmers and Nodari Kankava
 *
 * This file is part of Nidhugg.
 *
 * Nidhugg is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nidhugg is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#ifndef __RFSC_DECISION_TREE_H__
#define __RFSC_DECISION_TREE_H__

#define UNF_LEAF_PAIR std::__detail::_Node_iterator<std::pair<const std::shared_ptr<RFSCUnfoldingTree::UnfoldingNode>, Leaf>, false, false>

#include "SymEv.h"
#include "SaturatedGraph.h"
#include "RFSCUnfoldingTree.h"
// #include "RFSCTraceBuilder.h"

#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <queue>


struct Branch {
public:
  Branch(int pid, int size, int decision, bool pinned, SymEv sym)
    : pid(pid), size(size), decision(decision), pinned(pinned),
      sym(std::move(sym)) {}
  Branch() : Branch(-1, 0, -1, false, {}) {}
  int pid, size, decision;
  bool pinned;
  SymEv sym;
};

struct Leaf {
public:
  /* Construct a bottom-leaf. */
  Leaf() : prefix() {}
  /* Construct a prefix leaf. */
  Leaf(std::vector<Branch> prefix) : prefix(prefix) {}
  std::vector<Branch> prefix;

  bool is_bottom() const { return prefix.empty(); }
};

struct DecisionNode {
public:
  DecisionNode() : siblings() {}
  std::unordered_map<std::shared_ptr<RFSCUnfoldingTree::UnfoldingNode>, Leaf> siblings;
  std::unordered_set<std::shared_ptr<RFSCUnfoldingTree::UnfoldingNode>> sleep;
  SaturatedGraph graph_cache;
};

class RFSCDecisionTree final {
public:
  RFSCDecisionTree() {};

  /* Using the last decision that caused a failure, and then
   * prune all later decisions. */
  void prune_decisions(int blame);
  void clear_unrealizable_siblings();
  bool empty() {return decisions.empty();};
  size_t size() {return decisions.size();};
  DecisionNode &get(int decision) {return decisions[decision];}
  void place_decision_into_sleepset(const std::shared_ptr<RFSCUnfoldingTree::UnfoldingNode> &decision);

  UNF_LEAF_PAIR get_next_sibling();
  void erase_sibling(UNF_LEAF_PAIR sit);

  int new_decision_node();
  SaturatedGraph &get_saturated_graph(unsigned i);
  void add_node_to_wq(DecisionNode &node);
  std::shared_ptr<UNF_LEAF_PAIR> get_node_from_wq();
protected:
  std::mutex tree_mutex;
  std::queue<std::shared_ptr<DecisionNode>> work_queue;
  std::vector<DecisionNode> decisions;
};


#endif