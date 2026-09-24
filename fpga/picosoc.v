/*
 *  PicoSoC - A simple example SoC using PicoRV32
 *
 *  Copyright (C) 2017  Claire Xenia Wolf <claire@yosyshq.com>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

`ifndef PICORV32_REGS
`ifdef PICORV32_V
`error "picosoc.v must be read before picorv32.v!"
`endif

`define PICORV32_REGS picosoc_regs
`endif

`ifndef PICOSOC_MEM
`define PICOSOC_MEM picosoc_mem
`endif

// this macro can be used to check if the verilog files in your
// design are read in the correct order.
`define PICOSOC_V

module picosoc (
	input clk,
	input resetn,

	output        iomem_valid,
	input         iomem_ready,
	output [ 3:0] iomem_wstrb,
	output [31:0] iomem_addr,
	output [31:0] iomem_wdata,
	input  [31:0] iomem_rdata,

	input  irq_5,
	input  irq_6,
	input  irq_7,

	output ser_tx,
	input  ser_rx,

	output flash_csb,
	output flash_clk,

	output flash_io0_oe,
	output flash_io1_oe,
	output flash_io2_oe,
	output flash_io3_oe,

	output flash_io0_do,
	output flash_io1_do,
	output flash_io2_do,
	output flash_io3_do,

	input  flash_io0_di,
	input  flash_io1_di,
	input  flash_io2_di,
	input  flash_io3_di,

	output hspi_csb,
	output hspi_clk,

	output hspi_io0_oe,
	output hspi_io1_oe,
	output hspi_io2_oe,
	output hspi_io3_oe,

	output hspi_io0_do,
	output hspi_io1_do,
	output hspi_io2_do,
	output hspi_io3_do,

	input  hspi_io0_di,
	input  hspi_io1_di,
	input  hspi_io2_di,
	input  hspi_io3_di
);
	parameter [0:0] BARREL_SHIFTER = 1;
	parameter [0:0] ENABLE_MUL = 1;
	parameter [0:0] ENABLE_DIV = 1;
	parameter [0:0] ENABLE_FAST_MUL = 0;
	parameter [0:0] ENABLE_COMPRESSED = 1;
	parameter [0:0] ENABLE_COUNTERS = 1;
	parameter [0:0] ENABLE_IRQ_QREGS = 0;

	parameter integer MEM_WORDS = 256;
	parameter [31:0] STACKADDR = (4*MEM_WORDS);       // end of memory
	parameter [31:0] PROGADDR_RESET = 32'h 0010_0000; // 1 MB into flash
	parameter [31:0] PROGADDR_IRQ = 32'h 0000_0000;

	reg [31:0] irq;
	wire irq_stall = 0;
	wire irq_uart = 0;

	always @* begin
		irq = 0;
		irq[3] = irq_stall;
		irq[4] = irq_uart;
		irq[5] = irq_5;
		irq[6] = irq_6;
		irq[7] = irq_7;
	end

	wire mem_valid;
	wire mem_instr;
	wire mem_ready;
	wire [31:0] mem_addr;
	wire [31:0] mem_wdata;
	wire [3:0] mem_wstrb;
	wire [31:0] mem_rdata;

	wire spimem_ready;
	wire [31:0] spimem_rdata;

	wire hspi_ready;
	wire [31:0] hspi_rdata;

	reg ram_ready;
	wire [31:0] ram_rdata;

	// EBR ROM window: 0x06000000..0x06001FFF (8 kB, 2048 x 32-bit words).
	// Preloaded from the bitstream (will host the no-XIP boot ROM).
	// 16 MB slot, following the existing pattern: 0x04xxxxxx = host flash,
	// 0x06xxxxxx = EBR ROM, 0x08xxxxxx = GPIO. Does not touch the spimemio
	// XIP window, so the whole onboard flash stays accessible.
	wire ebrrom_sel = mem_valid && (mem_addr >= 32'h 0600_0000) && (mem_addr < 32'h 0600_2000);
	reg ebrrom_ready;
	wire [31:0] ebrrom_rdata;

	// RNG data register: 0x07000200 (see rng.v). Read-only, one bit in
	// bit0. Each read returns the current random bit and shifts in a
	// fresh sample for the next read (read-to-shift), so the firmware's
	// read rate is the sampling rate.
	wire        rng_reg_dat_sel = mem_valid && (mem_addr == 32'h 0700_0200);
	wire [31:0] rng_reg_dat_do;

	assign iomem_valid = mem_valid && (mem_addr[31:24] >= 8'h 04);
	assign iomem_wstrb = mem_wstrb;
	assign iomem_addr = mem_addr;
	assign iomem_wdata = mem_wdata;

	wire spimemio_cfgreg_sel = mem_valid && (mem_addr == 32'h 0200_0000);
	wire [31:0] spimemio_cfgreg_do;

	wire hspi_cfgreg_sel = mem_valid && (mem_addr == 32'h 0500_0000);
	wire [31:0] hspi_cfgreg_do;

	// Window reads while the controller is in manual mode (config_en=0):
	// the FSM is held in softreset and can never assert ready, so serve 0s
	// instead of stalling the CPU forever. (Window reads require
	// spi_memio_enable() first; manual mode is sticky after flashio calls.)
	wire cspi_win_off = mem_valid && (mem_addr >= 32'h 0100_0000) && (mem_addr < 32'h 0200_0000) && !spimemio_cfgreg_do[31];
	wire hspi_win_off = mem_valid && (mem_addr >= 32'h 0400_0000) && (mem_addr < 32'h 0500_0000) && !hspi_cfgreg_do[31];

	wire        uart_reg_div_sel = mem_valid && (mem_addr == 32'h 0200_0004);
	wire [31:0] uart_reg_div_do;

	wire        uart_reg_dat_sel = mem_valid && (mem_addr == 32'h 0200_0008);
	wire [31:0] uart_reg_dat_do;
	wire        uart_reg_dat_wait;

	wire        uart_reg_sts_sel = mem_valid && (mem_addr == 32'h 0200_000C);
	wire [31:0] uart_reg_sts_do;

	assign mem_ready = (iomem_valid && iomem_ready) ||
			cspi_win_off ||
			hspi_win_off ||
			spimem_ready ||
			spimemio_cfgreg_sel ||
			hspi_ready ||
			hspi_cfgreg_sel ||
			ram_ready ||
			ebrrom_ready ||
			rng_reg_dat_sel ||
			uart_reg_div_sel || 
			(uart_reg_dat_sel && !uart_reg_dat_wait) ||
			uart_reg_sts_sel;

	assign mem_rdata = (iomem_valid && iomem_ready) ? iomem_rdata : 
			cspi_win_off ? 32'h 0000_0000 :
			hspi_win_off ? 32'h 0000_0000 :
			spimem_ready ? spimem_rdata :
			spimemio_cfgreg_sel ? spimemio_cfgreg_do :
			hspi_ready ? hspi_rdata :
			hspi_cfgreg_sel ? hspi_cfgreg_do :
			ram_ready ? ram_rdata :
			ebrrom_ready ? ebrrom_rdata :
			rng_reg_dat_sel ? rng_reg_dat_do :
			uart_reg_div_sel ? uart_reg_div_do :
			uart_reg_dat_sel ? uart_reg_dat_do :
			uart_reg_sts_sel ? uart_reg_sts_do :
			32'h 0000_0000;

	picorv32 #(
		.STACKADDR(STACKADDR),
		.PROGADDR_RESET(PROGADDR_RESET),
		.PROGADDR_IRQ(PROGADDR_IRQ),
		.BARREL_SHIFTER(BARREL_SHIFTER),
		.COMPRESSED_ISA(ENABLE_COMPRESSED),
		.ENABLE_COUNTERS(ENABLE_COUNTERS),
		.ENABLE_MUL(ENABLE_MUL),
		.ENABLE_DIV(ENABLE_DIV),
		.ENABLE_FAST_MUL(ENABLE_FAST_MUL),
		.ENABLE_IRQ(1),
		.ENABLE_IRQ_QREGS(ENABLE_IRQ_QREGS)
	) cpu (
		.clk         (clk        ),
		.resetn      (resetn     ),
		.mem_valid   (mem_valid  ),
		.mem_instr   (mem_instr  ),
		.mem_ready   (mem_ready  ),
		.mem_addr    (mem_addr   ),
		.mem_wdata   (mem_wdata  ),
		.mem_wstrb   (mem_wstrb  ),
		.mem_rdata   (mem_rdata  ),
		.irq         (irq        )
	);

	spimemio host_spi (
        .clk    (clk),
        .resetn (resetn),
        .valid  (mem_valid && mem_addr >= 32'h 0400_0000 && mem_addr < 32'h 0500_0000),
        .ready  (hspi_ready),
        .addr   (mem_addr[23:0]),
        .rdata  (hspi_rdata),

        .flash_csb    (hspi_csb),
        .flash_clk    (hspi_clk),

        .flash_io0_oe (hspi_io0_oe),
        .flash_io1_oe (hspi_io1_oe),
        .flash_io2_oe (hspi_io2_oe),
        .flash_io3_oe (hspi_io3_oe),

        .flash_io0_do (hspi_io0_do),
        .flash_io1_do (hspi_io1_do),
        .flash_io2_do (hspi_io2_do),
        .flash_io3_do (hspi_io3_do),

        .flash_io0_di (hspi_io0_di),
        .flash_io1_di (hspi_io1_di),
        .flash_io2_di (hspi_io2_di),
        .flash_io3_di (hspi_io3_di),

        .cfgreg_we(hspi_cfgreg_sel ? mem_wstrb : 4'b0000),
        .cfgreg_di(mem_wdata),
        .cfgreg_do(hspi_cfgreg_do)
    );

	spimemio spimemio (
		.clk    (clk),
		.resetn (resetn),
		.valid  (mem_valid && mem_addr >= 32'h 0100_0000 && mem_addr < 32'h 0200_0000),
		.ready  (spimem_ready),
		.addr   (mem_addr[23:0]),
		.rdata  (spimem_rdata),

		.flash_csb    (flash_csb   ),
		.flash_clk    (flash_clk   ),

		.flash_io0_oe (flash_io0_oe),
		.flash_io1_oe (flash_io1_oe),
		.flash_io2_oe (flash_io2_oe),
		.flash_io3_oe (flash_io3_oe),

		.flash_io0_do (flash_io0_do),
		.flash_io1_do (flash_io1_do),
		.flash_io2_do (flash_io2_do),
		.flash_io3_do (flash_io3_do),

		.flash_io0_di (flash_io0_di),
		.flash_io1_di (flash_io1_di),
		.flash_io2_di (flash_io2_di),
		.flash_io3_di (flash_io3_di),

		.cfgreg_we(spimemio_cfgreg_sel ? mem_wstrb : 4'b 0000),
		.cfgreg_di(mem_wdata),
		.cfgreg_do(spimemio_cfgreg_do)
	);

	uart uart (
		.clk         (clk         ),
		.resetn      (resetn      ),

		.ser_tx      (ser_tx      ),
		.ser_rx      (ser_rx      ),

		.reg_div_we  (uart_reg_div_sel ? mem_wstrb : 4'b 0000),
		.reg_div_di  (mem_wdata),
		.reg_div_do  (uart_reg_div_do),

		.reg_dat_we  (uart_reg_dat_sel ? mem_wstrb[0] : 1'b 0),
		.reg_dat_re  (uart_reg_dat_sel && !mem_wstrb),
		.reg_dat_di  (mem_wdata),
		.reg_dat_do  (uart_reg_dat_do),
		.reg_dat_wait(uart_reg_dat_wait),

		.reg_sts_we  (uart_reg_sts_sel ? mem_wstrb[0] : 1'b 0),
		.reg_sts_di  (mem_wdata),
		.reg_sts_do  (uart_reg_sts_do)
	);

	rng rng_inst (
		.clk(clk),
		.resetn(resetn),
		.re(rng_reg_dat_sel && !mem_wstrb),
		.rdata(rng_reg_dat_do)
	);

	ebrrom ebrrom_inst (
		.clk(clk),
		.addr(mem_addr[12:2]),
		.rdata(ebrrom_rdata)
	);

	// One-cycle latency read, like the SPRAM: the EBR samples the address
	// at the rising edge and drives rdata on the following cycle.
	always @(posedge clk)
		ebrrom_ready <= mem_valid && !mem_ready && ebrrom_sel;

	always @(posedge clk)
		ram_ready <= mem_valid && !mem_ready && mem_addr < 4*MEM_WORDS;

	`PICOSOC_MEM #(
		.WORDS(MEM_WORDS)
	) memory (
		.clk(clk),
		.wen((mem_valid && !mem_ready && mem_addr < 4*MEM_WORDS) ? mem_wstrb : 4'b0),
		.addr(mem_addr[23:2]),
		.wdata(mem_wdata),
		.rdata(ram_rdata)
	);
endmodule

// Implementation note:
// Replace the following two modules with wrappers for your SRAM cells.

module picosoc_regs (
	input clk, wen,
	input [5:0] waddr,
	input [5:0] raddr1,
	input [5:0] raddr2,
	input [31:0] wdata,
	output [31:0] rdata1,
	output [31:0] rdata2
);
	reg [31:0] regs [0:31];

	always @(posedge clk)
		if (wen) regs[waddr[4:0]] <= wdata;

	assign rdata1 = regs[raddr1[4:0]];
	assign rdata2 = regs[raddr2[4:0]];
endmodule

module picosoc_mem #(
	parameter integer WORDS = 256
) (
	input clk,
	input [3:0] wen,
	input [21:0] addr,
	input [31:0] wdata,
	output reg [31:0] rdata
);
	reg [31:0] mem [0:WORDS-1];

	always @(posedge clk) begin
		rdata <= mem[addr];
		if (wen[0]) mem[addr][ 7: 0] <= wdata[ 7: 0];
		if (wen[1]) mem[addr][15: 8] <= wdata[15: 8];
		if (wen[2]) mem[addr][23:16] <= wdata[23:16];
		if (wen[3]) mem[addr][31:24] <= wdata[31:24];
	end
endmodule