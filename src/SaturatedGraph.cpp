/* Copyright (C) 2018 Magnus Lång and Tuan Phong Ngo
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

#include "SaturatedGraph.h"

#include "Timing.h"
#include <immer/algorithm.hpp>
#include <immer/vector_transient.hpp>
#include <iostream>

#ifdef TRACE
#  define IFTRACE(X) X
#else
#  define IFTRACE(X) ((void)0)
#endif

static Timing::Context saturate_timing("saturate");
static Timing::Context saturate1_timing("saturate_one");
static Timing::Context add_edge_timing("add_edge");
static Timing::Context add_event_timing("add_event");

SaturatedGraph::SaturatedGraph() {}

void SaturatedGraph::add_event(Pid pid, ExtID extid, EventKind kind,
                               SymAddr addr, Option<ExtID> ext_read_from,
                               const std::vector<ExtID> &orig_in) {
  Timing::Guard timing_guard(add_event_timing);
  ID id = events.size();
  extid_to_id = std::move(extid_to_id).set(extid, id);
  const auto add_out = [id](immer::vector<ID> o) {
      return std::move(o).push_back(id);
    };
  bool is_load = kind == LOAD || kind == RMW;
  bool is_store = kind == STORE || kind == RMW;
  Option<ID> po_predecessor;
  int index = 1;
  if (events_by_pid.count(pid)) {
    assert(events_by_pid.at(pid).size() != 0);
    po_predecessor = events_by_pid.at(pid).back();
    IFTRACE(std::cerr << "Adding PO between " << *po_predecessor << " and " << id << "\n");
    index = events.at(*po_predecessor).iid.get_index() + 1;
    outs = std::move(outs).update(*po_predecessor, add_out);
  }
  events_by_pid = std::move(events_by_pid).update(pid, [id](auto v) {
      return std::move(v).push_back(id);
    });

  auto extid_to_id = [this](ExtID i) { return this->extid_to_id.at(i); };
  Option<ID> read_from = ext_read_from.map(extid_to_id);

  immer::vector_transient<ID> out;
  immer::vector_transient<ID> in;
  for (const ExtID &ei : orig_in) {
    ID i = extid_to_id(ei);
    IFTRACE(std::cerr << "Adding edge between " << i << " and " << id << "\n");
    in.push_back(i);
  }
  if (is_store) {
    /* TODO: Optimise; only add the first write of each process */
    /* We do stores first so that in the case of RMW we do not find
     * ourselves in reads_from_init.
     */
    for (ID r : reads_from_init[addr]) {
      IFTRACE(std::cerr << "Adding from-read between " << r << " and " << id << "\n");
      in.push_back(r);
      outs = std::move(outs).update(r, add_out);
    }
  }
  if (read_from) {
    IFTRACE(std::cerr << "Adding read-from between " << *read_from << " and " << id << "\n");
    events = std::move(events).update(*read_from, [id](Event w) {
        w.readers = std::move(w.readers).push_back(id);
        return std::move(w);
      });
  } else if (is_load) {
    /* Read from init */
    reads_from_init = std::move(reads_from_init).update
      (addr, [id](immer::vector<ID> v) {
               return std::move(v).push_back(id);
             });
    for (ID w : writes_by_address[addr]) {
      /* TODO: Optimise; only add the first write of each process */
      IFTRACE(std::cerr << "Adding from-read between " << id << " and " << w << "\n");
      out.push_back(w);
      ins = std::move(ins).update(w, [id](immer::vector<ID> v) {
          return std::move(v).push_back(id);
        });
      wq_add(w);
    }
  }

  for (ID after : in) {
    IFTRACE(std::cerr << "Adding out edge for " << after << " and " << id << "\n");
    outs = std::move(outs).update(after, add_out);
  }

  IID<Pid> iid(pid, index);
  events = std::move(events).push_back
    (Event(iid, extid, is_load, is_store, addr, read_from, {}, po_predecessor));

  assert(ins.size() == id);
  ins = std::move(ins).push_back(std::move(in).persistent());
  assert(outs.size() == id);
  outs = std::move(outs).push_back(std::move(out).persistent());

  if (is_store) {
    writes_by_address = std::move(writes_by_address).update
      (addr, [id] (auto &&vec) { return std::move(vec).push_back(id); });
  }

  assert(vclocks.size() == id);
  vclocks = std::move(vclocks).push_back({});
  wq_add(id);
}

