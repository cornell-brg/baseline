#ifndef __PARALLEL_SORT_HPP
#define __PARALLEL_SORT_HPP
#include <cstdint>

#define K 4

// out-of-place merge using the scratchpad (instead of the provided buffer)
// two subarrays: 
// left array = [left, middle]
// right array = (middle, right]
template <typename TA, typename TC>
void merge_on_scratchpad(const TA *A, TC *C, int left, int middle, int right) {
    int left_len = middle - left + 1;
    int right_len = right - middle;
    int left_arr[left_len]; // on scratchpad
    int right_arr[right_len]; // on scratchpad

    // copy values to left array
    for (int ii = 0; ii < left_len; ii++) {
        left_arr[ii] = A[left + ii];
    }
    
    // copy values to right array
    for (int jj = 0; jj < right_len; jj++) {
        right_arr[jj] = A[middle + 1 + jj];
    }
    
    int i = 0; // where in left array are we
    int j = 0; // where in right array are we
    int k = left; // where in final array are we
    // choose from right and left arrays and copy
    while (i < left_len && j < right_len) {
        if (left_arr[i] <= right_arr[j]) {
            C[k] = left_arr[i];
            i++;
        } else {
            C[k] = right_arr[j];
            j++;
        }
        k++;
    }
    
    // copy the remaining values to the array
    while (i < left_len) {
        C[k] = left_arr[i];
        k++;
        i++;
    }
    while (j < right_len) {
        C[k] = right_arr[j];
        k++;
        j++;
    }

}

// insertion sort of array [left, right]
template <typename TA, typename TC>
void insertion_sort(const TA *A, TC *C, int left, int right) {
    for (int i = left; i <= right; i++) {
        int j = i + 1;
        int temp = A[j];
        while (j > left && A[j-1] > temp) {
            C[j] = A[j-1];
            j--;
        }
    }
}

// out-of-place merge using the provided buffer @B
// two subarrays: 
// left array = [left, middle]
// right array = (middle, right]
template <typename TA, typename TB, typename TC>
void merge(const TA *A, TB *B, TC *C, int left, int middle, int right) {
    // copy values to left array
    for (int left_idx = left; left_idx <= middle; left_idx++) {
        B[left_idx] = A[left_idx];
    }
    
    // copy values to right array
    for (int right_idx = middle + 1; right_idx <= right; right_idx++) {
        B[right_idx] = A[right_idx];
    }
    
    int i = left; // where in left array are we
    int j = middle + 1; // where in right array are we
    int k = left; // where in final array are we
    // choose from right and left arrays and copy
    while (i <= middle && j <= right) {
        if (B[i] <= B[j]) {
            C[k] = B[i];
            i++;
        } else {
            C[k] = B[j];
            j++;
        }
        k++;
    }
    
    // copy the remaining values to the array
    while (i <= middle) {
        C[k] = B[i];
        k++;
        i++;
    }
    while (j <= right) {
        C[k] = B[j];
        k++;
        j++;
    }
}

// Helper function to perform merge sort
template <typename TA, typename TB, typename TC>
void merge_sort(const TA *A, TB *B, TC *C, int left, int right) {

// uncomment this for insertion sort
/*
    if (right - left <= K) {
        insertion_sort(A, C, left, right);
    } else {
        int middle = left + (right - left) / 2;
        merge_sort(A, B, C, left, middle);
        merge_sort(A, B, C, middle + 1, right);
        merge(A, B, C, left, middle, right);
    }
*/

// uncomment this for scratchpad sort
/*
    if (left < right) {
        int middle = left + (right - left) / 2;
        if (right - left <= K) {
             merge_sort(A, B, C, left, middle);
             merge_sort(A, B, C, middle + 1, right);
             merge_on_scratchpad(A, C, left, middle, right);
        } else {
             merge_sort(A, B, C, left, middle);
             merge_sort(A, B, C, middle + 1, right);
             merge(A, B, C, left, middle, right);
        }
    }
*/

// uncomment this for regular merge sort

    if (left < right) {
        int middle = left + (right - left) / 2;
        merge_sort(A, B, C, left, middle);
        merge_sort(A, B, C, middle + 1, right);
        merge(A, B, C, left, middle, right);
    }


}


/*
 * This is the single 2 dimensional tile group version of vector addition
 * that adds two vectors A and B and stores the result in C.
 *
 * The code assumes a sinlge 1x1 grid of tile group
 */
template <typename TA, typename TB, typename TC>
int __attribute__ ((noinline)) kernel_parallel_sort_single_tile_group(TA *A, TB *B, TC *C,
                      uint32_t WIDTH) {

        // Vector is divided among tiles in the tile group
        // As the tile group is two diemsnional, each tile sorts
        // (WIDTH / (bsg_tiles_X * bsg_tiles_Y)) elements
        const int N_TILES = bsg_tiles_X * bsg_tiles_Y;
        const int N_ELMS = WIDTH / N_TILES;
        const int OFFSET = WIDTH % N_TILES;
        int left = __bsg_id * N_ELMS;
        int right = (__bsg_id + 1) * N_ELMS - 1;

        //bsg_printf("\nIM TILE %d LEFT %d RIGHT %d\n", __bsg_id, left, right);

        // if number of elements in A is not a multiple of number of tiles
        // make the last tile sort the remaining elements as well
        if (__bsg_id ==  N_TILES - 1) {
            right += OFFSET;
        }

/*
        int middle = left + (right - left) / 2;
        if (left < right) {
            merge_sort(A, B, C, left, right);
            merge_sort(A, B, C, middle + 1, right);
            merge(A, B, C, left, middle, right);
        }
*/

        merge_sort(A, B, C, left, right);

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
