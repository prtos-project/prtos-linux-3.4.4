# prtos-linux-3.4.4

Introduction
------------
prtos-linux-3.4.4 is para-virtualized linux kernel target for prtos hypervisor vitural machine(.i.e: partition), it requires the prtos hypervisor package to be installed firstly.
Here assues prtos hypervisor is installed in directory `/home/prtos/prtos-sdk`.

Build Instructions
------------------
```
git clone git@github.com:prtos-project/prtos-linux-3.4.4.git
cd prtos-linux-3.4.4.git
make PRTOS_PATH=/home/prtos/prtos-sdk/prtos  ARCH=x86  prtos_vmware_defconfig
make PRTOS_PATH=/home/prtos/prtos-sdk/prtos  ARCH=x86  distro-kernel
make PRTOS_PATH=/home/prtos/prtos-sdk/prtos  ARCH=x86  distro-tar #Generating LINUX distribution "linux-3.4.4-prtos.tar.bz2"
make PRTOS_PATH=/home/prtos/prtos-sdk/prtos  ARCH=x86  distro-run #self extracting binary distribution "linux-3.4.4-prtos.run"

```
**NOTE**: you can install prtos linux-3.4.4 sdk by untar linux-3.4.4-prtos.tar.bz2 and install it, or install linux-3.4.4-prtos.run directly.



