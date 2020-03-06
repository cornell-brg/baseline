# Vector-vector add accelerator

## Actiavet global HB environment:
  
```
  % source setup-hb.sh
```

## Get the repos:
  
```
  % git clone git@github.com:cornell-brg/bsg_bladerunner.git
  % git checkout pp482-xcel-integrate
  % cd bsg_bladerunner
  % HB_TOP=$PWD
  % git clone git@github.com:cornell-brg/baseline.git
  % git checkout pp482-xcel-integration
```

## Initialize the submodules:
  
```
  % cd bsg_bladerunner
  % git submodule update --init bsg_manycore bsg_replicant basejump_stl
```
  Note that we don't use `make setup` because we want to reuse the globally
  installed HB RISCV tools and AMI build.
  
## Run the accelerator co-simulation:
  
```
  % cd $HB_TOP/baseline/examples/vector_add_xcel/
  % make vector_add_xcel.cosim.log BSG_MACHINE_PATH=$HB_TOP/bsg_replicant/machines/4x4_vvadd_xcel_blocking_vcache_f1_model
```
  The very first build takes ~5 minutes to compile all Verilog modules and
  may fail due to a known bug. Running the command again should solve that.
