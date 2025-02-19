#include "arc.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdint>

// Constructor that uses the cache object to determine the number of sets and ways.
arc::arc(CACHE* cache) : arc(cache, cache->NUM_SET, cache->NUM_WAY) {}

// Constructor with explicit sets and ways.
arc::arc(CACHE* cache, long sets, long ways)
    : replacement(cache), NUM_SET(sets), NUM_WAY(ways), arc_states(sets) { initialize_replacement(); }

void arc::initialize_replacement() {
    // Initialize each ARC set.
    for (long set = 0; set < NUM_SET; set++) {
        arc_states[set].T1.clear();
        arc_states[set].T2.clear();
        arc_states[set].B1.clear();
        arc_states[set].B2.clear();
        arc_states[set].p = 0; // TODO: change to smth different for experiment
    }
}

long arc::find_victim(uint32_t cpu, uint64_t instr_id, long set, 
                     const champsim::cache_block* current_set,
                     champsim::address ip, champsim::address addr, 
                     access_type type) {
    ARC_State &aset = arc_states[set];

    champsim::address victim_addr;

    // Handle WRITE requests separately
    if (type == access_type::WRITE) {
        if (!aset.T1.empty()) {
            // Prefer evicting from T1 (recency list)
            victim_addr = aset.T1.back();
            aset.T1.pop_back();
            aset.B1.push_front(victim_addr);  // Move evicted block to B1 (ghost list)

            // Ensure B1 does not grow beyond NUM_WAY
            if (aset.B1.size() > (size_t)NUM_WAY) {
                aset.B1.pop_back();
            }
        } 
        else if (!aset.T2.empty()) {
            // If T1 is too small, evict from T2 (frequency list)
            victim_addr = aset.T2.back();
            aset.T2.pop_back();
            aset.B2.push_front(victim_addr);  // Move evicted block to B2 (ghost list)

            // Ensure B2 does not grow beyond NUM_WAY
            if (aset.B2.size() > (size_t)NUM_WAY) {
                aset.B2.pop_back();
            }
        }
        else {
            // Fallback: Evict the first clean block encountered, else choose way 0
            for (long way = 0; way < NUM_WAY; way++) {
                if (!current_set[way].dirty) {
                    return way;
                }
            }
        }

        // current_set[way].valid && 
        for (long way = 0; way < NUM_WAY; way++) {
            if (current_set[way].address == victim_addr) {
                return way;
            }
        }
    } else { // Handle READ requests
        if (aset.T1.empty() && aset.T2.empty()) {
            return NUM_WAY;  // Indicating no eviction possible, bypass
        }                   
        
        if (!aset.T1.empty() && aset.T1.size() > aset.p) {
            victim_addr = aset.T1.back();
            aset.T1.pop_back();
            aset.B1.push_front(victim_addr);  // Move evicted block to B1 (ghost list)

            // Ensure B1 does not grow beyond NUM_WAY
            if (aset.B1.size() > (size_t)NUM_WAY) {
                aset.B1.pop_back();
            }
        } else if (!aset.T2.empty()) {
            victim_addr = aset.T2.back();
            aset.T2.pop_back();
            aset.B2.push_front(victim_addr);  // Move evicted block to B2 (ghost list)

            // Ensure B2 does not grow beyond NUM_WAY
            if (aset.B2.size() > (size_t)NUM_WAY) {
                aset.B2.pop_back();
            }
        } else {
            return NUM_WAY;  // No valid eviction candidate
        }

        for (long way = 0; way < NUM_WAY; way++) {
            if (current_set[way].address == victim_addr) {
                return way;
            }
        }
        return 0;
    }
    return 0;
}

