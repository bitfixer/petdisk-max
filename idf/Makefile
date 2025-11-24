firmware-esp32s2:
	rm -r -f build; rm -f sdkconfig
	cd ..; cd esp-sd-loader; rm -r -f build; rm -f sdkconfig; idf.py set-target esp32s2; idf.py build
	idf.py set-target esp32s2; idf.py build
	# assemble flash image from components
	esptool.py --chip esp32s2 merge_bin -o build/image-esp32s2.bin \
		0x1000 ../esp-sd-loader/build/bootloader/bootloader.bin \
		0x8000 ../esp-sd-loader/build/partition_table/partition-table.bin \
		0xe000 ../esp-sd-loader/build/ota_data_initial.bin \
		0x10000 ../esp-sd-loader/build/main.bin \
		0x60000 build/main.bin