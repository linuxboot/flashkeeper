/*
 * ebrrom - EBR-backed read-only memory window for the PicoRV32.
 *
 * Two 2048x16 memories -> 16 x SB_RAM40_4K (MODE 0: 256x16) = 8 kB,
 * presented as 2048 x 32-bit words (low half in rom_lo, high in rom_hi).
 *
 * The contents are preloaded from the bitstream at configuration time
 * (SB_RAM40_4K INIT data); the readmemh files are generated at build
 * time by tools/make_ebrrom_mem.py (see the gateware Makefile).
 *
 * Read port: synchronous, registered output, one-cycle latency
 * (address sampled at the rising edge, data valid the next cycle) -
 * the same read pattern as the SPRAM in ice40up5k_spram.v.
 */
module ebrrom (
	input clk,
	input [10:0] addr,      // word address, 0 .. 2047
	output reg [31:0] rdata
);
	reg [15:0] rom_lo [0:2047];
	reg [15:0] rom_hi [0:2047];

	initial begin
		$readmemh("build/ebrrom_lo.mem", rom_lo);
		$readmemh("build/ebrrom_hi.mem", rom_hi);
	end

	// Synchronous read, one-cycle latency: samples addr at the rising edge,
	// rdata valid on the following cycle. The read must be synchronous for
	// memory_libmap to match the SB_RAM40_4K library entry ("port sr R")
	// and map the arrays to EBRs with INIT data instead of LUT RAM.
	always @(posedge clk)
		rdata <= {rom_hi[addr], rom_lo[addr]};
endmodule
