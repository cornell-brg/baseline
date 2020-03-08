// Copyright (c) 2020, University of Washington All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice, this list
// of conditions and the following disclaimer.
//
// Redistributions in binary form must reproduce the above copyright notice, this
// list of conditions and the following disclaimer in the documentation and/or
// other materials provided with the distribution.
//
// Neither the name of the copyright holder nor the names of its contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
// ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Parallel Sort (C = sort(A)).

// A and C's sizes are WIDTH.
// Each time the kernel is called, it is run for (NUM_ITER + 1)
// iterations and the first iteration is discarded.
// 
// NOTE: 3 * WIDTH <= 4KB, the size of DMEM on the tile.
#include <iterator>
#include <algorithm>
#include "parallel_sort.hpp"

// Matrix sizes:
#define WIDTH  64
#define NUM_ITER 1

// Host Parallel Sort code (to compare results)
template <typename TA, typename TC>
void parallel_sort (TA *A, TC *C, uint64_t N) {
        std::sort(A, A + N);
        memcpy(C, A, N * sizeof(TA));
        return;
}


// Helper merge function for the final merge
template <typename TC>
void merge(TC *C, int left, int middle, int right) {
    int i = 0;
    int j = 0;
    int k = 0;
    int left_length = middle - left + 1;
    int right_length = right - middle;
    TC left_array[left_length];
    TC right_array[right_length];
    
    /* copy values to left array */
    for (int i = 0; i < left_length; i ++) {
        left_array[i] = C[left + i];
    }
    
    /* copy values to right array */
    for (int j = 0; j < right_length; j ++) {
        right_array[j] = C[middle + 1 + j];
    }
    
    i = 0;
    j = 0;
    /** chose from right and left arrays and copy */
    while (i < left_length && j < right_length) {
        if (left_array[i] <= right_array[j]) {
            C[left + k] = left_array[i];
            i ++;
        } else {
            C[left + k] = right_array[j];
            j ++;
        }
        k ++;
    }
    
    /* copy the remaining values to the array */
    while (i < left_length) {
        C[left + k] = left_array[i];
        k ++;
        i ++;
    }
    while (j < right_length) {
        C[left + k] = right_array[j];
        k ++;
        j ++;
    }
}

// Final merge of each tile's sorted portion
// note that this must be done on the host
template <typename TC>
void final_merge(TC *C, int number, int aggregation, int N_ELMS) {
    for (int i = 0; i < number; i = i + 2) {
        int left = i * (N_ELMS * aggregation);
        int right = ((i + 2) * N_ELMS * aggregation) - 1;
        int middle = left + (N_ELMS * aggregation) - 1;
        if (right >= WIDTH) {
            right = WIDTH - 1;
        }
        merge(C, left, middle, right);
    }
    if (number / 2 >= 1) {
        final_merge(C, number / 2, aggregation * 2, N_ELMS);
    }
}

// Compute the sum of squared error between vectors A and B (M x N)
template <typename T>
double vector_sse (const T *A, const T *B, uint64_t N) {
        double sum = 0;
        for (uint64_t x = 0; x < N; x ++) {
                T diff = A[x] - B[x];
                if(std::isnan(diff)){
                        return diff;
                }
                sum += diff * diff;
        }
        return sum;
}

template <typename T>
bool compare_vectors (const T *C, const T *gold, uint64_t N) {
        bool rc = 1;
        for (uint64_t x = 0; x < N; x ++) {
                if (C[x] != gold[x]) {
                        rc = 0;
                        break;
                }
        }
        return rc;
}

