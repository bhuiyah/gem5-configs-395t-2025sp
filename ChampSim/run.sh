#!/usr/bin/env bash

# Run a ChampSim binary locally on a test trace (matmul_small).
#
# Usage:
#     ./run_cluster.sh CHAMPSIM_BINARY_NAME
#
# CHAMPSIM_BINARY_NAME: The name of the ChampSim binary to run. It should
#                       be located in the bin/ directory.
#
# This runs the script for 0M warmup, and 50M simulation instructions,
# as a sanity check. You can change these values as needed.

# Check that the script is not running on a submit node
if [[ "$(hostname)" =~ ^(jalad|darmok)$ ]]; then
    printf "Error: Don't run locally on jalad or darmok!!!\n"
    exit
fi

# Check that args are correct
if [ "$#" -ne 1 ]; then
    printf "Error: Incorrect # of arguments:\n"
    printf "Usage: ./run.sh CHAMPSIM_BINARY_NAME\n"
    exit
fi

# Parse args
CHAMPSIM_BINARY_NAME=$1

# Simulation parameters
TRACE="/projects/coursework/2025-spring/cs395t-lin/bhuiyah/predmech/part2/traces/bfs.trace.gz"
WARM_INS=0
SIM_INS=125000000

# Inferred parameters
CHAMPSIM_BINARY=./bin/${CHAMPSIM_BINARY_NAME}
TRACE_NAME=$(basename "${TRACE}" | cut -d '.' -f 1)
OUTPUT_DIR="./out/${TRACE_NAME}"
OUTPUT_JSON_FILE="${OUTPUT_DIR}/${CHAMPSIM_BINARY_NAME}.json"
OUTPUT_TXT_FILE="${OUTPUT_DIR}/${CHAMPSIM_BINARY_NAME}.txt"

# Ensure binary exists
if [ ! -f ${CHAMPSIM_BINARY} ]; then
    printf "Error: Binary ${CHAMPSIM_BINARY_NAME} not found at bin/${CHAMPSIM_BINARY}.\n"
    exit
fi

# Ensure trace file exists
if [ ! -f ${TRACE} ]; then
    printf "Error: Could not find trace file at ${TRACE}.\n"
    exit
fi

# Ensure output dir exists
if test ! -d ${OUTPUT_DIR}; then
    mkdir -p ${OUTPUT_DIR}
fi

# Run our cache replacement policy / branch predictor on the trace
# Print to terminal (stdbuf --output=L) and file (tee)
printf "=== Running ${CHAMPSIM_BINARY_NAME} on ${TRACE_NAME} ===\n"

CMD="stdbuf --output=L ${CHAMPSIM_BINARY} \
--warmup-instructions ${WARM_INS} \
--simulation-instructions ${SIM_INS} \
--json ${OUTPUT_JSON_FILE} \
${TRACE} \
2>&1 | tee ${OUTPUT_TXT_FILE}"

printf "=== Running command: ${CMD} ===\n"

eval ${CMD}

printf "=== Run complete ===\n"
