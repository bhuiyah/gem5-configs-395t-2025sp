#!/usr/bin/env bash

# Run a ChampSim binary over a small set of traces on the Condor cluster.
#
# Usage:
#     ./run_cluster.sh CHAMPSIM_BINARY_NAME
#
# CHAMPSIM_BINARY_NAME: The name of the ChampSim binary to run. It should
#                       be located in the bin/ directory.

# Check that the script is running on a submit node
if [[ ! "$(hostname)" =~ ^(jalad|darmok)$ ]]; then
    printf "Error: Only run cluster jobs on the submit nodes (jalad or darmok).\n"
    exit 1
fi

# Check that args are correct
if [ "${#}" -ne 1 ]; then
    printf "Error: Incorrect # of arguments:\n"
    printf "Usage: ./run_cluster.sh CHAMPSIM_BINARY_NAME\n"
    exit 1
fi

# Parse args
CHAMPSIM_BINARY_NAME="${1}"

# Simulation parameters
TRACE_DIR="/scratch/cluster/speedway/cs395t/hw1/part2/traces"
WARM_INS=25000000
SIM_INS=100000000

# Inferred parameters
CHAMPSIM_DIR="$(pwd)"
CHAMPSIM_BINARY="${CHAMPSIM_DIR}/bin/${CHAMPSIM_BINARY_NAME}"

# Get a trace's name from its trace file path.
#
# trace_path: The path to the trace file.
#
get_trace_name() {
    local trace_path="${1}"
    local trace_name="$(basename "${trace_path}" | cut -d '.' -f 1)"

    echo "${trace_name}"
}

# Get the path to the ChampSim directory.
#
get_champsim_dir() {
    local champsim_dir="$(pwd)"

    echo "${champsim_dir}"
}

# Get the path to a trace output directory.
#
# trace_path: The path to the trace file.
#
get_trace_output_dir() {
    local trace_path="${1}"
    local champsim_dir="$(get_champsim_dir)"
    local trace_name="$(get_trace_name "${trace_path}")"

    echo "${champsim_dir}/out/${trace_name}"
}

# Get the path to a trace output file.
#
# trace_path: The path to the trace file.
# champsim_binary_name: The name of the ChampSim binary.
# suffix: The suffix of the output file.
#
get_trace_output_file() {
    local trace_path="${1}"
    local champsim_binary_name="${2}"
    local suffix="${3}"

    local champsim_dir=$(get_champsim_dir)
    local trace_output_dir=$(get_trace_output_dir "${trace_path}")
    local output_file="${trace_output_dir}/${champsim_binary_name}.${suffix}"

    echo "${output_file}"
}

# Generate a script to launch the trace.
#
# trace_path: The path to the trace file.
# champsim_binary_name: The name of the ChampSim binary.
#
generate_trace_script() {
    local trace_path="${1}"
    local champsim_binary_name="${2}"

    local output_dir="$(get_trace_output_dir "${trace_path}")"
    local script_file="$(get_trace_output_file "${trace_path}" "${champsim_binary_name}" "sh")"
    local json_file="$(get_trace_output_file "${trace_path}" "${champsim_binary_name}" "json")"
    local txt_file="$(get_trace_output_file "${trace_path}" "${champsim_binary_name}" "txt")"
    local binary_file=$(get_champsim_dir)/bin/${champsim_binary_name}
    
    local script_content=\
"#!/bin/bash

stdbuf --output=L ${binary_file} \\
    --warmup-instructions ${WARM_INS} \\
    --simulation-instructions ${SIM_INS} \\
    --json ${json_file} \\
    "${trace_path}" \\
    2>&1 | tee ${txt_file}
"
    # Write script to file and make it executable
    mkdir -p "${output_dir}"
    printf "${script_content}" > "${script_file}"
    chmod u+x "${script_file}"

    # Return path to script file
    echo "${script_file}"
}

# Function to generate a condor file to submit the trace job.
#
# trace_path: The path to the trace file.
# champsim_binary_name: The name of the ChampSim binary.
#
generate_trace_condor() {
    local trace_path="${1}"
    local champsim_binary_name="${2}"

    local output_dir="$(get_trace_output_dir "${trace_path}")"

    # Use condorize.sh to generate the condor file
    mkdir -p "${output_dir}"
    local condor_file=$(/scratch/cluster/speedway/cs395t/hw1/part2/scripts/condorize.sh "${output_dir}" "${champsim_binary_name}")

    # Return path to condor file
    echo "${condor_file}"
}

# Ensure binary exists
if test ! -f ${CHAMPSIM_BINARY} ; then
    printf "Error: Binary ${CHAMPSIM_BINARY_NAME} not found at bin/${CHAMPSIM_BINARY}.\n"
    exit
fi

# Ensure trace dir exists
if test ! -d ${TRACE_DIR}; then
    printf "Error: Could not find trace directory at ${TRACE_DIR}.\n"
    exit
fi

# Generate for each trace
# Make list of traces in TRACE_DIR using glob
TRACE_I=1
TRACE_N=$(ls ${TRACE_DIR} -1c | wc -l)

for TRACE_FILE in ${TRACE_DIR}/*; do    
    TRACE_NAME=$(get_trace_name ${TRACE_FILE})

    printf "=== [${TRACE_I}/${TRACE_N}] ${TRACE_NAME} ===\n"

    # Create script file
    SCRIPT_FILE=$(generate_trace_script ${TRACE_FILE} ${CHAMPSIM_BINARY_NAME})
    printf "Created script file at ${SCRIPT_FILE}\n"

    # Create condor file
    CONDOR_FILE=$(generate_trace_condor ${TRACE_FILE} ${CHAMPSIM_BINARY_NAME})
    printf "Created condor file at ${CONDOR_FILE}\n"

    # Submit condor file
    /lusr/opt/condor/bin/condor_submit ${CONDOR_FILE} -batch-name ${CHAMPSIM_BINARY_NAME}

    TRACE_I=$((TRACE_I + 1))
    printf "\n"
done
