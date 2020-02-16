#ifndef __PARALLEL_SORT_HPP
#define __PARALLEL_SORT_HPP
#include <cstdint>

/*
 * This is the most basic single tile version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of a single 1x1 tile group
 * Do NOT use this version with larger tile groups 
 */
//template <typename TA, typename TB, typename TC>
//int __attribute__ ((noinline)) kernel_vector_add_single_tile(TA *A, TB *B, TC *C,
//                      uint32_t WIDTH) {
//        // A single tile performs the entire vector addition
//	for (int iter_x = 0; iter_x < WIDTH; iter_x += 1) { 
//                C[iter_x] = A[iter_x] + B[iter_x];
//	}
//
//	bsg_tile_group_barrier(&r_barrier, &c_barrier); 
//
//        return 0;
//}


/*
 * This is the single 1 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of 1-dimensional tile group
 */
//template <typename TA, typename TB, typename TC>
//int __attribute__ ((noinline)) kernel_vector_add_single_1D_tile_group(TA *A, TB *B, TC *C,
//                      uint32_t WIDTH) {
//
//        // Vector is divided among tiles in the tile group
//        // As tile group is one dimensional, each tile performs
//        // (WIDTH / bsg_tiles_X) additions
//	for (int iter_x = __bsg_x; iter_x < WIDTH; iter_x += bsg_tiles_X) { 
//                C[iter_x] = A[iter_x] + B[iter_x];
//	}
//
//	bsg_tile_group_barrier(&r_barrier, &c_barrier); 
//
//        return 0;
//}

// Helper function to actually merge
template <typename TA, typename TC>
void merge(const TA *A, TC *C, int left, int middle, int right) {
    int i = 0;
    int j = 0;
    int k = 0;
    int left_length = middle - left + 1;
    int right_length = right - middle;
    TA left_array[left_length];
    TA right_array[right_length];
    
    /* copy values to left array */
    for (int i = 0; i < left_length; i++) {
        left_array[i] = A[left + i];
    }
    
    /* copy values to right array */
    for (int j = 0; j < right_length; j++) {
        right_array[j] = A[middle + 1 + j];
    }
    
    i = 0;
    j = 0;
    /** chose from right and left arrays and copy */
    while (i < left_length && j < right_length) {
        if (left_array[i] <= right_array[j]) {
            C[left + k] = left_array[i];
            i++;
        } else {
            C[left + k] = right_array[j];
            j++;
        }
        k++;
    }
    
    /* copy the remaining values to the array */
    while (i < left_length) {
        C[left + k] = left_array[i];
        k++;
        i++;
    }
    while (j < right_length) {
        C[left + k] = right_array[j];
        k++;
        j++;
    }
}

// Helper function to perform merge sort
template <typename TA, typename TC>
void merge_sort(const TA *A, TC *C, int left, int right) {
    if (left < right) {
        int middle = left + (right - left) / 2;
        merge_sort(A, C, left, middle);
        merge_sort(A, C, middle + 1, right);
        merge(A, C, left, middle, right);
    }
}


/*
 * This is the single 2 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of tile group
 */
template <typename TA, typename TC>
int __attribute__ ((noinline)) kernel_parallel_sort_single_tile_group(TA *A, TC *C,
                      uint32_t WIDTH) {

        // Vector is divided among tiles in the tile group
        // As the tile group is two diemsnional, each tile sorts
        // (WIDTH / (bsg_tiles_X * bsg_tiles_Y)) elements
        const int N_TILES = bsg_tiles_X * bsg_tiles_Y;
        const int N_ELMS = WIDTH / N_TILES;
        const int OFFSET = WIDTH % N_TILES;
        int left = __bsg_id * N_ELMS;
        int right = (__bsg_id + 1) * N_ELMS - 1;

        // if number of elements in A is not a multiple of number of tiles
        // make the last tile sort the remaining elements as well
        if (__bsg_id ==  N_TILES - 1) {
            right += OFFSET;
        }
        int middle = left + (right - left) / 2;
        if (left < right) {
            merge_sort(A, C, left, right);
            merge_sort(A, C, left + 1, right);
            merge(A, C, left, middle, right);
        }

	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

        return 0;
}


/*
 * This is the 1 dimensional grid of 2 dimensional tile groups version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a 1-dimensional grid of 2-dimensional tile groups are called.
 * Due to the nature of the compuation (1 dimensional vector addition), there
 * is no need to launch a 2-dimensional grid. Look at matrix multiplication examples
 * for a sample of launching 2-dimensional grids.
 */
//template <typename TA, typename TB, typename TC>
//int __attribute__ ((noinline)) kernel_vector_add_1D_grid_2D_tile_groups(TA *A, TB *B, TC *C,
//                      uint32_t WIDTH, uint32_t block_size_x) {
//
//        // Each tile group is responsbile for calculating block_size_x elements
//        // As there are multiple tile groups that shared the work,
//        // first we calculate the porition of each specific tile group
//        uint32_t start = __bsg_tile_group_id_x * block_size_x;
//        uint32_t end = start + block_size_x;
//        
//
//        // A tile group's share (block_size_x) is divided among tiles in the tile group
//        // As the tile group is two diemsnional, each tile performs
//        // (block_size_x / (bsg_tiles_X * bsg_tiles_Y)) additions
//	for (int iter_x = start + __bsg_id; iter_x < end; iter_x += bsg_tiles_X * bsg_tiles_Y) { 
//                C[iter_x] = A[iter_x] + B[iter_x];
//	}
//
//	bsg_tile_group_barrier(&r_barrier, &c_barrier); 
//
//        return 0;
//}




#endif
