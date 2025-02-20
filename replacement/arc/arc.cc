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

void arc::replacement_cache_fill(uint32_t cpu, long set, long way,
                                 champsim::address addr, champsim::address ip,
                                 champsim::address victim_addr, access_type type)
{
    ARC_State &aset = arc_states[set];
    size_t c = NUM_WAY;

    // FIRST: Handle the victim (evicted) block if there was one
    if (victim_addr.to<uint64_t>() != 0) {
        // Check if it was in T1
        auto it_T1 = std::find(aset.T1.begin(), aset.T1.end(), victim_addr);
        if (it_T1 != aset.T1.end()) {
            aset.T1.erase(it_T1);
            aset.B1.push_front(victim_addr);
            std::cout << "[ARC Fill] Evicted " << victim_addr << " from T1 to B1\n";
        }
        // Check if it was in T2
        auto it_T2 = std::find(aset.T2.begin(), aset.T2.end(), victim_addr);
        if (it_T2 != aset.T2.end()) {
            aset.T2.erase(it_T2);
            aset.B2.push_front(victim_addr);
            std::cout << "[ARC Fill] Evicted " << victim_addr << " from T2 to B2\n";
        }
    }

    size_t B1_sz = std::max(1ul, aset.B1.size());
    size_t B2_sz = std::max(1ul, aset.B2.size());

    auto it_B1 = std::find(aset.B1.begin(), aset.B1.end(), addr);
    auto it_B2 = std::find(aset.B2.begin(), aset.B2.end(), addr);
    
    // Check if `addr` is in B1 => "ghost hit" => Case II
    if (it_B1 != aset.B1.end()) {
        // Increase p
        size_t increase = std::max(1ul, B2_sz / B1_sz);
        aset.p = std::min(c, aset.p + increase);

        aset.B1.erase(it_B1);

        // Insert xᵗ in T2 (MRU)
        aset.T2.push_front(addr);
        std::cout << "[ARC Fill] Ghost hit in B1 => move " << addr << " to T2\n";
    } 
    // Check if `addr` is in B2 => "ghost hit" => Case III
    else if (it_B2 != aset.B2.end()) {
        // Decrease p
        size_t decrease = std::max(1ul, B1_sz / B2_sz);
        aset.p = (aset.p >= decrease) ? (aset.p - decrease) : 0;

        aset.B2.erase(it_B2);
    
        // Insert xᵗ in T2 (MRU)
        aset.T2.push_front(addr);
        std::cout << "[ARC Fill] Ghost hit in B2 => move " << addr << " to T2\n";
    } 
    // Otherwise, brand‐new block => "Case IV"
    else{
        //    If L1 == c, remove from B1 or T1, REPLACE if needed
        size_t l1_size = aset.T1.size() + aset.B1.size(); // L1 = T1 ∪ B1
        size_t l2_size = aset.T2.size() + aset.B2.size(); // L2 = T2 ∪ B2

        if (l1_size >= c) {
            if (aset.T1.size() < c) {
                if (!aset.B1.empty()) { // Evict LRU from B1
                    aset.B1.pop_back();
                }
            } else {
                if (!aset.T1.empty()) { // B1 empty => Evict LRU from T1
                    aset.T1.pop_back();
                }
            }
        }
        else if (l1_size < c) {  // Case B
            size_t total_size = l1_size + l2_size;
            if (total_size >= c) {
                // Delete LRU from B2 only if total size = 2c
                if (total_size == 2*c && !aset.B2.empty()) {
                    aset.B2.pop_back();
                }
            }
        }
        // Insert brand-new line in T1
        aset.T1.push_front(addr);
        std::cout << "[ARC Fill] New block => Insert " << addr << " into T1\n";
    }
}

