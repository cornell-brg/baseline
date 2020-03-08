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

// Vector-Vector Addition (A + B = C).

// A,B, and C's sizes are WIDTH.
// Each time the kernel is called, it is run for (NUM_ITER + 1)
// iterations and the first iteration is discarded.
// 
// NOTE: 3 * WIDTH <= 4KB, the size of DMEM on the tile.

#include "sorting.hpp"

// Matrix sizes:
#define WIDTH  64
#define NUM_ITER 1

template <typename T>
void merge( T* dst,
            T* src0, size_t begin0, size_t end0,
            T* src1, size_t begin1, size_t end1 )
{
  size_t size = ( end0 - begin0 ) + ( end1 - begin1 );
  size_t idx0 = begin0;
  size_t idx1 = begin1;

  for ( size_t idx = begin0; idx < begin0 + size; idx++ ) {
    // Done with array src0
    if ( idx0 == end0 ) {
      dst[idx] = src1[idx1];
      idx1 += 1;
    }
    // Done with array src1
    else if ( idx1 == end1 ) {
      dst[idx] = src0[idx0];
      idx0 += 1;
    }
    else if ( src0[idx0] < src1[idx1] ) {
      dst[idx] = src0[idx0];
      idx0 += 1;
    }
    else {
      dst[idx] = src1[idx1];
      idx1 += 1;
    }
  }
}

template <typename T>
void sort( T *A, T *B, int begin, int end ) {
    if (end - begin == 1) {
        return;
    }
    int mid = (begin + end) / 2;
    sort(A, B, begin, mid);
    sort(A, B, mid, end);

  // Out-of-place sort
    merge( B, A, begin, mid, A, mid, end );

  // Transfer elements back to the original array
  for ( size_t i = begin; i < end; i++ ) {
    A[i] = B[i];
  }
}

template <typename T> 
void merge_sections(T *A, T *B, int begin, int end, int size) {
    int mid = (begin + end) / 2;
    if ( (end - begin) == size ) {
        return;
    }
    if ( (end - begin) != 2 * size) {
        merge_sections(A, B, begin, mid, size);
        merge_sections(A, B, mid, end, size);
    }
    merge( B, A, begin, mid, A, mid, end );
    for (size_t i = begin; i < end; i++) {
        A[i] = B[i];
    }
}


// Compute the sum of squared error between vectors A and B (M x N)
template <typename T>
double sort_sse (const T *A, const T *B, uint64_t N) {
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


// Run a Vector Addition test on the Manycore, and compare the result.  A
// is the array to be sorted, and gold is the
// known-good result computed by the host.
template<typename T>
int run_test(hb_mc_device_t &device, const char* kernel,
             T *A, T *B, const T *gold,
             const eva_t &A_device, const eva_t &B_device,
             const hb_mc_dimension_t &tg_dim,
             const hb_mc_dimension_t &grid_dim,
             const hb_mc_dimension_t block_size,
             const unsigned int tag){
        int rc;

        for (int i = 0; i < WIDTH; i++) {
            printf("%d  ", A[i]);
        }
        printf("\n");

        // Copy A from host onto device DRAM.
        void *dst = (void *) ((intptr_t) A_device);
        void *src = (void *) &A[0];
        rc = hb_mc_device_memcpy (&device, dst, src,
                                  (WIDTH) * sizeof(T),
                                  HB_MC_MEMCPY_TO_DEVICE);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to copy memory to device.\n");
                return rc;
        }

        // Copy B from host onto device DRAM.
        dst = (void *) ((intptr_t) B_device);
        src = (void *) &B[0];
        rc = hb_mc_device_memcpy (&device, dst, src,
                                  (WIDTH) * sizeof(T),
                                  HB_MC_MEMCPY_TO_DEVICE);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to copy memory to device.\n");
                return rc;
        }


        // Prepare list of input arguments for kernel. See the kernel source
        // file for the argument uses.
        uint32_t cuda_argv[7] = {A_device, B_device,
                                  WIDTH, block_size.y, block_size.x,
                                  tag, NUM_ITER};

        // Enquque grid of tile groups, pass in grid and tile group dimensions,
        // kernel name, number and list of input arguments
        rc = hb_mc_kernel_enqueue (&device, grid_dim, tg_dim,
                                   kernel, 7, cuda_argv);
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
        src = (void *) ((intptr_t) A_device);
        dst = (void *) &A[0];
        rc = hb_mc_device_memcpy (&device, (void *) dst, src,
                                  WIDTH * sizeof(T),
                                  HB_MC_MEMCPY_TO_HOST);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to copy memory from device.\n");
                return rc;
        }

        int size = WIDTH / (tg_dim.x * tg_dim.y);
        merge_sections(A, B, 0, WIDTH, size); 

        // Compare the known-correct vector (gold) and the result vector (C)
        float max = 0.1;
        double sse = sort_sse(gold, A, WIDTH);

        if (sse > max) {
                bsg_pr_test_err(BSG_RED("Vector Mismatch. SSE: %f\n"), sse);
                return HB_MC_FAIL;
        }
        bsg_pr_test_info(BSG_GREEN("Vector Match.\n"));
}

