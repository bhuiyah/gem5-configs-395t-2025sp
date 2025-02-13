#include "arc.h"
#include <cassert>
#include <iostream>
#include <cstdint>
#include <algorithm>

// Constructor that uses the cache object to determine the number of sets and ways.
arc::arc(CACHE* cache) : arc(cache, cache->NUM_SET, cache->NUM_WAY) {}

// Constructor with explicit sets and ways.
arc::arc(CACHE* cache, long sets, long ways)
    : replacement(cache), NUM_SET(sets), NUM_WAY(ways), arc_sets(sets)
{
  // Initialize each ARC set.
  for (long set = 0; set < NUM_SET; set++) {
    arc_sets[set].T1.clear();
    arc_sets[set].T2.clear();
    arc_sets[set].B1.clear();
    arc_sets[set].B2.clear();
    arc_sets[set].p = 0;  // Start with a default value; you may later tune this.
  }
}

// Called during cache initialization.
void arc::initialize_replacement() {
  // In this example, initialization is done in the constructor.
  // You could also perform additional initialization here if needed.
}

// Called when a cache miss occurs to select a victim block in a given set.
long arc::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                      const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
  ARC_Set &aset = arc_sets[set];
  uint64_t victim_tag = 0;
  // Decision: if T1's size is larger than p, choose the LRU block from T1.
  if (aset.T1.size() > aset.p && !aset.T1.empty()) {
    victim_tag = aset.T1.back();
  } else if (!aset.T2.empty()) {
    victim_tag = aset.T2.back();
  } else {
    // If both resident lists are empty (should not normally occur), return an invalid index.
    return NUM_WAY;
  }

  // Map victim_tag back to the cache's way index.
  for (long way = 0; way < NUM_WAY; way++) {
    if (current_set[way].tag == victim_tag)
      return way;
  }
  // Fallback: if no matching tag found, indicate bypass.
  return NUM_WAY;
}

// Called when a block is filled in the cache (either in an empty slot or after eviction).
void arc::replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                 champsim::address full_addr, champsim::address ip,
                                 champsim::address victim_addr, access_type type)
{
  ARC_Set &aset = arc_sets[set];
  // If the new block was previously seen (a ghost hit), adjust p accordingly.
  if (in_list(aset.B1, full_addr)) {
    // Increase p; here we use a simple adjustment.
    aset.p = std::min((uint32_t)NUM_WAY, aset.p + 1);
    remove_from_list(aset.B1, full_addr);
    aset.T2.push_front(full_addr);  // Promote to T2.
  } else if (in_list(aset.B2, full_addr)) {
    // Decrease p. 
    aset.p = (aset.p > 0) ? aset.p - 1 : 0;
    remove_from_list(aset.B2, full_addr);
    aset.T2.push_front(full_addr);  // Also go to T2.
  } else {
    // Standard case: insert the new block into T1.
    aset.T1.push_front(full_addr);
  }
}

// Called on every cache access to update ARC state.
void arc::update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                   champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  ARC_Set &aset = arc_sets[set];
  if (hit) {
    // If hit in T1, promote the block to T2.
    if (in_list(aset.T1, full_addr)) {
      remove_from_list(aset.T1, full_addr);
      aset.T2.push_front(full_addr);
    }
    // If hit in T2, refresh its position (move to MRU).
    else if (in_list(aset.T2, full_addr)) {
      remove_from_list(aset.T2, full_addr);
      aset.T2.push_front(full_addr);
    }
    // Else, if it's a hit in the ghost lists, this would have been handled in replacement_cache_fill.
  }
  // Optionally, you can update additional ARC-specific statistics on misses here.
}

// Called at the end of simulation to output ARC-specific statistics.
void arc::replacement_final_stats() {
  std::cout << "ARC Replacement Final Stats:\n";
  for (long set = 0; set < NUM_SET; set++) {
    ARC_Set &aset = arc_sets[set];
    std::cout << "Set " << set << " p = " << aset.p << "\n";
  }
}

// --- Helper functions ---

// Returns true if addr is found in the provided list.
bool arc::in_list(const std::deque<uint64_t>& list, champsim::address addr) {
  for (auto tag : list) {
    if (tag == addr)
      return true;
  }
  return false;
}

// Remove addr from the provided list.
void arc::remove_from_list(std::deque<uint64_t>& list, champsim::address addr) {
  auto it = std::find(list.begin(), list.end(), addr);
  if (it != list.end())
    list.erase(it);
}