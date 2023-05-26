% Linux kernel Relocator

\pagebreak

# Introduction

The Linux kernel relocator is a tool that takes as an input the vmlinux image
ELF file and relocates it to a specific location. When the Linux kernel is
compiled, there are several configuration options that define how the kernel is
linked:

 * CONFIG\_PHYSICAL\_START. This is the address where the kernel is linked. By
default, this value is 0x1000000. 

 * CONFIG\_PAGE\_OFFSET. This directive sets the page offset of the kernel, i.e.,
the value which is added to the physical address to obtain the virtual address
where the kernel resides. It defaults to 0xC0000000. 

 * CONFIG\_RELOCATABLE. This configuration option is mandatory in order for the
application to run. With this option, the kernel generates all the relocation
information required on the ELF file. If not set, the relocator would not work

Considering the default values, the Linux kernel assumes it is loaded at
physical location 0x1000000 and, hence, the virtual address space starts at
0xC1000000. 

# Rationale

When running standalone or full virtualized, the linux kernel has full access to
the default physical address configured in CONFIG\_PHYSICAL\_START. The
problem comes when the system uses paravirtualization, and the kernel has to
share the physical memory addresses with other partitions. This may cause that
the default physical location was unavailable. In this case, the kernel should
be re-linked to work properly.

For example, lets assume that the kernel has to be loaded starting in the
physical location 0x4000000. The first solution would be to modify the
CONFIG\_PHYSICAL\_START directive. However, this may force to compile the kernel
again, which is something that may not be interesting in some cases.

Another solution would be to modify the CONFIG\_PAGE\_OFFSET. Indeed, this is
the best solution for reasons that would be explained later.

The solution we have applied is very similar to modifying CONFIG\_PHYSICAL\_START
only faster. 

## Relocator

The relocator is the solution we have applied. Once the kernel has already been
compiled, the relocator takes the ELF file and *relocates* each symbol to the
specified physical address (which is passed as an argument). This way, it is not
necessary to modify the configuration options and, thus, the kernel does not
need to be recompiled (the relocator is considerably faster that recompiling the 
kernel).

As an example, assume that the kernel has to be loaded at physical location 
0x4000000. Then, as we have not modified the CONFIG\_PAGE\_OFFSET (0xC0000000),
the kernel virtual address space would start at 0xC4000000. 

## Drawbacks

When the system has a high amount of RAM memory, there is a problem with this
method. Let's assume that we have a 32-bit system with 2GB of RAM memory and we
want to run two Linux-based partitions. For the version of PRTOS this has been
written, PAE addresses are not supported, so we are limited to a 4GB address
space. In conclusion We could assign each of the Linux partitions 1 GB of RAM.
For the first partition, the address range could be \[0x1000000-0x40000000\].
The virtual address space range then would be \[0xC1000000-0xFFFFFFFF\]. As you
can see, the addresses overflow and we are limited to a 1GB total application
image. 

Going back to the previous point, if we modified CONFIG\_PAGE\_OFFSET to a
proper value we could assign the 2GB of space by overlapping the virtual address
space of both partitions (of course, they would still be spatially isolated).
This solution may be considered for systems with high amount of memory which 
have not been addressed yet as PRTOS is mostly focused towards embedded
systems with lower memory requirements. 
 
# Usage

~~~
 $ relocate <vmlinux-file> <load-address>
~~~

# References

