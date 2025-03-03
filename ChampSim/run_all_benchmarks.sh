#!/bin/bash
# run_arc_tests.sh
# Launch ARC tests in parallel and log their PIDs.

PID_FILE="arc_test_pids.txt"
# Clear the PID log file
> "$PID_FILE"

# Define a function to run a test and log its PID.
run_test() {
    local test_name="$1"
    local cmd="$2"
    echo "Launching test: $test_name..."
    # Run the command in background; redirect both stdout and stderr to the appropriate file.
    eval "$cmd" &
    local pid=$!
    echo "$test_name: $pid" >> "$PID_FILE"
}

# Run the tests
run_test "bfs" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_bfs_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/bfs.trace.gz > spec_runs/bhalf_spec_bfs_arc.txt 2>&1'
run_test "astar" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_astar_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/astar_313B.trace.gz > spec_runs/bhalf_spec_astar_arc.txt 2>&1'
run_test "cc" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_cc_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/cc.trace.gz > spec_runs/bhalf_spec_cc_arc.txt 2>&1'
run_test "mcf" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_mcf_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/mcf_46B.trace.gz > spec_runs/bhalf_spec_mcf_arc.txt 2>&1'
run_test "omnetpp" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_omnetpp_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/omnetpp_340B.trace.gz > spec_runs/bhalf_spec_omnetpp_arc.txt 2>&1'
run_test "soplex" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_soplex_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/soplex_66B.trace.gz > spec_runs/bhalf_spec_soplex_arc.txt 2>&1'
run_test "sphinx" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_sphinx_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/sphinx3_2520B.trace.gz > spec_runs/bhalf_spec_sphinx_arc.txt 2>&1'
run_test "xalanc" 'stdbuf --output=L bin/champsim_skylake_arc4l2c --warmup-instructions 25000000 --simulation-instructions 100000000 --json spec_runs/bhalf_spec_xalanc_arc.json /scratch/cluster/speedway/cs395t/hw1/part2/traces/xalancbmk_99B.trace.gz > spec_runs/bhalf_spec_xalanc_arc.txt 2>&1'

echo "All tests launched. PIDs are recorded in $PID_FILE."