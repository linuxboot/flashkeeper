`default_nettype none
`timescale 1ns / 1ps

/*
 * rng.v - ring oscillator entropy source peripheral.
 *
 * Entropy source is N_RO independent 5-stage ring oscillators built 
 * from SB_LUT4 primitives (16'h5555 = inverter). Process/routing
 * variation gives each ring a slightly different frequency. Rings
 * are free-running and start oscillating within microseconds of
 * power-up. The rings oscillate ~10-30 cycles per system clock.
 *
 * XOR of all rings (rnd_raw) is sampled into rnd_bit only when the CPU
 * reads the data register: each read returns the current bit and loads
 * a fresh sample for the next read.
 *
 * Address map (see picosoc.v):
 *   0x07000200  DATA (read): bit0 = random bit; each read returns the
                 current bit and samples one fresh bit
 */

module rng (
    input  clk,
    input  resetn,

    // CPU read port. re = one pulse per CPU read of the data register.
    input  re,
    output [31:0] rdata
);
    // Four rings should achieve sufficient min-entropy for 512-per-4096-bit.
    // Can be increased to 8 for a few more LUTs if needed.
    localparam N_RO = 4;

    // Ring oscillators. SB_LUT4 is a primitive and will not be optimized
    // away. (* keep *) preserves feedback wires.
    wire [N_RO-1:0] ro_out;

    // Ring oscillators
    genvar i;
    generate
        for (i = 0; i < N_RO; i = i + 1) begin : gen_ro
            (* keep *) wire [4:0] chain;

            SB_LUT4 #(.LUT_INIT(16'h5555)) u_inv0 (
                .I0(chain[4]), .I1(1'b0), .I2(1'b0), .I3(1'b0),
                .O(chain[0])
            );
            SB_LUT4 #(.LUT_INIT(16'h5555)) u_inv1 (
                .I0(chain[0]), .I1(1'b0), .I2(1'b0), .I3(1'b0),
                .O(chain[1])
            );
            SB_LUT4 #(.LUT_INIT(16'h5555)) u_inv2 (
                .I0(chain[1]), .I1(1'b0), .I2(1'b0), .I3(1'b0),
                .O(chain[2])
            );
            SB_LUT4 #(.LUT_INIT(16'h5555)) u_inv3 (
                .I0(chain[2]), .I1(1'b0), .I2(1'b0), .I3(1'b0),
                .O(chain[3])
            );
            SB_LUT4 #(.LUT_INIT(16'h5555)) u_inv4 (
                .I0(chain[3]), .I1(1'b0), .I2(1'b0), .I3(1'b0),
                .O(chain[4])
            );

            assign ro_out[i] = chain[4];
        end
    endgenerate

    // Collapse the rings into one bit.
    wire rnd_raw = ^ro_out;

    // Sampling flip-flop
    reg rnd_bit;

    always @(posedge clk) begin
        if (!resetn)
            rnd_bit <= 1'b0;
        else if (re)
            rnd_bit <= rnd_raw; // shift in new entropy bit when register read
    end

    assign rdata = {31'h0, rnd_bit};

endmodule
