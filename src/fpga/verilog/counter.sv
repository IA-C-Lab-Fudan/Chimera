`timescale 1ns / 1ps

module counter #(
    parameter COUNTER_WIDTH = 64
) (
    input   wire clk,
    input   wire rstn,
    output  reg [COUNTER_WIDTH - 1:0] count
    );

always @(posedge clk or negedge rstn) begin
    if (~rstn) begin
        count <= 0;
    end else begin
        count <= count + 1;
    end
end

endmodule
