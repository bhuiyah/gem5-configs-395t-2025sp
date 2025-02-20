#ifndef REPLACEMENT_ARC_H
#define REPLACEMENT_ARC_H

#include <vector>
#include <deque>
#include "cache.h"
#include "modules.h"

// Adaptive Replacement Cache (ARC) implementation
class arc : public champsim::modules::replacement {
private:
    const long NUM_SET;
    const long NUM_WAY; 
    
    // Track entries in each list per set
    struct ARC_State {
        std::deque<uint64_t> T1;  // Cache ways in T1 (recency)
        std::deque<uint64_t> T2;  // Cache ways in T2 (frequency)
        std::deque<uint64_t> B1;  // Ghost list for T1 evictions
        std::deque<uint64_t> B2;  // Ghost list for T2 evictions
        
        // Target size for T1 (p)
        size_t p;
    };

    // State for each set
    std::vector<ARC_State> arc_states;

    inline uint64_t get_block_tag(champsim::address addr) {
        return addr.to<uint64_t>() >> LOG2_BLOCK_SIZE;
    }

public:
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
};

#endif