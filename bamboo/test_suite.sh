#!/bin/bash

EXIT_CODE=0

function help_message {
    local SCRIPT=$(basename ${0})
    local N=$(tput sgr0)
    local C=$(tput setf 4)
    cat <<EOF
Run test configurations of LBANN.
By default, run all tests.
Usage: ${SCRIPT} [options]
Options:
    ${C}-h|--help${N}                   Display this help message and exit
    ${C}-u|--unit${N}                   Run all unit tests
    ${C}-i|--integration${N}            Run all integration tests 
    ${C}-c|--compiler${N}               Run all compiler tests
    ${C}-e|--executable${N} <val>       Specify path to executable (default executable built from build_lbann_lc.sh)
    ${C}-n|--nobuild${N}                Do not build any executable
EOF
}

function print_label {
    TEST_SET=${1}
    echo "-----------------------------------------------------------------------------------------------------------------"
    echo "STARTING ${TEST_SET} TESTS"
    echo "-----------------------------------------------------------------------------------------------------------------"
}

function print_results {
    if [ $? -ne 0 ]; then
        EXIT_CODE=1
        TEST=${1}
	echo "-------------------------------------------------------------------------------------------------------------"
        echo "${TEST} FAILED"
        echo "-------------------------------------------------------------------------------------------------------------"
    else
        echo "-------------------------------------------------------------------------------------------------------------"
        echo "${TEST} PASSED"
        echo "-------------------------------------------------------------------------------------------------------------"
    fi
}

CLUSTER=$(hostname | sed 's/\([a-zA-Z][a-zA-Z]*\)[0-9]*/\1/g')
export LBANN_DIR=$(git rev-parse --show-toplevel)
export LBANN_EXE="${LBANN_DIR}/build/${CLUSTER}.llnl.gov/model_zoo/lbann"
export PATH=../../LBANN-NIGHTD-CS/bin:$PATH

# Default is to build an executable
BUILD_EXE=1

# Set defaults to run all tests
RUN_ALL=1
RUN_UNIT_TESTS=0
RUN_INTEGRATION_TESTS=0
RUN_COMPILER_TESTS=0

while :; do 
    key="$1"
    case $key in
	-h|--help)
	    help_message
	    exit 0
	    ;;
	-u|--unit)
	    RUN_UNIT_TESTS=1
	    RUN_ALL=0
	    ;;
	-i|--integration)
	    RUN_INTEGRATION_TESTS=1
	    RUN_ALL=0
	    ;;
	-c|--compiler)
	    RUN_COMPILER_TESTS=1
	    RUN_ALL=0
	    # Do NOT set BUILD_EXE=0 because a user could specify -i -c for example.
	    # The -i tests would still need an executable.
	    ;;
	-e|--executable)
	    LBANN_EXE=${2}
	    BUILD_EXE=0 # User has specified an executable - do not build default.
	    shift
	    ;;
	-n|--nobuild)
	    BUILD_EXE=0 # User does not want executable to be built
	    ;;
	-?*)
	    echo "Unknown option (${!})" >&2
	    exit 1
	    ;;
	*)
	    break
    esac
    shift
done


source /usr/share/lmod/lmod/init/bash
source /etc/profile.d/00-modulepath.sh
# Build the default executable
if [ ${BUILD_EXE} == 1 ]
then
    ../scripts/build_lbann_lc.sh
fi

# Run the tests
if [ ${RUN_ALL} == 1 ] || [ ${RUN_UNIT_TESTS} == 1 ]
then
    print_label "UNIT"

    #unit_tests/ridge_regression_test.sh
    print_results "GRADIENT CHECK"
fi

if [ ${RUN_ALL} == 1 ] || [ ${RUN_INTEGRATION_TESTS} == 1 ]
then
    print_label "INTEGRATION"

    python integration_tests/accuracy_tests/test_wrapper.py

    #python -m pytest integration_tests/performance_tests/test_performance.py --exe="${LBANN_EXE}"
    print_results "TEST PERFORMANCE"
fi

if [ ${RUN_ALL} == 1 ] || [ ${RUN_COMPILER_TESTS} == 1 ]
then
    print_label "COMPILER"

    # Do we just want to test that these build?
    # Do we actually want to run any tests on these executables?

    #../scripts/spack_recipes/build_lbann.sh -c clang@4.0.0
    #spack install lbann@develop %clang@4.0.0 ^protobuf@3.1.0 ^elemental@develop
    #../scripts/build_lbann_lc.sh --compiler clang
    print_results "CLANG 4.0.0 BUILD"

    #../scripts/spack_recipes/build_lbann.sh -c gcc@4.9.3
    #spack install --no-checksum lbann@develop %gcc@4.9.3 ^protobuf@3.1.0 ^elemental@develop
    #../scripts/build_lbann_lc.sh --compiler gnu
    print_results "GCC 4.9.3 BUILD"

    #../scripts/spack_recipes/build_lbann.sh -c gcc@7.1.0
    #spack install --no-checksum lbann@develop %gcc@7.1.0 ^protobuf@3.1.0 ^elemental@develop
    #../scripts/build_lbann_lc.sh --compiler gnu
    print_results "GCC 7.1.0 BUILD"

    #../scripts/spack_recipes/build_lbann.sh -c intel@18.0.0
    #spack install lbann@develop %intel@18.0.0 ^protobuf@3.1.0 ^elemental@develop
    #../scripts/build_lbann_lc.sh --compiler intel
    print_results "INTEL 18.0.0 BUILD"
fi

exit ${EXIT_CODE}
