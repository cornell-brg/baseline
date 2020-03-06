/*
 * This kernel performs vector addition. 
 * 
 * This is the most basic version of single-tile Vector-Vector Addition.
 * This version assumes only a single 1x1 tile group is called.
 */

// BSG_TILE_GROUP_X_DIM and BSG_TILE_GROUP_Y_DIM must be defined
// before bsg_manycore.h and bsg_tile_group_barrier.h are
// included. bsg_tiles_X and bsg_tiles_Y must also be defined for
// legacy reasons, but they are deprecated.
#define BSG_TILE_GROUP_X_DIM 1
#define BSG_TILE_GROUP_Y_DIM 1
#define bsg_tiles_X BSG_TILE_GROUP_X_DIM
#define bsg_tiles_Y BSG_TILE_GROUP_Y_DIM
#include <bsg_manycore.h>
#include <bsg_tile_group_barrier.h>
INIT_TILE_GROUP_BARRIER(r_barrier, c_barrier, 0, bsg_tiles_X-1, 0, bsg_tiles_Y-1);

#include <vector_add_xcel.hpp>
#include <cstring>

#define MAX_ARRAY_SIZE 64
#define XCEL_X_CORD 0
#define XCEL_Y_CORD (bsg_global_Y-1)

extern int bsg_printf(const char*, ...);

int scratchpad_A[MAX_ARRAY_SIZE];
int scratchpad_B[MAX_ARRAY_SIZE];

// Xcel will write this variable to indicate it has finished
int done;

enum {
  CSR_CMD_IDX = 0,
  CSR_DRAM_ENABLE_IDX,
  CSR_NELEM_IDX,
  CSR_A_ADDR_HI_IDX,
  CSR_A_ADDR_LO_IDX,
  CSR_B_ADDR_HI_IDX,
  CSR_B_ADDR_LO_IDX,
  CSR_SIG_ADDR_HI_IDX,
  CSR_SIG_ADDR_LO_IDX,
  CSR_DST_ADDR_IDX,
} CSR_IDX_e;

typedef union{
  struct {
    unsigned int  epa32;     // LSB
    unsigned char x8;
    unsigned char y8;
    unsigned char chip8;
    unsigned char reserved8; // MSB
  } NPA_s;
  struct {
    unsigned int LO;
    unsigned int HI;
  } HL_s;
} Norm_NPA_s;

bsg_remote_int_ptr xcel_CSR_base_ptr;
Norm_NPA_s addr_A, addr_B, addr_signal;

/* We wrap all external-facing C++ kernels with `extern "C"` to
 * prevent name mangling 
 */
extern "C" {

  int __attribute__ ((noinline)) xcel_configure(
      int N, Norm_NPA_s *A, Norm_NPA_s *B, Norm_NPA_s *signal) {
    // Number of elements
    *(xcel_CSR_base_ptr + CSR_NELEM_IDX) = N;

    // A address
    *(xcel_CSR_base_ptr + CSR_A_ADDR_HI_IDX) = A->HL_s.HI;
    *(xcel_CSR_base_ptr + CSR_A_ADDR_LO_IDX) = A->HL_s.LO;

    // B address
    *(xcel_CSR_base_ptr + CSR_B_ADDR_HI_IDX) = B->HL_s.HI;
    *(xcel_CSR_base_ptr + CSR_B_ADDR_LO_IDX) = B->HL_s.LO;

    // Signaling address
    *(xcel_CSR_base_ptr + CSR_SIG_ADDR_HI_IDX) = signal->HL_s.HI;
    *(xcel_CSR_base_ptr + CSR_SIG_ADDR_LO_IDX) = signal->HL_s.LO;

    // Xcel local memory destination
    *(xcel_CSR_base_ptr + CSR_DST_ADDR_IDX) = (int) &done;
    return 0;
  }

  int __attribute__ ((noinline)) kernel_vec_add_xcel(int *A, int *B, int *C, int N) {
    int i;
    volatile int *xcel_res_ptr;

    /* bsg_set_tile_x_y(); */

    xcel_CSR_base_ptr = bsg_global_ptr(XCEL_X_CORD, XCEL_Y_CORD, 0);

    if((__bsg_x == 0) && (__bsg_y == 0) && (N <= MAX_ARRAY_SIZE)) {
      /* bsg_printf("[INFO] vvadd-xcel starts!\n"); */
      // Copy data from DRAM into scratchpad
      for(i = 0; i < N; i++) {
        scratchpad_A[i] = A[i];
        scratchpad_B[i] = B[i];
      }

      // Setup the configs
      addr_A.NPA_s.epa32 = (unsigned int) &scratchpad_A;
      addr_A.NPA_s.x8    = (char)__bsg_grp_org_x;
      addr_A.NPA_s.y8    = (char)__bsg_grp_org_y;
      addr_A.NPA_s.chip8 = (char)0;
      addr_A.NPA_s.reserved8 = (char)0;

      addr_B.NPA_s.epa32 = (unsigned int) &scratchpad_B;
      addr_B.NPA_s.x8    = (char)__bsg_grp_org_x;
      addr_B.NPA_s.y8    = (char)__bsg_grp_org_y;
      addr_B.NPA_s.chip8 = (char)0;
      addr_B.NPA_s.reserved8 = (char)0;

      addr_signal.NPA_s.epa32 = (unsigned int) &done;
      addr_signal.NPA_s.x8    = (char)(__bsg_x + __bsg_grp_org_x);
      addr_signal.NPA_s.y8    = (char)(__bsg_y + __bsg_grp_org_y);
      addr_signal.NPA_s.chip8 = (char)0;
      addr_signal.NPA_s.reserved8 = (char)0;

      // Configure the xcel CSRs
      xcel_configure(N, &addr_A, &addr_B, &addr_signal);

      // Xcel go
      done = 0;
      *(xcel_CSR_base_ptr + CSR_CMD_IDX) = 1;

      // Wait till xcel is done
      bsg_wait_local_int(&done, 1);

      xcel_res_ptr = (volatile int*) bsg_global_ptr(XCEL_X_CORD, XCEL_Y_CORD, &done);

      // Copy result from scratchpad into DRAM
      for(i = 0; i < N; i++)
        C[i] = *(xcel_res_ptr + i);
    }
    return 0;
  }

}
