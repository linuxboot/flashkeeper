buildpath = build/
filename = flashkeeper
files = top.v spimemio.v simpleuart.v picosoc.v picorv32.v ice40up5k_spram.v
flashrom = $(HOME)/Flashkeeper/flashrom/builddir/flashrom -p ch341a_spi -c AT25DF041B

.PHONY: build prog prog_flash clean firmware

$(buildpath)$(filename)_breakout.bin: $(files) io_breakout.pcf
	yosys -p "synth_ice40 -dsp -json $(buildpath)$(filename)_breakout.json -top top" $(files)
	nextpnr-ice40 --freq 13 --package sg48 --up5k --json $(buildpath)$(filename)_breakout.json --pcf io_breakout.pcf --asc $(buildpath)$(filename)_breakout.asc
	icepack $(buildpath)$(filename)_breakout.asc $(buildpath)$(filename)_breakout.bin

$(buildpath)$(filename)_som.bin: $(files) io_som.pcf
	yosys -p "synth_ice40 -dsp -json $(buildpath)$(filename)_som.json -top top" $(files)
	nextpnr-ice40 --freq 13 --package uwg30 --up5k --json $(buildpath)$(filename)_som.json --pcf io_som.pcf --asc $(buildpath)$(filename)_som.asc
	icepack $(buildpath)$(filename)_som.asc $(buildpath)$(filename)_som.bin

prog_breakout: $(buildpath)$(filename)_breakout.bin #for sram
	iceprog -S $(buildpath)$(filename)_breakout.bin

prog_fw_breakout: firmware
	iceprog -o 128k firmware/build/firmware.bin

prog_flash_breakout: $(buildpath)$(filename)_breakout.bin prog_fw_breakout
	iceprog $(buildpath)$(filename)_breakout.bin

som_image: $(buildpath)$(filename)_som.bin firmware
	dd if=/dev/zero of=$(buildpath)$(filename)_som_image.bin bs=512k count=1  # create a blank 512 kB flash image
	dd if=$(buildpath)$(filename)_som.bin of=$(buildpath)$(filename)_som_image.bin conv=notrunc  # copy the bitstream to the beginning
	dd if=firmware/build/firmware.bin of=$(buildpath)$(filename)_som_image.bin bs=1 seek=128k conv=notrunc  # copy the firmware beginning at 128 kB

prog_flash_som: som_image
	$(flashrom) -w $(buildpath)$(filename)_som_image.bin

firmware: 
	$(MAKE) --directory=firmware/

clean:
	rm -rf $(buildpath)/*
	rm -rf firmware/build/*