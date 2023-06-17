$(if $(LINUX_PATH),, \
	$(warning "The configuration variable LINUX_PATH is not set,") \
	$(error "Please, define LINUX_PATH"))
export LINUX_PATH
	
include $(LINUX_PATH)/common/config.mk
$(if $(PRTOS_PATH),, \
	$(warning "The configuration variable PRTOS_PATH is not set,") \
	$(error "Please, define PRTOS_PATH"))
include $(BAIL_PATH)/common/rules.mk