// Run a parallel sort test on the Manycore, and compare the result.  A
// is the input vector, C is the destination, and gold is the
// known-good result computed by the host.
template<typename TA, typename TC>
int run_test(hb_mc_device_t &device, const char* kernel,
             const TA *A, TC *C, const TC *gold,
             const eva_t &A_device,
             const eva_t &B_device,
             const eva_t &C_device,
             const hb_mc_dimension_t &tg_dim,
             const hb_mc_dimension_t &grid_dim,
             const hb_mc_dimension_t block_size,
             const unsigned int tag){
        int rc;

        // Copy A from host onto device DRAM.
        void *dst = (void *) ((intptr_t) A_device);
        void *src = (void *) &A[0];
        rc = hb_mc_device_memcpy (&device, dst, src,
                                  (WIDTH) * sizeof(TA),
                                  HB_MC_MEMCPY_TO_DEVICE);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to copy memory to device.\n");
                return rc;
        }


        // Prepare list of input arguments for kernel. See the kernel source
        // file for the argument uses.
        uint32_t cuda_argv[8] = {A_device, B_device, C_device,
                                  WIDTH, block_size.y, block_size.x,
                                  tag, NUM_ITER};

        // Enquque grid of tile groups, pass in grid and tile group dimensions,
        // kernel name, number and list of input arguments
        rc = hb_mc_kernel_enqueue (&device, grid_dim, tg_dim,
                                   kernel, 8, cuda_argv);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to initialize grid.\n");
                return rc;
        }


        // Launch and execute all tile groups on device and wait for all to
        // finish.
        rc = hb_mc_device_tile_groups_execute(&device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to execute tile groups.\n");
                return rc;
        }


        // Copy result vector back from device DRAM into host memory.
        src = (void *) ((intptr_t) C_device);
        dst = (void *) &C[0];
        rc = hb_mc_device_memcpy (&device, (void *) dst, src,
                                  WIDTH * sizeof(TC),
                                  HB_MC_MEMCPY_TO_HOST);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to copy memory from device.\n");
                return rc;
        }

        // Do a final merge of the tile sorted array
        int N_TILES = tg_dim.x * tg_dim.y * grid_dim.x * grid_dim.y;
        int N_ELMS = WIDTH / N_TILES;
        final_merge(C, N_TILES, 1, N_ELMS);

        // Compare the known-correct vector (gold) and the result vector (C)
        float max = 0.1;
        double sse = vector_sse(gold, C, WIDTH);
        //int n = memcmp(gold, C, WIDTH * sizeof(TC));

        if (sse > max) {
                bsg_pr_test_err(BSG_RED("Vector Mismatch."));
                return HB_MC_FAIL;
        }
        bsg_pr_test_info(BSG_GREEN("Vector Match.\n"));
        return rc;
}

