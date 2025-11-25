###############################################################################
# CONFIGURATION
###############################################################################

B_DIR := ../esp-sd-loader
PORT ?= /dev/ttyUSB0
CHIP ?= esp32s2

A_DIR := $(CURDIR)
A_BUILD := $(A_DIR)/build
B_BUILD := $(B_DIR)/build

PART_TABLE := $(B_DIR)/partitions.csv
IMAGE_FILE := $(A_BUILD)/image-$(CHIP).bin

A_APP_BIN := $(A_BUILD)/main.bin
B_APP_BIN := $(B_BUILD)/main.bin

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

	@echo "B app binary: $(B_APP_BIN)"
	@echo "A app binary: $(A_APP_BIN)"

	@echo "=== MERGING BINARIES ==="
	mkdir -p $(A_BUILD)
	esptool.py --chip $(CHIP) merge_bin -o $(IMAGE_FILE) \
		$(BOOT_OFFSET) $(B_BUILD)/bootloader/bootloader.bin \
		$(PART_OFFSET) $(B_BUILD)/partition_table/partition-table.bin \
		$(OTADATA_OFFSET) $(B_BUILD)/ota_data_initial.bin \
		$(APP0_OFFSET) $(B_APP_BIN) \
		$(APP1_OFFSET) $(A_APP_BIN)

	@echo ""
	@echo "======================================================="
	@echo "  DONE! Image written to: $(IMAGE_FILE)"
	@echo "======================================================="
	@echo ""

.PHONY: firmware

#
# Flash full monolithic image
#
flash:
	@if [ ! -f "$(IMAGE_FILE)" ]; then \
		echo "ERROR: Image file not found: $(IMAGE_FILE)"; \
		echo "Build the project first."; \
		exit 1; \
	fi

	esptool.py --chip $(CHIP) -p $(PORT) write_flash 0x0 $(IMAGE_FILE)
