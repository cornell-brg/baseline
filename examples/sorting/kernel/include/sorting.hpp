#ifndef __SORTING_HPP
#define __SORTING_HPP
#include <cstdint>


template <typename T>
void __attribute__ ((noinline)) kernel_merge 
    ( T* dst, T* src0, int begin0, int end0,
      T* src1, int begin1, int end1 )
{
  int size = ( end0 - begin0 ) + ( end1 - begin1 );
  int idx0 = begin0;
  int idx1 = begin1;

  for ( int idx = begin0; idx < begin0 + size; idx++ ) {
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
void __attribute__ ((noinline)) kernel_sort( T *A, T *B, int begin, int end ) {
    if (end - begin == 1) {
        return;
    }
    int mid = (begin + end) / 2;
    kernel_sort(A, B, begin, mid);
    kernel_sort(A, B, mid, end);

  // Out-of-place merge
    kernel_merge( B, A, begin, mid, A, mid, end );

  // Transfer elements back to the original array
  int j = begin;
  for ( int i = begin; i < end; i++ ) {
    A[i] = B[j];
    j += 1;
  }
}


/*
 * This is the most basic single tile version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of a single 1x1 tile group
 * Do NOT use this version with larger tile groups 
 */
template <typename T>
int __attribute__ ((noinline)) kernel_sort_single_tile(T *A, T *B, uint32_t WIDTH) {
    // A single tile performs the entire vector addition
        kernel_sort(A, B, 0, WIDTH);

	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

        return 0;
}


/*
 * This is the single 1 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of 1-dimensional tile group
 */
template <typename T>
int __attribute__ ((noinline)) kernel_vector_sort_1D_tile_group(T *A, T *B, int32_t WIDTH) {

        // Vector is divided among tiles in the tile group
        // As tile group is one dimensional, each tile performs
        // (WIDTH / bsg_tiles_X) additions
  int size = WIDTH / bsg_tiles_X;
  for (int iter_x = 0; iter_x < bsg_tiles_X; iter_x += 1) { 
                kernel_sort(A, B, iter_x * size, (iter_x + 1) * size);
  }

	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

        return 0;
}


/*
 * This is the single 2 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of tile group
 */
template <typename T>
int __attribute__ ((noinline)) kernel_vector_sort_2D_tile_group(T *A, T *B, uint32_t WIDTH) {

        // Vector is divided among tiles in the tile group
        // As the tile group is two diemsnional, each tile performs
        // (WIDTH / (bsg_tiles_X * bsg_tiles_Y)) additions
    int size = WIDTH / (bsg_tiles_X * bsg_tiles_Y);
	for (int iter_x = 0; iter_x < bsg_tiles_X * bsg_tiles_Y; iter_x += 1) { 
                kernel_sort(A, B, iter_x * size, (iter_x + 1) * size);
	}

	bsg_tile_group_barrier(&r_barrier, &c_barrier); 

        return 0;
}


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
