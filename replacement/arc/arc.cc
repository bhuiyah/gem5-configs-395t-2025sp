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
    ARC_Set &aset = arc_sets[set];
    // Extract the tag from full_addr. Here we simply convert the full address,
    // but you could mask out offset and index bits if desired.
    uint64_t tag = full_addr.to<uint64_t>();

    // Check the ghost lists for a previous occurrence.
    if (in_list(aset.B1, tag)) {
        // A ghost hit in B1 indicates that blocks from T1 are being re-referenced.
        // Increase p (up to the number of ways), remove from B1, and promote to T2.
        aset.p = std::min((uint32_t)NUM_WAY, aset.p + 1);
        remove_from_list(aset.B1, tag);
        aset.T2.push_front(tag);
    } else if (in_list(aset.B2, tag)) {
        // A ghost hit in B2 indicates frequency is more important.
        // Decrease p (ensuring it does not go below zero), remove from B2, and promote to T2.
        aset.p = (aset.p > 0 ? aset.p - 1 : 0);
        remove_from_list(aset.B2, tag);
        aset.T2.push_front(tag);
    } else {
        // Standard case: this is a new block that wasn't in the ghost lists.
        // Insert it into T1 (the recency list).
        aset.T1.push_front(tag);
    }
}

// Called on every cache access to update ARC state.
void arc::update_replacement_state(uint32_t triggering_cpu, long set, long way,
                                   champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type, uint8_t hit)
{
    ARC_Set &aset = arc_sets[set];
    uint64_t tag = full_addr.to<uint64_t>();

    if (hit) {
        // On a hit, determine which list the block is in.
        if (in_list(aset.T1, tag)) {
            // A hit in T1 indicates that the block has now been accessed twice.
            // Promote it to T2 (frequency list) to reflect that it is being reused.
            remove_from_list(aset.T1, tag);
            aset.T2.push_front(tag);
        } else if (in_list(aset.T2, tag)) {
            // If the block is already in T2, simply refresh its position
            // by moving it to the MRU (front) of T2.
            remove_from_list(aset.T2, tag);
            aset.T2.push_front(tag);
        }
        // (If the block is somehow in a ghost list, that case should have been handled in replacement_cache_fill().)
    }
    // No additional action is taken on a miss here.
}

// Called at the end of simulation to output ARC-specific statistics.
void arc::replacement_final_stats() {
    std::cout << "ARC Replacement Final Stats:\n";
    for (long set = 0; set < NUM_SET; set++) {
        ARC_Set &aset = arc_sets[set];
        std::cout << "Set " << set 
                  << " | p = " << aset.p 
                  << " | T1 size = " << aset.T1.size() 
                  << " | T2 size = " << aset.T2.size() 
                  << " | B1 size = " << aset.B1.size() 
                  << " | B2 size = " << aset.B2.size() 
                  << "\n";
    }
}

// Returns true if 'tag' is found in the given list.
bool arc::in_list(const std::deque<uint64_t>& list, uint64_t tag) {
    return std::find(list.begin(), list.end(), tag) != list.end();
}

// Removes the first occurrence of 'tag' from the list.
void arc::remove_from_list(std::deque<uint64_t>& list, uint64_t tag) {
    auto it = std::find(list.begin(), list.end(), tag);
    if (it != list.end())
        list.erase(it);
}