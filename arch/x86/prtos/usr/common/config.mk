# ARCH: The PRTOS target architecture
ARCH=x86

# LINUX_PATH: path to the LINUX PRTOS SDK directory
LINUX_PATH=/opt/linux-sdk/

# PRTOS_PATH: path to the PRTOS PRTOS SDK directory
PRTOS_PATH=/opt/prtos-sdk/prtos

# End of configurable section: do NOT edit below edit below this line

BAIL_PATH=${PRTOS_PATH}/../bail

LINUXBIN_PATH=${LINUX_PATH}/bin
BUILD_LINUX=${LINUXBIN_PATH}/build_linux
RELOCATE=${LINUXBIN_PATH}/relocate
MKINITRAMFS=${LINUXBIN_PATH}/mkinitramfs
INITRD_ROOT=${LINUX_PATH}/initrd-root

export ARCH LINUX_PATH PRTOS_PATH BAIL_PATH
export BUILD_LINUX RELOCATE MKINITRAMFS INITRD_ROOT