// Run a series of parallel sort tests on the Manycore device
int kernel_parallel_sort (int argc, char **argv) {
        int rc;
        char *bin_path, *test_name;
        struct arguments_path args = {NULL, NULL};

        argp_parse (&argp_path, argc, argv, 0, 0, &args);
        bin_path = args.path;
        test_name = args.name;

        bsg_pr_test_info("Running CUDA Parallel Sort.\n");

        // Define tg_dim_x/y: number of tiles in each tile group
        // Define block_size_x/y: amount of work each tile group should do
        // Calculate grid_dim_x/y: number of tile groups you want to launch as:
        //   Entire work (WIDTH) / (grid_dim_x/y)

        hb_mc_dimension_t tg_dim = { .x = 0, .y = 0 };
        hb_mc_dimension_t grid_dim = { .x = 0, .y = 0 };
        hb_mc_dimension_t block_size = { .x = 0, .y = 0 };
        if (!strcmp("v0", test_name)){
                tg_dim = { .x = 1, .y = 1 };
                grid_dim = { .x = 1, .y = 1};
        } else if (!strcmp("v1", test_name)){
                tg_dim = { .x = 4, .y = 1 };
                grid_dim = {.x = 1, .y = 1};
        } else if (!strcmp("v2", test_name)){
                tg_dim = { .x = 4, .y = 4 };
                grid_dim = {.x = 1, .y = 1};
        } else if (!strcmp("v3", test_name)){
                tg_dim = { .x = 2, .y = 2 };
                grid_dim = {.x = 1, .y = 1};
        } else {
                bsg_pr_test_err("Invalid version provided!.\n");
                return HB_MC_INVALID;
        }



        // Initialize the random number generators
        std::numeric_limits<int8_t> lim; // Used to get INT_MIN and INT_MAX in C++
        std::default_random_engine generator;
        generator.seed(42);
        std::uniform_real_distribution<float> distribution(lim.min(),lim.max());

        // Allocate A, C and R (result) on the host for each datatype.
        int32_t A_32[WIDTH];
        int32_t C_32[WIDTH];
        int32_t R_32[WIDTH];

        int16_t A_16[WIDTH];
        int16_t C_16[WIDTH];
        int16_t R_16[WIDTH];

        int8_t A_8[WIDTH];
        int8_t C_8[WIDTH];
        int8_t R_8[WIDTH];

        float A_f[WIDTH];
        float C_f[WIDTH];
        float R_f[WIDTH];

        
        // Generate random numbers. Since the Manycore can't handle infinities,
        // subnormal numbers, or NANs, filter those out.
        auto res = distribution(generator);

        for (uint64_t i = 0; i < WIDTH; i++) {
                do{
                        res = distribution(generator);
                }while(!std::isnormal(res) ||
                       !std::isfinite(res) ||
                       std::isnan(res));

                A_32[i] = static_cast<int32_t>(res);
                A_16[i] = static_cast<int16_t>(res);
                A_8[i] = static_cast<int8_t>(res);
                A_f[i] = static_cast<float>(res);
        }


        // Generate the known-correct results on the host
        parallel_sort(A_32, R_32, WIDTH);
        parallel_sort(A_16, R_16, WIDTH);
        parallel_sort(A_8, R_8, WIDTH);
        parallel_sort(A_f, R_f, WIDTH);


        // Initialize device, load binary and unfreeze tiles.
        hb_mc_device_t device;
        rc = hb_mc_device_init(&device, test_name, 0);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to initialize device.\n");
                return rc;
        }


        // Initialize the device with a kernel file
        rc = hb_mc_device_program_init(&device, bin_path, "default_allocator", 0);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to initialize program.\n");
                return rc;
        }

        // Allocate memory on the device for A, B and C. Since sizeof(float) ==
        // sizeof(int32_t) > sizeof(int16_t) > sizeof(int8_t) we'll reuse the
        // same buffers for each test

        eva_t A_device, B_device, C_device;

        // Allocate A on the device
        rc = hb_mc_device_malloc(&device,
                                 WIDTH * sizeof(uint32_t),
                                 &A_device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to allocate memory on device.\n");
                return rc;
        }

        // Allocate B on the device
        rc = hb_mc_device_malloc(&device,
                                 WIDTH * sizeof(uint32_t),
                                 &B_device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to allocate memory on device.\n");
                return rc;
        }

        // Allocate C on the device
        rc = hb_mc_device_malloc(&device,
                                 WIDTH * sizeof(uint32_t),
                                 &C_device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to allocate memory on device.\n");
                return rc;
        }


        // Run the 32-bit integer test and check the result
        rc = run_test(device, "kernel_parallel_sort_int",
                      A_32, C_32, R_32,
                      A_device, B_device, C_device,
                      tg_dim, grid_dim, block_size, 1);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("int32_t test failed\n");
                return rc;
        }
        bsg_pr_test_info("int32_t test passed!\n");

        // Run the 16-bit integer test and check the result
        rc = run_test(device, "kernel_parallel_sort_int16",
                      A_16, C_16, R_16,
                      A_device, B_device, C_device,
                      tg_dim, grid_dim, block_size, 2);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("int16_t test failed\n");
                return rc;
        }
        bsg_pr_test_info("int16_t test passed!\n");
/*
        // Run the 8-bit integer test and check the result
        rc = run_test(device, "kernel_parallel_sort_int8",
                      A_8, C_8, R_8,
                      A_device, B_device, C_device,
                      tg_dim, grid_dim, block_size, 3);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("int8_t test failed\n");
                return rc;
        }
        bsg_pr_test_info("int8_t test passed!\n");

        // Run the 32-bit floating-point test and check the result
        rc = run_test(device, "kernel_parallel_sort_float",
                      A_f, C_f, R_f,
                      A_device, B_device, C_device,
                      tg_dim, grid_dim, block_size, 4);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("float test failed\n");
                return rc;
        }
        bsg_pr_test_info("float test passed!\n");
*/
        // Freeze the tiles and memory manager cleanup.
        rc = hb_mc_device_finish(&device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to de-initialize device.\n");
                return rc;
        }

        return HB_MC_SUCCESS;
}

#ifdef COSIM
void cosim_main(uint32_t *exit_code, char * args) {
        // We aren't passed command line arguments directly so we parse them
        // from *args. args is a string from VCS - to pass a string of arguments
        // to args, pass c_args to VCS as follows: +c_args="<space separated
        // list of args>"
        int argc = get_argc(args);
        char *argv[argc];
        get_argv(args, argc, argv);

        svScope scope;
        scope = svGetScopeFromName("tb");
        svSetScope(scope);

        int rc = kernel_parallel_sort(argc, argv);
        *exit_code = rc;
        bsg_pr_test_pass_fail(rc == HB_MC_SUCCESS);
        return;
}
#else
int main(int argc, char ** argv) {
        int rc = kernel_parallel_sort(argc, argv);
        bsg_pr_test_pass_fail(rc == HB_MC_SUCCESS);
        return rc;
}
#endif


