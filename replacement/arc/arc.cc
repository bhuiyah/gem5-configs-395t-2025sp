#include "arc.h"
#include <cassert>
#include <iostream>
#include <cstdint>
#include <algorithm>

// Constructor that uses the cache object to determine the number of sets and ways.
arc::arc(CACHE* cache) : arc(cache, cache->NUM_SET, cache->NUM_WAY) {}

// Constructor with explicit sets and ways.
arc::arc(CACHE* cache, long sets, long ways)
    : replacement(cache), NUM_SET(sets), NUM_WAY(ways), arc_sets(sets) { initialize_replacement(); }

// Called during cache initialization.
void arc::initialize_replacement() 
{
    // Initialize each ARC set.
    for (long set = 0; set < NUM_SET; set++) {
      arc_sets[set].T1.clear();
      arc_sets[set].T2.clear();
      arc_sets[set].B1.clear();
      arc_sets[set].B2.clear();
      arc_sets[set].p = 0;
  }
}

// Called when a cache miss occurs to select a victim block in a given set.
long arc::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                      const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
    ARC_Set &aset = arc_sets[set];
    uint64_t victim_tag = 0;

    // Decide which list to choose the victim from:
    // If the number of blocks in T1 exceeds the target (p),
    // select the LRU block from T1; otherwise, select from T2.
    if (!aset.T1.empty() && (aset.T1.size() > aset.p)) {
        // T1 holds blocks seen only once; choose its LRU block.
        victim_tag = aset.T1.back();
    } else if (!aset.T2.empty()) {
        // Otherwise, choose the LRU block from T2.
        victim_tag = aset.T2.back();
    } else {
        // If both resident lists are empty (an unlikely situation), return NUM_WAY to indicate bypass.
        return NUM_WAY;
    }

    // Now, map the victim_tag to a cache way.
    // Here we compare the stored tag with each block's tag,
    // which we extract from the cache block's address.
    for (long way = 0; way < NUM_WAY; ++way) {
        // Convert the cache block's address to a uint64_t tag.
        uint64_t block_tag = current_set[way].address.to<uint64_t>();
        if (block_tag == victim_tag)
            return way;
    }

    // If no matching block is found, indicate bypass by returning NUM_WAY (no valid candidate was found).
    return NUM_WAY;
}

// Called when a block is filled in the cache (either in an empty slot or after eviction).
void arc::replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
                                 champsim::address full_addr, champsim::address ip,
                                 champsim::address victim_addr, access_type type)
{
  
}

// Called on every cache access to update ARC state.
void arc::update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                   champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
  
}

// Called at the end of simulation to output ARC-specific statistics.
void arc::replacement_final_stats() {
  
}