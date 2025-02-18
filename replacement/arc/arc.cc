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
    for (size_t set = 0; set < NUM_SET; set++) {
      arc_sets[set].T1.clear();
      arc_sets[set].T2.clear();
      arc_sets[set].B1.clear();
      arc_sets[set].B2.clear();
      arc_sets[set].p = 0; // TODO: change to smth different for experiment
  }
}

// Called when a cache miss occurs to select a victim block in a given set.
long arc::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
                      const champsim::cache_block* current_set, champsim::address ip,
                      champsim::address full_addr, access_type type)
{
    ARC_Set &aset = arc_sets[set];
    uint64_t victim_tag = 0;

    // First check for an invalid (empty) way
    for (long way = 0; way < static_cast<long>(NUM_WAY); ++way) {
        // If the block is invalid, it is a candidate for replacement.
        if (!current_set[way].valid) {
            return way;
        }
    }

    // Writes must always return a valid way
    if (type == access_type::WRITE) {
        // If T1 is not empty, evict from T1
        if (!aset.T1.empty()) {
            victim_tag = aset.T1.back();
        }
        // Otherwise evict from T2 if not empty 
        else if (!aset.T2.empty()) {
            victim_tag = aset.T2.back();
        }
        // If both lists are empty, evict LRU way
        else {
            return static_cast<long>(NUM_WAY - 1);
        }
    }

    // Any other access type, follow ARC replacement policy
    else {
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
            return static_cast<long>(NUM_WAY);
        }
    }

    // Now, map the victim_tag to a cache way.
    // Here we compare the stored tag with each block's tag,
    // which we extract from the cache block's address.
    for (long way = 0; way < static_cast<long>(NUM_WAY); ++way) {
        // Convert the cache block's address to a uint64_t tag.
        uint64_t block_tag = current_set[way].address.to<uint64_t>();
        if (block_tag == victim_tag)
            return way;
    }

    // If no matching block is found, indicate bypass by returning NUM_WAY (no valid candidate was found).
    // If the access type is WRITE (this shouldn't  happen but in case), return the last way.
    return static_cast<long>((type == access_type::WRITE) ? NUM_WAY - 1 : NUM_WAY);
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
    uint64_t victim_tag = victim_addr.to<uint64_t>();

    // Handle the evicted block if there was one
    if (victim_tag != 0) {
        if (in_list(aset.T1, victim_tag)) {
            // Move from T1 to B1
            remove_from_list(aset.T1, victim_tag);
            aset.B1.push_front(victim_tag);
            // Manage B1 size - T1 + B1 should not exceed cache size
            if (aset.T1.size() + aset.B1.size() > NUM_WAY) {
                aset.B1.pop_back(); // Remove LRU entry from B1
            }
        } else if (in_list(aset.T2, victim_tag)) {
            // Move from T2 to B2
            remove_from_list(aset.T2, victim_tag);
            aset.B2.push_front(victim_tag);
            // Manage B2 size - T2 + B2 should not exceed cache size
            if (aset.T2.size() + aset.B2.size() > NUM_WAY) {
                aset.B2.pop_back(); // Remove LRU entry from B2
            }
        }
    }

    // Handle the incoming block
    if (in_list(aset.B1, tag)) {
        // A ghost hit in B1 indicates that blocks from T1 are being re-referenced.
        size_t delta = 1;
        // If |B1| < |B2|: increase p by |B2|/|B1|
        if (aset.B2.size() > aset.B1.size()) {
            delta = aset.B2.size() / aset.B1.size();
        }
        aset.p = std::min(NUM_WAY, aset.p + delta);
        remove_from_list(aset.B1, tag);
        aset.T2.push_front(tag);
    } else if (in_list(aset.B2, tag)) {
        // A ghost hit in B2 indicates frequency is more important.
        size_t delta = 1;
        // If |B2| < |B1|: decrease p by |B1|/|B2|
        if (aset.B1.size() > aset.B2.size()) {
            delta = aset.B1.size() / aset.B2.size();
        }
        aset.p = (delta > aset.p) ? 0 : aset.p - delta;
        remove_from_list(aset.B2, tag);
        aset.T2.push_front(tag);
    } else {
        // Standard case: this is a new block that wasn't in the ghost lists.
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
    for (size_t set = 0; set < NUM_SET; set++) {
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