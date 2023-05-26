PARTITIONS=linux-partition 
ifndef V
MAKEFLAGS   += --no-print-directory --quiet
endif

all: resident_sw

resident_sw:
	@for dir in $(PARTITIONS) ; do \
		$(MAKE) -C $$dir all; \
	done
	@$(MAKE) -C integration ${MAKECMDGOALS}
	@cp integration/resident_sw* .

resident_sw.iso: resident_sw
	@$(MAKE) -C integration ${MAKECMDGOALS}
	@cp integration/resident_sw* .

clean:
	@for dir in $(PARTITIONS) ; do \
		$(MAKE) -C $$dir $(MAKECMDGOALS); \
		$(RM) $$dir/*.xef; \
	done
	@$(MAKE) -C integration ${MAKECMDGOALS}
	@$(RM) integration/*.xef integration/resident_sw* resident_sw*
	@$(RM) *.iso

