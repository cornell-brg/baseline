# Vector-vector add accelerator

## Actiavet global environment:
  
```
  % source setup-hb.sh
```

## Get the repos:
  
```
  % git clone git@github.com:cornell-brg/bsg_bladerunner.git
  % git checkout pp482-xcel-integrate
  % git clone git@github.com:cornell-brg/baseline.git
  % git checkout pp482-xcel-integration
```

## Initialize the submodules:
  
```
  % cd bsg_bladerunner
  % make setup
```
  This setup will fetch and build the RISCV tool chain. I cannot use the
  globally installed tool chain because that's tightly coupled with the
  manycore repo, and the accelerator source code also lives there.

## Run the accelerator co-simulation:
  
```
  % cd bsg_bladerunner/baseline/examples/vector_add_xcel/
  % make vector_add_xcel.cosim.log BSG_MACHINE_PATH=/<path>/bsg_bladerunner/bsg_replicant/machines/4x4_vvadd_xcel_blocking_vcache_f1_model
```
  
  You need to change the above path to the path of your
  `4x4_vvadd_xcel_blocking_vcache_f1_model`
