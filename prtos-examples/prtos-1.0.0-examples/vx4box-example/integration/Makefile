PARTITIONS=../linux-partition/linux-partition.xef 
XAL_PATH=/home/prtos/prtos-sdk/xal
PACK_ARGS=-h $(PRTOSCORE):prtos_cf.xef  \
	-p 0:../linux-partition/linux-partition.xef:../linux-partition/cmdline.xef
all: resident_sw

PRTOS_CF=../prtos_cf.xml
include $(XAL_PATH)/common/rules.mk

prtos_cf.xef: $(PRTOS_CF)
	$(PRTOSCPARSER) -o prtos_cf.bin $^
	$(XEF) -m prtos_cf.bin -c -o $@
	@$(RM) -f prtos_cf.bin

container.bin: $(PARTITIONS) prtos_cf.xef
	$(PRTOSPACK) check prtos_cf.xef $(PACK_ARGS)
	$(PRTOSPACK) build $(PACK_ARGS) $@
