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
 *  MERCHANTABILITY OR FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

module uart #(parameter integer DEFAULT_DIV = 1) (
	input clk,
	input resetn,

	output ser_tx,
	input  ser_rx,

	input   [3:0] reg_div_we,
	input  [31:0] reg_div_di,
	output [31:0] reg_div_do,

	input         reg_dat_we,
	input         reg_dat_re,
	input  [31:0] reg_dat_di,
	output [31:0] reg_dat_do,
	output        reg_dat_wait,

	input         reg_sts_we,
	input  [31:0] reg_sts_di,
	output [31:0] reg_sts_do
);
	reg [31:0] cfg_divider;

	reg [3:0] recv_state;
	reg [31:0] recv_divcnt;
	reg [7:0] recv_pattern;

	// ---------------------------------------------------------------
	// RX FIFO: 512 bytes in an EBR (SB_RAM40_4K, 512x8 dual-port mode).
	// The RX state machine writes on the write port, the CPU reads on
	// the read port. True dual-port, so no arbitration is needed.
	//
	// Overflow policy: drop newest + sticky flag in the status register
	// (16550-style overrun; cleared by writing 1 to status bit 1).
	//
	// In 8-bit mode the logical byte lives on the even physical
	// bitlines: bit i on pin 2*i (see Project IceStorm "RAM Tile"
	// documentation: "Used WDATA/RDATA bits: 14,12,10,8,6,4,2,0").
	//
	// The read port is registered: a read request takes two cycles
	// (request -> data valid). Reads on an empty FIFO take one cycle
	// and return 0xFF, keeping the CPU's poll loop fast.
	// ---------------------------------------------------------------
	reg  [9:0] rx_wrptr, rx_rdptr;  // 9 address bits + 1 wrap bit
	reg        rx_overflow;
	reg        rx_rreq;             // read request pending (data valid next cycle)
	reg  [7:0] rx_ferr;             // framing error counter (saturates at 0xFF)
	wire [15:0] rx_rdata;

	wire rx_empty = (rx_wrptr == rx_rdptr);
	wire rx_full  = (rx_wrptr[8:0] == rx_rdptr[8:0]) && (rx_wrptr[9] != rx_rdptr[9]);
	wire rx_done  = (recv_state == 10) && (recv_divcnt > cfg_divider);
	
	// Framing check (16550-style): the rx_done edge falls in the middle of
	// the true stop bit when the receiver is bit-aligned, so ser_rx must be
	// high there. Low means the RX lost bit alignment (bit slip after a long
	// bit / glitch on the line): discard the byte and count it, never push
	// a slipped byte into the stream.
	wire rx_frame_ok = ser_rx;
	wire rx_wr    = rx_done && !rx_full && rx_frame_ok;
	wire rx_framing = rx_done && !rx_frame_ok;

	SB_RAM40_4K #(.READ_MODE(1), .WRITE_MODE(1)) rx_fifo (
		.WCLK(clk),  .WCLKE(1'b1), .WE(rx_wr),
		.WADDR({2'b00, rx_wrptr[8:0]}),
		.WDATA({1'b0, recv_pattern[7], 1'b0, recv_pattern[6], 1'b0, recv_pattern[5], 1'b0, recv_pattern[4],
		        1'b0, recv_pattern[3], 1'b0, recv_pattern[2], 1'b0, recv_pattern[1], 1'b0, recv_pattern[0]}),
		.MASK(16'h0000),
		.RCLK(clk),  .RCLKE(1'b1), .RE(1'b1),
		.RADDR({2'b00, rx_rdptr[8:0]}),
		.RDATA(rx_rdata)
	);

	wire [7:0] rx_data = {rx_rdata[14], rx_rdata[12], rx_rdata[10], rx_rdata[8],
	                      rx_rdata[6],  rx_rdata[4],  rx_rdata[2],  rx_rdata[0]};

	always @(posedge clk) begin
		if (!resetn) begin
			rx_wrptr    <= 0;
			rx_rdptr    <= 0;
			rx_overflow <= 0;
			rx_rreq     <= 0;
			rx_ferr     <= 0;
		end else begin
			if (rx_wr)
				rx_wrptr <= rx_wrptr + 1;
			if (rx_done && rx_full)
				rx_overflow <= 1;
			else if (reg_sts_we && reg_sts_di[1])
				rx_overflow <= 0;
			if (rx_framing)
				rx_ferr <= (rx_ferr == 8'hFF) ? 8'hFF : rx_ferr + 1;
			if (rx_rreq) begin
				rx_rreq  <= 0;
				rx_rdptr <= rx_rdptr + 1;
			end else if (reg_dat_re && !rx_empty)
				rx_rreq <= 1;
		end
	end

	assign reg_div_do = cfg_divider;

	// Write: block until the transmitter is idle (1-byte TX register).
	// Read: block for the one cycle while the EBR read settles.
	assign reg_dat_wait = (reg_dat_we && (send_bitcnt || send_dummy)) ||
			(reg_dat_re && !rx_empty && !rx_rreq);
	assign reg_dat_do = rx_rreq ? {24'h000000, rx_data} : ~0;

	// Status register: bit 0 = FIFO not empty, bit 1 = overrun (W1C),
	// bits [15:8] = framing error counter (read-only, cumulative since reset)
	assign reg_sts_do = {23'h0, rx_ferr, rx_overflow, !rx_empty};

	always @(posedge clk) begin
		if (!resetn) begin
			cfg_divider <= DEFAULT_DIV;
		end else begin
			if (reg_div_we[0]) cfg_divider[ 7: 0] <= reg_div_di[ 7: 0];
			if (reg_div_we[1]) cfg_divider[15: 8] <= reg_div_di[15: 8];
			if (reg_div_we[2]) cfg_divider[23:16] <= reg_div_di[23:16];
			if (reg_div_we[3]) cfg_divider[31:24] <= reg_div_di[31:24];
		end
	end

	always @(posedge clk) begin
		if (!resetn) begin
			recv_state <= 0;
			recv_divcnt <= 0;
			recv_pattern <= 0;
		end else begin
			recv_divcnt <= recv_divcnt + 1;
			case (recv_state)
				0: begin
					if (!ser_rx)
						recv_state <= 1;
					recv_divcnt <= 0;
				end
				1: begin
					// Half-bit wait: commit to the frame only if the line is
					// still low. A sub-half-bit noise dip is rejected instead
					// of being consumed as a spurious all-ones frame.
					if (2*recv_divcnt > cfg_divider) begin
						if (!ser_rx)
							recv_state <= 2;
						else
							recv_state <= 0;
						recv_divcnt <= 0;
					end
				end
				10: begin
					if (recv_divcnt > cfg_divider)
						recv_state <= 0;
				end
				default: begin
					if (recv_divcnt > cfg_divider) begin
						recv_pattern <= {ser_rx, recv_pattern[7:1]};
						recv_state <= recv_state + 1;
						recv_divcnt <= 0;
					end
				end
			endcase
		end
	end

	reg [9:0] send_pattern;
	reg [3:0] send_bitcnt;
	reg [31:0] send_divcnt;
	reg send_dummy;

	assign ser_tx = send_pattern[0];

	always @(posedge clk) begin
		if (reg_div_we)
			send_dummy <= 1;
		send_divcnt <= send_divcnt + 1;
		if (!resetn) begin
			send_pattern <= ~0;
			send_bitcnt <= 0;
			send_divcnt <= 0;
			send_dummy <= 1;
		end else begin
			if (send_dummy && !send_bitcnt) begin
				send_pattern <= ~0;
				send_bitcnt <= 15;
				send_divcnt <= 0;
				send_dummy <= 0;
			end else
			if (reg_dat_we && !send_bitcnt) begin
				send_pattern <= {1'b1, reg_dat_di[7:0], 1'b0};
				send_bitcnt <= 10;
				send_divcnt <= 0;
			end else
			if (send_divcnt > cfg_divider && send_bitcnt) begin
				send_pattern <= {1'b1, send_pattern[9:1]};
				send_bitcnt <= send_bitcnt - 1;
				send_divcnt <= 0;
			end
		end
	end
endmodule
