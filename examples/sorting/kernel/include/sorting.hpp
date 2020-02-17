#ifndef __SORTING_HPP
#define __SORTING_HPP
#include <cstdint>


template <typename T>
void __attribute__ ((noinline)) kernel_merge (T *A, uint64_t begin, uint64_t mid, uint64_t end) {
    uint64_t begin2 = mid + 1;
    if (A[mid] <= A[begin2]) {return;}

    while (begin <= mid && begin2 <= end) {
        if (A[begin] <= A[begin2]) {
            begin++;
        } else {
            T value = A[begin2];
            uint64_t idx = begin2; 

            while (idx != begin) {
                A[idx] = A[idx - 1];
                idx --;
            }
            A[begin] = value;
            begin ++;
            mid ++;
            begin2 ++;
        }
    }
}

template <typename T>
void __attribute__ ((noinline)) kernel_sort( T *A, uint64_t begin, uint64_t end ) {
    end = end - 1;
    if (begin >= end) {return;}
    uint64_t mid = (begin + end - 1) / 2;
    kernel_sort(A, begin, mid);
    kernel_sort(A, mid + 1, end);

    kernel_merge(A, begin, mid, end);

}


/*
 * This is the most basic single tile version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of a single 1x1 tile group
 * Do NOT use this version with larger tile groups 
 */
template <typename T>
int __attribute__ ((noinline)) kernel_sort_single_tile(T *A, uint32_t WIDTH) {
    // A single tile performs the entire vector addition
	for (int iter_x = 0; iter_x < WIDTH; iter_x += 1) { 
        kernel_sort(A, 0, WIDTH);
	}

	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

        return 0;
}


/*
 * This is the single 1 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of 1-dimensional tile group
 */
// template <typename TA, typename TB, typename TC>
// int __attribute__ ((noinline)) kernel_vector_sort_1D_tile_group(TA *A, int32_t WIDTH) {

//         // Vector is divided among tiles in the tile group
//         // As tile group is one dimensional, each tile performs
//         // (WIDTH / bsg_tiles_X) additions
// 	for (int iter_x = __bsg_x; iter_x < WIDTH; iter_x += bsg_tiles_X) { 
//                 C[iter_x] = A[iter_x] + B[iter_x];
// 	}

// 	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

//         return 0;
// }


/*
 * This is the single 2 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of tile group
 */
// template <typename TA, typename TB, typename TC>
// int __attribute__ ((noinline)) kernel_vector_sort_2D_tile_group(TA *A, uint32_t WIDTH) {

//         // Vector is divided among tiles in the tile group
//         // As the tile group is two diemsnional, each tile performs
//         // (WIDTH / (bsg_tiles_X * bsg_tiles_Y)) additions
// 	for (int iter_x = __bsg_id; iter_x < WIDTH; iter_x += bsg_tiles_X * bsg_tiles_Y) { 
//                 C[iter_x] = A[iter_x] + B[iter_x];
// 	}

// 	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

//         return 0;
// }


// /*
//  * This is the 1 dimensional grid of 2 dimensional tile groups version of vector addition
//  * that adds two vectors A and B and stores the result in C.
//  *
//  * The code assumes a 1-dimensional grid of 2-dimensional tile groups are called.
//  * Due to the nature of the compuation (1 dimensional vector addition), there
//  * is no need to launch a 2-dimensional grid. Look at matrix multiplication examples
//  * for a sample of launching 2-dimensional grids.
//  */
// template <typename TA, typename TB, typename TC>
// int __attribute__ ((noinline)) kernel_vector_add_1D_grid_2D_tile_groups(TA *A, TB *B, TC *C,
//                       uint32_t WIDTH, uint32_t block_size_x) {

//         // Each tile group is responsbile for calculating block_size_x elements
//         // As there are multiple tile groups that shared the work,
//         // first we calculate the porition of each specific tile group
//         uint32_t start = __bsg_tile_group_id_x * block_size_x;
//         uint32_t end = start + block_size_x;
        

//         // A tile group's share (block_size_x) is divided among tiles in the tile group
//         // As the tile group is two diemsnional, each tile performs
//         // (block_size_x / (bsg_tiles_X * bsg_tiles_Y)) additions
// 	for (int iter_x = start + __bsg_id; iter_x < end; iter_x += bsg_tiles_X * bsg_tiles_Y) { 
//                 C[iter_x] = A[iter_x] + B[iter_x];
// 	}

// 	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

//         return 0;
// }




#endif