long arc::find_victim(uint32_t cpu, uint64_t instr_id, long set,
                      const champsim::cache_block* current_set,
                      champsim::address ip, champsim::address addr,
                      access_type type)
{
    ARC_State &aset = arc_states[set];
    champsim::address evicted_addr;

    // *********** CASE A: WRITE REQUESTS ***********
    if (type == access_type::WRITE) {
        auto in_B2 = std::find(aset.B2.begin(), aset.B2.end(), addr) != aset.B2.end();

        // We must never return NUM_WAY for a write fill in a write-allocate cache.
        if (!aset.T1.empty() && (aset.T1.size() > aset.p || (in_B2 && aset.T1.size() == aset.p))) {
            // Evict from T1 (recency list) to ensure we get a valid victim
            evicted_addr = aset.T1.back();
        }
        else if (!aset.T2.empty()) {
            // If T1 is empty, evict from T2 (frequency list)
            evicted_addr = aset.T2.back();
        }
        else {
            // If both T1 and T2 are empty, fallback to the first clean block or way 0
            for (long w = 0; w < NUM_WAY; w++) {
                if (!current_set[w].dirty) return w; // Evict a clean block
            }
            // If all are dirty, pick way 0
            return 0;
        }
    } else {
        // *********** CASE B: READ (and others) ***********
        // Normal ARC approach for read requests. We can bypass if T1/T2 are empty

        // If T1 and T2 are both empty, no victim candidate => bypass
        if (aset.T1.empty() && aset.T2.empty()) {
            return NUM_WAY;
        }
        auto in_B2 = std::find(aset.B2.begin(), aset.B2.end(), addr) != aset.B2.end();

        // Use p-based logic to choose from T1 or T2
        if (!aset.T1.empty() && (aset.T1.size() > aset.p || (in_B2 && aset.T1.size() == aset.p))) {
            evicted_addr = aset.T1.back();
        }
        else if (!aset.T2.empty()) {
            evicted_addr = aset.T2.back();
        }
        else {
            // If T1 is empty or T2 has <= p, no valid eviction => bypass
            return NUM_WAY;
        }
    }

    // ************************
    // address -> way lookup
    // ************************
    //print out the address that is being evicted
    // std::cout << "Evicted address: " << evicted_addr << std::endl;
    for (long w = 0; w < NUM_WAY; w++) {
        if (current_set[w].address == evicted_addr) {
            std::cout << "Successful Eviction from Cache: " << evicted_addr << std::endl;
            return w;
        }
    }
    // we get here => mismatch
    //print out that we have a mismatch
    std::cout << "Eviction Mismatch:" << evicted_addr << std::endl;
    auto it_T1 = std::find(aset.T1.begin(), aset.T1.end(), evicted_addr);
    if (it_T1 != aset.T1.end()) {
        aset.T1.erase(it_T1);
    } else {
        auto it_T2 = std::find(aset.T2.begin(), aset.T2.end(), evicted_addr);
        if (it_T2 != aset.T2.end()) {
            aset.T2.erase(it_T2);
        }
    }
    // if (type == access_type::WRITE) {
    //     // can't bypass => pick something forcibly
    //     // e.g., evict the first valid or the first block in T1 or T2
    //     // or just fallback to way0
    //     return 0;
    // } else {
    //     // read => can bypass
    //     return NUM_WAY;
    // }
    return 0;
}

void arc::update_replacement_state(uint32_t cpu, long set, long way,
                                 champsim::address addr, champsim::address ip,
                                 champsim::address victim_addr, access_type type,
                                 uint8_t hit) {
    ARC_State &aset = arc_states[set];

    //print out that we are updating the replacement state
    std::cout << "Updating replacement state" << std::endl;

    if (way >= NUM_WAY) return;

    if (hit) {
        // print out the address that hit
        std::cout << "Hit: " << addr << std::endl;
        // Case 1: Address is in T1 or T2, HIT
        // Check if the page is in T1 (seen once recently)
        //print all of the addresses in T1
        if (aset.T1.empty()) {
            std::cout << "T1 is empty" << std::endl;
        }else{
            std::cout << "T1: ";
            for (auto it = aset.T1.begin(); it != aset.T1.end(); ++it) {
                // print out T1
                std::cout << *it << " ";
            }
            std::cout << std::endl;
        }
        auto it_T1 = std::find(aset.T1.begin(), aset.T1.end(), addr);
        if (it_T1 != aset.T1.end()) {
            // Move from T1 to MRU position of T2 (upgrade to "seen multiple times")
            aset.T1.erase(it_T1);
            aset.T2.push_front(addr);
            return;
        } 

        // Check if the page is in T2 (seen multiple times)
        //print all of the addresses in T2
        if (aset.T2.empty()) {
            std::cout << "T2 is empty" << std::endl;
        }else{
            std::cout << "T2: ";
            for (auto it = aset.T2.begin(); it != aset.T2.end(); ++it) {
                std::cout << *it << " ";
            }
            std::cout << std::endl;
        }
        auto it_T2 = std::find(aset.T2.begin(), aset.T2.end(), addr);
        if (it_T2 != aset.T2.end()) {
            // Move to MRU position of T2 (maintain frequency)
            aset.T2.erase(it_T2);
            aset.T2.push_front(addr);
            return;
        }
    }
    //If there is a hit, it can't get here
    assert(hit == 0);
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