void arc::update_replacement_state(uint32_t cpu, long set, long way,
                                 champsim::address addr, champsim::address ip,
                                 champsim::address victim_addr, access_type type,
                                 uint8_t hit) {
    ARC_State &aset = arc_states[set];

    if (way >= NUM_WAY) return;

    if (hit) {
        // Only process actual hits (ignore misses and writebacks)
        if (type == access_type::WRITE) { return; }

        // Check if the page is in T1 (seen once recently)
        auto it_T1 = std::find(aset.T1.begin(), aset.T1.end(), addr);
        if (it_T1 != aset.T1.end()) {
            // Move from T1 to MRU position of T2 (upgrade to "seen multiple times")
            aset.T1.erase(it_T1);
            aset.T2.push_front(addr);
            return;
        } 

        // Check if the page is in T2 (seen multiple times)
        auto it_T2 = std::find(aset.T2.begin(), aset.T2.end(), addr);
        if (it_T2 != aset.T2.end()) {
            // Move to MRU position of T2 (maintain frequency)
            aset.T2.erase(it_T2);
            aset.T2.push_front(addr);
            return;
        }
    }

    // Handle cache misses (fills)
    auto it_B1 = std::find(aset.B1.begin(), aset.B1.end(), addr);
    auto it_B2 = std::find(aset.B2.begin(), aset.B2.end(), addr);

    size_t B1_size = std::max(1ul, aset.B1.size());
    size_t B2_size = std::max(1ul, aset.B2.size());

    if (it_B1 != aset.B1.end()) {
        size_t increase = std::max(1ul, B2_size / B1_size);
        aset.p = std::min(static_cast<size_t>(NUM_WAY), aset.p + increase);
        aset.B1.erase(it_B1);
        aset.T2.push_front(addr);
    } 
    else if (it_B2 != aset.B2.end()) {
        size_t decrease = std::max(1ul, B1_size / B2_size);
        aset.p = (aset.p >= decrease) ? (aset.p - decrease) : 0;
        aset.B2.erase(it_B2);
        aset.T2.push_front(addr);
    }
    else {
        // Neither B1 nor B2 contains the address, insert into T1
        aset.T1.push_front(addr);
    }

    // **Handle Cache Replacement If Necessary**
    size_t cache_capacity = aset.T1.size() + aset.T2.size();
    if (cache_capacity >= static_cast<size_t>(NUM_WAY)) {
        champsim::address evicted_addr;

        if (aset.T1.size() >= aset.p) {
            // **Evict from T1 and move to B1**
            evicted_addr = (!victim_addr.to<uint64_t>()) ? aset.T1.back() : victim_addr;
            aset.T1.pop_back();
            aset.B1.push_front(evicted_addr);
        } 
        else {
            // **Evict from T2 and move to B2**
            evicted_addr = (!victim_addr.to<uint64_t>()) ? aset.T2.back() : victim_addr;
            aset.T2.pop_back();
            aset.B2.push_front(evicted_addr);
        }
    }

    // **Ensure `B1 + B2` does not grow indefinitely, remove LRU only when necessary**
    if (aset.T1.size() + aset.B1.size() >= static_cast<size_t>(NUM_WAY)) {
        aset.B1.pop_back(); // Evict LRU from B1 if `T1 + B1 == NUM_WAY`
    } 
    else if (aset.T1.size() + aset.T2.size() + aset.B1.size() + aset.B2.size() >= 2 * static_cast<size_t>(NUM_WAY)) {
        aset.B2.pop_back(); // Evict LRU from B2 if `T1 + T2 + B1 + B2` exceeds cache size
    }
}

void arc::replacement_cache_fill(uint32_t cpu, long set, long way,
                               champsim::address addr, champsim::address ip,
                               champsim::address victim_addr, access_type type) {

}

void arc::replacement_final_stats() {
    std::cout << "ARC Replacement Final Stats:\n";
    for (long set = 0; set < NUM_SET; set++) {
        ARC_State &aset = arc_states[set];
        std::cout << "Set " << set 
                  << " | p = " << aset.p 
                  << " | T1 size = " << aset.T1.size() 
                  << " | T2 size = " << aset.T2.size() 
                  << " | B1 size = " << aset.B1.size() 
                  << " | B2 size = " << aset.B2.size() 
                  << "\n";
    }
}