bool SaturatedGraph::saturate() {
  Timing::Guard saturate_guard(saturate_timing);
  check_graph_consistency();
  while (!wq_empty()) {
    Timing::Guard saturate1_guard(saturate1_timing);
    const ID id = wq_pop();
    assert(id < events.size());
    std::vector<ID> new_in;
    std::vector<std::pair<ID,ID>> new_edges;
    {
      Event e = events[id];
      const immer::vector<ID> &old_in = ins[id];
      VC vc = recompute_vc_for_event(e, old_in);
      const VC &old_vc = vclocks[id];
      if (is_in_cycle(e, old_in, vc)) {
        IFTRACE(std::cerr << "Cycle found\n");
        check_graph_consistency();
        return false;
      }
      if (vc == old_vc) {
        IFTRACE(std::cerr << id << " unchanged\n");
        continue;
      }
      /* Saturation logic */
      if (e.is_load || e.is_store) {
        for (unsigned pid = 0; pid < unsigned(vc.size_ub()); ++pid) {
          if (old_vc[pid] == vc[pid]) continue;
          ID pe_id = get_process_event(pid, vc[pid]);
          const Event *pe;
          for (int pi = vc[pid]; pi > old_vc[pid];
               --pi, pe_id = pe->po_predecessor.value_or(~0)) {
            assert(pe_id != ~0u && pe_id < events.size());
            pe = &events[pe_id];
            if (pe_id == id) continue;
            if (pe->is_store && pe->addr == e.addr) {
              /* Add edges */
              if (e.is_store) {
                for (unsigned r : pe->readers) {
                  if (r == id) continue; /* RMW we're reading from */
                  /* from-read */
                  assert(events[r].is_load && events[r].addr == e.addr);
                  IFTRACE(std::cerr << "Adding from-read from " << r << " to " << id << "\n");
                  new_in.push_back(r);
                  pe = &events[pe_id];
                }
              }
              if (e.is_load) {
                if (!e.read_from) {
                  /* pe must happen after us since we're loading from
                   * init. Cycle detected.
                   */
                  IFTRACE(std::cerr << "Cycle found\n");
                  check_graph_consistency();
                  return false;
                }
                unsigned my_read_from = *e.read_from;
                if (pe_id != my_read_from) {
                  /* coherence order */
                  IFTRACE(std::cerr << "Adding coherence order from " << pe_id << " to " << my_read_from << "\n");
                  new_edges.emplace_back(pe_id, my_read_from);
                }
              }
              break;
            }
          }
        }
      }

      add_successors_to_wq(id, e);
      IFTRACE(std::cerr << "Updating " << id << ": " << vc << "\n");
      vclocks = vclocks.set(id, std::move(vc));
    }
    ins = std::move(ins).update(id, [&new_in](immer::vector<ID> v) {
        auto tmp = std::move(v).transient();
        for (ID b : new_in) tmp.push_back(b);
        return tmp.persistent();
      });
    for (ID b : new_in) {
      outs = std::move(outs).update(b, [id](immer::vector<ID> v) {
          return std::move(v).push_back(id);
        });
    }
    if (!new_in.empty()) wq_add_first(id);
    add_edges(new_edges);
  }
  check_graph_consistency();
  return true;
}

bool SaturatedGraph::is_in_cycle
(const Event &e, const immer::vector<ID> &in, const VC &vc) const {
  const auto vc_different = [&vc,this](unsigned other) {
                              return vclocks[other].get() != vc;
                            };
  if (e.po_predecessor && !vc_different(*e.po_predecessor)) return true;
  if (e.read_from && !vc_different(*e.read_from)) return true;
  if (!immer::all_of(in, vc_different)) return true;
  return false;
}

void SaturatedGraph::add_edges(const std::vector<std::pair<ID,ID>> &edges) {
  if (edges.size() == 0) return;
  for (auto pair : edges) {
    ID from = pair.first;
    ID to = pair.second;
    add_edge_internal(from, to);
  }
}

SaturatedGraph::ID SaturatedGraph::
get_process_event(unsigned pid, unsigned index) const {
  assert(index <= events_by_pid.at(pid).size());
  return events_by_pid.at(pid)[index-1];
}

SaturatedGraph::VC SaturatedGraph::initial_vc_for_event(IID<unsigned> iid) const {
  VC vc;
  vc[iid.get_pid()] = iid.get_index();
  return vc;
}

SaturatedGraph::VC SaturatedGraph::initial_vc_for_event(const Event &e) const {
  return initial_vc_for_event(e.iid);
}

SaturatedGraph::VC SaturatedGraph::
recompute_vc_for_event(const Event &e, const immer::vector<ID> &in) const {
  VC vc = initial_vc_for_event(e);;
  const auto add_to_vc = [&vc,this](ID id) {
                           assert(id < vclocks.size());
                           vc += vclocks[id];
                         };
  if (e.po_predecessor)
    add_to_vc(*e.po_predecessor);
  if (e.read_from) add_to_vc(*e.read_from);
  immer::for_each(in, add_to_vc);
  return vc;
}

void SaturatedGraph::add_successors_to_wq(ID id, const Event &e) {
  const auto add_to_wq = [this](unsigned id) { wq_add(id); };
  if (e.is_store)
    immer::for_each(e.readers, add_to_wq);
  immer::for_each(outs[id], add_to_wq);
}

