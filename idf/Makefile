###############################################################################
# CONFIGURATION
###############################################################################

B_DIR := ../esp-sd-loader
CHIP ?= esp32s2

A_DIR := $(CURDIR)
A_BUILD := $(A_DIR)/build
B_BUILD := $(B_DIR)/build

PART_TABLE := $(B_DIR)/partitions.csv

###############################################################################
# MAIN TARGET (robust CSV parsing + auto-detect .bin files)
###############################################################################

firmware:
	@echo "=== CLEANING PROJECTS A and B ==="
	rm -rf $(A_BUILD) $(A_DIR)/sdkconfig
	rm -rf $(B_BUILD) $(B_DIR)/sdkconfig

	@echo "=== SET TARGET ($(CHIP)) & BUILD PROJECT B ==="
	cd $(B_DIR) && idf.py set-target $(CHIP)
	cd $(B_DIR) && idf.py build

	@echo "=== SET TARGET ($(CHIP)) & BUILD PROJECT A ==="
	cd $(A_DIR) && idf.py set-target $(CHIP)
	cd $(A_DIR) && idf.py build

	@echo "=== PARSING PARTITION TABLE ($(PART_TABLE)) ==="
	# Ensure partition-table exists
	@if [ ! -f "$(PART_TABLE)" ]; then \
	  echo "ERROR: partition-table.csv not found at $(PART_TABLE)"; exit 1; \
	fi

	# Fixed offsets
	$(eval BOOT_OFFSET := 0x1000)
	$(eval PART_OFFSET := 0x8000)

	# Parse offsets with awk: ignore comments / blank lines, trim spaces
	$(eval OTADATA_OFFSET := $(shell awk -F, '!/^#/ && NF && $$1!="" { gsub(/^[ \t]+|[ \t]+$$/,"",$$1); gsub(/^[ \t]+|[ \t]+$$/,"",$$4); if($$1=="otadata"){print $$4; exit}}' $(PART_TABLE)))
	$(eval APP0_OFFSET   := $(shell awk -F, '!/^#/ && NF && $$1!="" { gsub(/^[ \t]+|[ \t]+$$/,"",$$1); gsub(/^[ \t]+|[ \t]+$$/,"",$$4); if($$1=="app0"){print $$4; exit}}' $(PART_TABLE)))
	$(eval APP1_OFFSET   := $(shell awk -F, '!/^#/ && NF && $$1!="" { gsub(/^[ \t]+|[ \t]+$$/,"",$$1); gsub(/^[ \t]+|[ \t]+$$/,"",$$4); if($$1=="app1"){print $$4; exit}}' $(PART_TABLE)))

	@echo "Offsets parsed:"
	@echo "  bootloader = $(BOOT_OFFSET)"
	@echo "  part table = $(PART_OFFSET)"
	@echo "  otadata    = $(if $(strip $(OTADATA_OFFSET)),$(OTADATA_OFFSET),<NOT FOUND>)"
	@echo "  app0       = $(if $(strip $(APP0_OFFSET)),$(APP0_OFFSET),<NOT FOUND>)"
	@echo "  app1       = $(if $(strip $(APP1_OFFSET)),$(APP1_OFFSET),<NOT FOUND>)"

	# Fail early if any required offset missing
	@if [ -z "$(OTADATA_OFFSET)" ]; then echo "ERROR: otadata offset not found in partition-table.csv"; exit 1; fi
	@if [ -z "$(APP0_OFFSET)" ]; then echo "ERROR: app0 offset not found in partition-table.csv"; exit 1; fi
	@if [ -z "$(APP1_OFFSET)" ]; then echo "ERROR: app1 offset not found in partition-table.csv"; exit 1; fi

	@echo "=== LOCATING B's app binary and A's app binary ==="
	# Try to find sensible .bin candidates, excluding known meta files
	$(eval B_APP_BIN := $(shell find $(B_BUILD) -maxdepth 2 -type f -name '*.bin' ! -name 'bootloader.bin' ! -name 'partition-table.bin' ! -name 'ota_data_initial.bin' -print | head -n1))
	$(eval A_APP_BIN := $(shell find $(A_BUILD) -maxdepth 2 -type f -name '*.bin' ! -name 'bootloader.bin' ! -name 'partition-table.bin' ! -name 'ota_data_initial.bin' -print | head -n1))

	@if [ -z "$(B_APP_BIN)" ]; then echo "ERROR: could not find B's app .bin under $(B_BUILD)"; exit 1; fi
	@if [ -z "$(A_APP_BIN)" ]; then echo "ERROR: could not find A's app .bin under $(A_BUILD)"; exit 1; fi

	@echo "B app binary: $(B_APP_BIN)"
	@echo "A app binary: $(A_APP_BIN)"

	@echo "=== MERGING BINARIES ==="
	mkdir -p $(A_BUILD)
	esptool.py --chip $(CHIP) merge_bin -o $(A_BUILD)/image-$(CHIP).bin \
		$(BOOT_OFFSET) $(B_BUILD)/bootloader/bootloader.bin \
		$(PART_OFFSET) $(B_BUILD)/partition_table/partition-table.bin \
		$(OTADATA_OFFSET) $(B_BUILD)/ota_data_initial.bin \
		$(APP0_OFFSET) $(B_APP_BIN) \
		$(APP1_OFFSET) $(A_APP_BIN)

	@echo ""
	@echo "======================================================="
	@echo "  DONE! Image written to: $(A_BUILD)/image-$(CHIP).bin"
	@echo "======================================================="
	@echo ""

.PHONY: firmware
