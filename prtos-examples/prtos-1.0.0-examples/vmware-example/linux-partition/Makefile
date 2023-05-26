PARTITION=linux-partition
LINUX_PATH=/home/tom/Public/hde/linux-sdk
ID=0
CMDLINE=cmdline

include $(LINUX_PATH)/common/rules.mk

PRTOS_CF=../prtos_cf.xml
all: $(PARTITION).xef $(CMDLINE).xef

$(PARTITION):
	@$(BUILD_LINUX) -r root -a $(shell $(XPATHSTART) $(ID) $(PRTOS_CF)) -o $@ $(LINUX_PATH)/vmlinux

$(CMDLINE).xef: $(CMDLINE)
	@$(XEF) -m -o $@ $<

clean: myclean
myclean:
	$(RM) -f $(PARTITION)*