void SaturatedGraph::wq_add(unsigned id) {
  if (wq_set.count(id)) return;
  wq_queue = wq_queue.push_back(id);
  wq_set = wq_set.insert(id);
}

void SaturatedGraph::wq_add_first(unsigned id) {
  if (wq_set.count(id)) return;
  wq_queue = wq_queue.push_front(id);
  wq_set = wq_set.insert(id);
}

bool SaturatedGraph::wq_empty() const {
  return wq_queue.empty();
}

unsigned SaturatedGraph::wq_pop() {
  assert(!wq_empty());
  unsigned ret = wq_queue[0];
  wq_queue = wq_queue.drop(1);
  wq_set = wq_set.erase(ret);
  return ret;
}

#ifndef NDEBUG
void SaturatedGraph::check_graph_consistency() const {
  for (ID id = 0; id < events.size(); ++id) {
    const auto is_not_id = [id](unsigned v) { return v != id; };
    const Event &e = events.at(id);
    /* All incoming types */
    for (ID in : ins[id]) {
      assert(!immer::all_of(outs[in], is_not_id));
    }
    if (e.read_from) {
      const ID w = *e.read_from;
      assert(events.at(w).is_store && events.at(w).addr == e.addr);
      assert(!immer::all_of(events.at(w).readers, is_not_id));
      // assert(immer::all_of(e.in, [w](unsigned v) { return v != w; }));
    }
    if (e.po_predecessor) {
      assert(!immer::all_of(outs[*e.po_predecessor], is_not_id));
      // assert(immer::all_of(e.in, [&e](unsigned v) { return v != *e.po_predecessor; }));
    }
    /* All outgoing types */
    for (ID out : outs[id]) {
      const Event &oute = events.at(out);
      assert(!immer::all_of(ins[out], is_not_id)
             || (oute.po_predecessor && *oute.po_predecessor == id));
    }
    if (e.is_store) {
      for (unsigned r : e.readers) {
        const Event &reade = events.at(r);
        assert(reade.is_load);
        assert(reade.read_from);
        assert(*reade.read_from == id);
        // !immer::all_of(reade.read_froms, is_not_id)
        // assert(immer::all_of(e.out, [r](unsigned v) { return v != r; })
        //        || (reade.po_predecessor && *reade.po_predecessor == id));
      }
    }
  }
}
#endif

void SaturatedGraph::print_graph
(std::ostream &o, std::function<std::string(unsigned)> event_str) const {
  o << "digraph {\n";
  for (ID id = 0; id < events.size(); ++id) {
    const Event &e = events[id];
    o << id << " [label=\"" << event_str(id) << "\"];\n";
    if (e.read_from) {
      o << *e.read_from << " -> " << id << " [label=\"rf\"];\n";
    }
    if (e.po_predecessor) {
      o << *e.po_predecessor << " -> " << id << " [label=\"po\"];\n";
    }
    for (ID in : ins[id]) {
      o << in << " -> " << id << " [label=\"in\"];\n";
    }
  }

  o << "}\n";
}

std::vector<SaturatedGraph::ExtID> SaturatedGraph::event_ids() const {
  std::vector<ExtID> ret;
  immer::for_each(events, [&ret](const Event &e) { ret.push_back(e.ext_id); });
  return ret;
}

bool SaturatedGraph::event_is_store(ExtID eid) const {
  return events[extid_to_id.at(eid)].is_store;
}

SymAddr SaturatedGraph::event_addr(ExtID eid) const {
  ID id = extid_to_id.at(eid);
  assert(events[id].is_load || events[id].is_store);
  return events[id].addr;
}

const SaturatedGraph::VC &SaturatedGraph::event_vc(ExtID eid) const {
  assert(is_saturated());
  return vclocks[extid_to_id.at(eid)];
}

std::vector<SaturatedGraph::ExtID> SaturatedGraph::event_in(ExtID eid) const {
  std::vector<unsigned> ret;
  ID id = extid_to_id.at(eid);
  const Event &e = events[id];
  const immer::vector<ID> &in = ins[id];
  ret.reserve(in.size() + 2);
  immer::for_each
    (in, [&ret,this](ID e) { ret.push_back(events[e].ext_id); });
  if (e.po_predecessor) ret.push_back(events[*e.po_predecessor].ext_id);
  if (e.read_from)
    ret.push_back(events[*e.read_from].ext_id);
  return ret;
}

void SaturatedGraph::add_edge(ExtID from, ExtID to) {
  add_edge_internal(extid_to_id.at(from), extid_to_id.at(to));
}

void SaturatedGraph::add_edge_internal(ID from, ID to) {
  Timing::Guard timing_guard(add_edge_timing);
  outs = std::move(outs).update(from, [to](immer::vector<ID> v) {
                                        return std::move(v).push_back(to);
                                      });
  ins = std::move(ins).update(to, [from](immer::vector<ID> v) {
                                    return std::move(v).push_back(from);
                                  });
  wq_add(to);
}
