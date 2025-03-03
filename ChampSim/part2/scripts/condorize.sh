#!/usr/bin/env bash

# Generate a Condor file to run a ChampSim simulation on the cluster.
#
# Usage:
#     ./condorize.sh OUTPUT_DIR JOB_NAME
#
# OUTPUT_DIR: The directory to write the job script and condor file to.
# JOB_NAME: The name of the job.

# Check that args are correct
if [ "$#" -ne 2 ]; then
    printf "Error: Incorrect # of arguments:\n"
    printf "Usage: ./condor.sh OUTPUT_DIR JOB_NAME\n"
    exit
fi

# Parse args
OUTPUT_DIR="${1}"
JOB_NAME="${2}"

# Get the group the user should submit the job with.
#
# This helps determine the job's priority on the cluster.
#     In 'grad' usergroup -> submit as GRAD
#     In 'under' usergroup -> submit as UNDER
#     Otherwise -> submit as GUEST.
#
get_group() {
    if groups | grep -q "\bgrad\b"; then
        echo "GRAD"
    elif groups | grep -q "\bunder\b"; then
        echo "UNDER"
    else
        echo "GUEST"
    fi
}

# Inferred parameters
GROUP="$(get_group)"
PROJECT="ARCHITECTURE"
DESCRIPTION="CS 395T Homework 1"
EMAIL="${USER}@cs.utexas.edu"
SCRIPT_FILE="${OUTPUT_DIR}/${JOB_NAME}.sh"
CONDOR_FILE="${OUTPUT_DIR}/${JOB_NAME}.condor"

# Create condor file
mkdir -p "${OUTPUT_DIR}"

CONDOR="+Group=\"${GROUP}\"
+Project=\"${PROJECT}\"
+ProjectDescription=\"${DESCRIPTION}\"

universe=vanilla
getenv=true
Rank=Memory
notification=Error
error=${JOB_NAME}.condor.err
output=${JOB_NAME}.condor.out
log=${JOB_NAME}.condor.log
notify_user=${EMAIL}
initial_dir=${OUTPUT_DIR}
executable=${SCRIPT_FILE}
requirements=InMastodon

queue"

printf "${CONDOR}" > "${CONDOR_FILE}"
echo "${CONDOR_FILE}"
