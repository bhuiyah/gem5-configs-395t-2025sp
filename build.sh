#!/usr/bin/env bash

# Build one or more ChampSim configurations.
#
# Usage:
#    ./build.sh CONFIG_FILE_1 [CONFIG_FILE_2 ...]
#
# CONFIG_FILE_i: The path to the configuration file to build.

# Check that the script is not running on a submit node
if [[ "$(hostname)" =~ ^(jalad|darmok)$ ]]; then
    printf "Error: Don't run locally on jalad or darmok!!!\n"
    exit
fi

# Check that args are correct
if [ "$#" -lt 1 ]; then
    printf "Error: Incorrect # of arguments:\n"
    printf "Usage: ./build.sh CONFIG_FILE_1 [CONFIG_FILE_2 ...]\n"
    exit
fi

# Parse args
CONFIG_FILES=("$@")

# Build a single configuration.
#
# config_file: The path to the configuration file.
#
build_config() {
    local config_file="${1}"

    printf "Running config.sh for ${config_file}\n"
    ./config.sh "${config_file}"

    printf "Building binary for ${config_file}\n"
    make -j 4
}

# Build all configurations
CONFIG_I=1
CONFIG_N=${#CONFIG_FILES[@]}

for CONFIG_FILE in "${CONFIG_FILES[@]}"; do
    printf "=== [${CONFIG_I}/${CONFIG_N}] ${CONFIG_FILE} ===\n"
    build_config "${CONFIG_FILE}"

    CONFIG_I=$((CONFIG_I + 1))
    printf "\n"
done