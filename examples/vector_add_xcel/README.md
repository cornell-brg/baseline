# Vector-vector add accelerator

To run the test, first make sure you are at the following commits:
  bsg_bladerunner: 155547c548e3b01740eb2100f7741871a0aa44a9
    ^ this commit should set the head of the submodules to
    bsg_manycore: 1891d2d1bb66ec334755ea563d2f25c2a2c8b0d7
    bsg_replicant: b7ba56ef3691c41b40416772571204e5eea191da

On brg-vip: 
  % source setup-hb.sh
  % cd bsg_bladerunner/baseline/examples/vector_add_xcel/
  % make vector_add_xcel.cosim.log BSG_MACHINE_PATH=/work/global/pp482/hammerblade/bsg_bladerunner/bsg_replicant/machines/4x4_vvadd_xcel_blocking_vcache_f1_model
  You can change the above path to the path of your `4x4_vvadd_xcel_blocking_vcache_f1_model`