// Run a series of sorting tests on the Manycore device
int kernel_sort (int argc, char **argv) {
        int rc;
        char *bin_path, *test_name;
        struct arguments_path args = {NULL, NULL};

        argp_parse (&argp_path, argc, argv, 0, 0, &args);
        bin_path = args.path;
        test_name = args.name;

        bsg_pr_test_info("Running CUDA Sorting.\n");

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
                block_size = {.x = 4, .y = 1};
                grid_dim = {.x = WIDTH / block_size.x, .y = 1};
        } else {
                bsg_pr_test_err("Invalid version provided!.\n");
                return HB_MC_INVALID;
        }




        // Initialize the random number generators
        std::numeric_limits<int8_t> lim; // Used to get INT_MIN and INT_MAX in C++
        std::default_random_engine generator;
        generator.seed(42);
        std::uniform_real_distribution<float> distribution(lim.min(),lim.max());

        // Allocate A, B, C and R (result) on the host for each datatype.
        int32_t A_32[WIDTH];
        int32_t B_32[WIDTH];
        int32_t R_32[WIDTH];

        int16_t A_16[WIDTH];
        int16_t B_16[WIDTH];
        int16_t R_16[WIDTH];

        int8_t A_8[WIDTH];
        int8_t B_8[WIDTH];
        int8_t R_8[WIDTH];

        float A_f[WIDTH];
        float B_f[WIDTH];
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

        for (uint64_t i = 0; i < WIDTH; i++) {
            R_32[i] = A_32[i];
            R_16[i] = A_16[i];
            R_8[i]  = A_8[i];
            R_f[i]  = A_f[i];
            B_32[i] = 0;
            B_16[i] = 0;
            B_8[i]  = 0;
            B_f[i]  = 0; 
        }

        // Generate the known-correct results on the host
        sort(R_32, B_32, 0, WIDTH);
        sort(R_16, B_16, 0, WIDTH);
        sort(R_8,  B_8,  0, WIDTH);
        sort(R_f,  B_f,  0, WIDTH);


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

        eva_t A_device;

        // Allocate A on the device
        rc = hb_mc_device_malloc(&device,
                                 WIDTH * sizeof(uint32_t),
                                 &A_device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to allocate memory on device.\n");
                return rc;
        }

        eva_t B_device;

        // Allocate A on the device
        rc = hb_mc_device_malloc(&device,
                                 WIDTH * sizeof(uint32_t),
                                 &B_device);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("failed to allocate memory on device.\n");
                return rc;
        }


        // Run the 32-bit integer test and check the result
        rc = run_test(device, "kernel_sort_int",
                      A_32, B_32, R_32, A_device, B_device,
                      tg_dim, grid_dim, block_size, 1);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("int32_t test failed\n");
                return rc;
        }
        bsg_pr_test_info("int32_t test passed!\n");

        // Run the 16-bit integer test and check the result
        rc = run_test(device, "kernel_sort_int16",
                      A_16, B_16, R_16, A_device, B_device,
                      tg_dim, grid_dim, block_size, 2);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("int16_t test failed\n");
                return rc;
        }
        bsg_pr_test_info("int16_t test passed!\n");

        // Run the 8-bit integer test and check the result
        rc = run_test(device, "kernel_sort_int8",
                      A_8, B_8, R_8, A_device, B_device,
                      tg_dim, grid_dim, block_size, 3);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("int8_t test failed\n");
                return rc;
        }
        bsg_pr_test_info("int8_t test passed!\n");

        // Run the 32-bit floating-point test and check the result
        rc = run_test(device, "kernel_sort_float",
                      A_f, B_f, R_f, A_device, B_device,
                      tg_dim, grid_dim, block_size, 4);
        if (rc != HB_MC_SUCCESS) {
                bsg_pr_test_err("float test failed\n");
                return rc;
        }
        bsg_pr_test_info("float test passed!\n");

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

        int rc = kernel_sort(argc, argv);
        *exit_code = rc;
        bsg_pr_test_pass_fail(rc == HB_MC_SUCCESS);
        return;
}
#else
int main(int argc, char ** argv) {
        int rc = kernel_sort(argc, argv);
        bsg_pr_test_pass_fail(rc == HB_MC_SUCCESS);
        return rc;
}
#endif

