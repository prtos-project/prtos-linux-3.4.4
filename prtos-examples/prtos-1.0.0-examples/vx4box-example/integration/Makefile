PARTITIONS=../linux-partition/linux-partition.pef 
BAIL_PATH=/home/prtos/prtos-sdk/bail
PACK_ARGS=-h $(PRTOS_CORE):prtos_cf.pef  \
	-p 0:../linux-partition/linux-partition.pef:../linux-partition/cmdline.pef
all: resident_sw

PRTOS_CF=../prtos_cf.xml
include $(BAIL_PATH)/common/rules.mk

prtos_cf.pef: $(PRTOS_CF)
	$(PRTOS_CPARSER) -o prtos_cf.bin $^
	$(PEF) -m prtos_cf.bin -c -o $@
	@$(RM) -f prtos_cf.bin

container.bin: $(PARTITIONS) prtos_cf.pef
	$(PRTOS_PACK) check prtos_cf.pef $(PACK_ARGS)
	$(PRTOS_PACK) build $(PACK_ARGS) $@
