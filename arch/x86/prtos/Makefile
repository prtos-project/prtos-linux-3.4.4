#
# Makefile for the PRTOS hypevisor
#

PRTOS_PATH = $(shell echo $(CONFIG_PRTOS_PATH))
LIBPRTOS_INCLUDE=$(PRTOS_PATH)/include
LIBXMA_PATH=$(PRTOS_PATH)/lib/libprtos.a

$(if $(wildcard $(LIBPRTOS_INCLUDE) $(LIBXMA_PATH)),,\
	$(warning "Error: PRTOS SDK not found at "$(CONFIG_PRTOS_PATH)".")\
	$(error "Please use make menuconfig to configure: CONFIG_PRTOS_PATH=/path/to/prtos-sdk"))

EXTRA_CFLAGS += -D"__PRTOS_INCFLD(_fld)=<prtos_inc/_fld>"
EXTRA_CFLAGS += -I$(LIBPRTOS_INCLUDE) --include prtos_inc/arch/arch_types.h
EXTRA_CFLAGS += $(call cc-option, -fno-stack-protector)
EXTRA_AFLAGS += -I$(LIBPRTOS_INCLUDE) -D"__PRTOS_INCFLD(_fld)=<prtos_inc/_fld>"

CONFIG_PRTOS_BUILDREVISION := '"'$(shell svn info >/dev/null 2>&1 && svn info | awk '/Revision/ {print "r"$$2}' || echo r0)'"'
EXTRA_CFLAGS += -DCONFIG_PRTOS_BUILDREVISION=${CONFIG_PRTOS_BUILDREVISION}

obj-$(CONFIG_PRTOS_PARTITION)	:= head.o setup_32.o processor.o irq.o time.o vmm.o hdr.o prtosmap.o probe_roms.o prtossrv.o libprtos.a 

# TBD subdir-y += usr/
obj-y += usr/
always   += libprtos.a # checkpatch.log 

$(src)/libprtos.a: $(LIBXMA_PATH) FORCE
	@echo "  SYMLINK $@ -> $<"
	@rm -f $@ && ln -s $< $@

$(src)/checkpatch.log: scripts/checkpatch.pl
	@echo "  CALL    $<"
	@$< --root $(srctree) --file /$(srctree)/$(src)/*.[ch] > $@ || true
