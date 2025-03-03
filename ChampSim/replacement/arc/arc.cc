#include "arc.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <fstream>  // for file output
#include <ctime>
#include <unistd.h>
#include <sstream>

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
        // arc_states[set].p = NUM_WAY / 2;
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
        uint64_t tag = get_block_tag(victim_addr);
        auto it_T1 = std::find(aset.T1.begin(), aset.T1.end(), tag);
        if (it_T1 != aset.T1.end()) {
            aset.T1.erase(it_T1);
            if (aset.B1.size() >= GHOST_MIN) {
                aset.B1.pop_back();
            }
            aset.B1.push_front(tag);
        }
        // Check if it was in T2
        auto it_T2 = std::find(aset.T2.begin(), aset.T2.end(), tag);
        if (it_T2 != aset.T2.end()) {
            aset.T2.erase(it_T2);
            if (aset.B2.size() >= GHOST_MIN) {
                aset.B2.pop_back();
            }
            aset.B2.push_front(tag);
        }
    }
    auto tag = get_block_tag(addr);

    size_t B1_sz = std::max(1ul, aset.B1.size());
    size_t B2_sz = std::max(1ul, aset.B2.size());

    auto it_B1 = std::find(aset.B1.begin(), aset.B1.end(), tag);
    auto it_B2 = std::find(aset.B2.begin(), aset.B2.end(), tag);
    
    // Check if `addr` is in B1 => "ghost hit" => Case II
    if (it_B1 != aset.B1.end()) {
        // Increase p
        size_t increase = std::max(1ul, B2_sz / B1_sz);
        aset.p = std::min(c, aset.p + increase);
        aset.B1.erase(it_B1);
            // Enforce maximum size for B1:
        if (aset.B1.size() >= GHOST_MIN) {
            aset.B1.pop_back();
        }
        // Insert xᵗ in T2 (MRU)
        aset.T2.push_front(tag);
    } 
    // Check if `addr` is in B2 => "ghost hit" => Case III
    else if (it_B2 != aset.B2.end()) {
        // Decrease p
        size_t decrease = std::max(1ul, B1_sz / B2_sz);
        aset.p = (aset.p >= decrease) ? (aset.p - decrease) : 0;

        aset.B2.erase(it_B2);
        if (aset.B2.size() >= GHOST_MIN) {
            aset.B2.pop_back();
        }
        // Insert xᵗ in T2 (MRU)
        aset.T2.push_front(tag);
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
        aset.T1.push_front(tag);
    }
    update_history();
}

long arc::find_victim(uint32_t cpu, uint64_t instr_id, long set,
                      const champsim::cache_block* current_set,
                      champsim::address ip, champsim::address addr,
                      access_type type)
{
    ARC_State &aset = arc_states[set];

    // Proactive cleanup: remove entries in T1 and T2 that are not in the current cache set.
    // cleanup_state(set, current_set);

    uint64_t evicted_addr;

    // *********** CASE A: WRITE REQUESTS ***********
    if (type == access_type::WRITE) {
        auto tag = get_block_tag(addr);
        auto in_B2 = std::find(aset.B2.begin(), aset.B2.end(), tag) != aset.B2.end();

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
        auto tag = get_block_tag(addr);
        auto in_B2 = std::find(aset.B2.begin(), aset.B2.end(), tag) != aset.B2.end();

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
        if (get_block_tag(current_set[w].address) == evicted_addr) {
            return w;
        }
    }
    // we get here => mismatch

    if (type == access_type::WRITE) {
        // can't bypass => pick something forcibly
        // e.g., evict the first valid or the first block in T1 or T2
        // or just fallback to way0
        return 0;
    } else {
        // read => can bypass
        return NUM_WAY;
    }
}

void arc::update_replacement_state(uint32_t cpu, long set, long way,
                                 champsim::address addr, champsim::address ip,
                                 champsim::address victim_addr, access_type type,
                                 uint8_t hit) {
    ARC_State &aset = arc_states[set];


    if (way >= NUM_WAY) return;

    if (hit) {
        // Case 1: Address is in T1 or T2, HIT
        // Check if the page is in T1 (seen once recently)
        auto tag = get_block_tag(addr);
        auto it_T1 = std::find(aset.T1.begin(), aset.T1.end(), tag);
        if (it_T1 != aset.T1.end()) {
            // Move from T1 to MRU position of T2 (upgrade to "seen multiple times")
            aset.T1.erase(it_T1);
            aset.T2.push_front(tag);
            return;
        } 

        // Check if the page is in T2 (seen multiple times)
        auto it_T2 = std::find(aset.T2.begin(), aset.T2.end(), tag);
        if (it_T2 != aset.T2.end()) {
            // Move to MRU position of T2 (maintain frequency)
            aset.T2.erase(it_T2);
            aset.T2.push_front(tag);
            return;
        }
    }
    //If there is a hit, it can't get here
    // assert(hit == 0);
}

void arc::replacement_final_stats() {
    // Generate a unique file name using process ID and current time.
    std::time_t now = std::time(nullptr);
    pid_t pid = getpid();
    std::ostringstream oss;
    oss << "arc_policy_selector_log_" << pid << "_" << now << ".txt";
    std::string file_name = oss.str();

    // Open the file for writing.
    std::ofstream log_file(file_name);
    if(log_file.is_open()) {
        log_file << "p_history = [";
        for (auto p : p_history) {
            log_file << p << ", ";
        }
        log_file << "]\n";
        log_file << "b1_history = [";
        for (auto b1 : b1_history) {
            log_file << b1 << ", ";
        }
        log_file << "]\n";
        log_file << "b2_history = [";
        for (auto b2 : b2_history) {
            log_file << b2 << ", ";
        }
        log_file << "]\n";
        log_file.close();
    }
    else {
        std::cerr << "Failed to open file for writing: " << file_name << std::endl;
    }
}

extern const unsigned LOG2_BLOCK_SIZE;
inline uint64_t get_block_tag(champsim::address addr) {
    return addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
}

void arc::update_history() {
    double sum_p  = 0.0, sum_b1 = 0.0, sum_b2 = 0.0;
    for (long set = 0; set < NUM_SET; set++) {
        sum_p  += static_cast<double>(arc_states[set].p);
        sum_b1 += static_cast<double>(arc_states[set].B1.size());
        sum_b2 += static_cast<double>(arc_states[set].B2.size());
    }
    double avg_p  = sum_p  / static_cast<double>(NUM_SET);
    double avg_b1 = sum_b1 / static_cast<double>(NUM_SET);
    double avg_b2 = sum_b2 / static_cast<double>(NUM_SET);
    p_history.push_back(avg_p);
    b1_history.push_back(avg_b1);
    b2_history.push_back(avg_b2);
}