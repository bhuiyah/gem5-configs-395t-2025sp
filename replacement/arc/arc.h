#ifndef REPLACEMENT_ARC_H
#define REPLACEMENT_ARC_H

#include <deque>
#include <vector>
#include <algorithm>
#include "cache.h"
#include "modules.h"

// Example ARC replacement module that inherits from the ChampSim replacement base class.
class arc : public champsim::modules::replacement {
 public:
  // Constructor: note that we inherit from the base class constructor by passing a CACHE* pointer.
  explicit arc(CACHE* cache);
  arc(CACHE* cache, long sets, long ways);

  // Replacement policy functions required by ChampSim:
  void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                           const champsim::cache_block* current_set, champsim::address ip,
                           champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                      champsim::address full_addr, champsim::address ip,
                                      champsim::address victim_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                        champsim::address full_addr, champsim::address ip,
                                        champsim::address victim_addr, access_type type, uint8_t hit);
  void replacement_final_stats();

 private:
  // Total ways and sets from the cache configuration.
  long NUM_SET;
  long NUM_WAY;

  // Per-set ARC state structure.
  struct ARC_Set {
    std::deque<uint64_t> T1; // Resident list: blocks seen once.
    std::deque<uint64_t> T2; // Resident list: blocks seen at least twice.
    std::deque<uint64_t> B1; // Ghost list for evicted T1 blocks.
    std::deque<uint64_t> B2; // Ghost list for evicted T2 blocks.
    uint32_t p;            // Adaptation parameter that controls the target size for T1.
  };

  // A vector of ARC state objects, one for each cache set.
  std::vector<ARC_Set> arc_sets;
};

#endif