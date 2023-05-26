#
# User tools rules
#

hostprogs-y := bin/relocate
clean-files += initrd.img.gz $(hostprogs-y)
always += initrd.img.gz $(hostprogs-y)
HOST_EXTRACFLAGS := -O2 -m32 -Wall -static -DCONFIG_PHYSICAL_START=$(CONFIG_PHYSICAL_START) $(EXTRA_CFLAGS)

$(src)/initrd.img.gz: $(src)/initrd-root FORCE
	$(Q)echo "  INITRD  $@ -> $<"
	$(Q)sh scripts/gen_initramfs_list.sh -o $@ $<
	$(Q)echo "" | ${CC} ${KBUILD_CFLAGS} -x c -c - -o $(src)/built-in.o # TBD

# requisites
$(if $(shell which makeself 2>/dev/null),,\
	$(warning "Error: makeself program not found.")\
	$(error "Please install the makeself package"))

#
# Linux distro rules
#

DISTRO = linux-${KERNELVERSION}
DISTROTMP =/tmp/$(DISTRO)-$$PPID
DISTROTAR = $(DISTRO).tar.bz2
DISTRORUN = $(DISTRO).run
DISTROLABEL = "LINUX binary distribution $(KERNELVERSION): "

$(DISTROTAR): clean distro-kernel
	$(Q)rm -f $(DISTROTAR)
	$(Q)mkdir $(DISTROTMP) || exit -1
	$(Q)bin/linuxdistro $(DISTROTMP)/$(DISTRO) $(DISTROTAR)
	$(Q)rm -f -r $(DISTROTMP)

$(DISTRORUN): $(DISTROTAR)
	$(Q)mkdir $(DISTROTMP) || exit -1
	$(Q)tar xf $(DISTROTAR) -C $(DISTROTMP)
	$(Q)/bin/echo "> Generating self extracting binary distribution \"$(DISTRORUN)\""
	$(Q)makeself --bzip2 $(DISTROTMP)/$(DISTRO) $(DISTRORUN) $(DISTROLABEL) ./linux-installer > /dev/null 2>&1
	$(Q)rm -f $(DISTROTAR)
	$(Q)rm -f -r $(DISTROTMP)
	$(Q)/bin/echo -e "> Done\n"

distro-kernel:
	$(Q)/bin/echo "> Building Linux Kernel Image/Modules $(KERNELRELEASE)"
	$(Q)$(MAKE) -C ${srctree} vmlinux modules ARCH=x86 INSTALL_MOD_PATH=${srctree}/arch/x86/prtos/usr/
	$(Q)$(MAKE) -C ${srctree} modules_install ARCH=x86 INSTALL_MOD_PATH=${srctree}/arch/x86/prtos/usr/
	$(Q)/bin/echo -e "> Done"

distro-help:
	@echo  'Available distro targets:'
	@echo  "  distro-run - Prepares a $(DISTROLABEL)as a .run self-installer"
	@echo  "  distro-tar - Prepares a $(DISTROLABEL)as a .tar package"
	@echo  "  clean      - Remove all generated files"

distro-tar: $(DISTROTAR)
distro-run: $(DISTRORUN)
clean:
	$(Q)rm -f $(DISTROTAR) $(DISTRORUN)
