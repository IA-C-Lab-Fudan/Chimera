`timescale 1ns / 1ps

module chimera_top #(
    parameter MAX_BATCH_THRESHOLD = 25,
    parameter AXI_DWIDTH          = 64,
    parameter TASK_SIZE           = 208,
    parameter RESULT_SIZE         = 336,
    parameter COUNTER_WIDTH       = 64,
    parameter IBUFFER_SIZE        = 100
) (
    input wire clk,
    input wire rstn,
    // AXI AW
    output wire s_axi_awready,
    input  wire s_axi_awvalid,
    input  wire [63:0] s_axi_awaddr,
    input  wire [7:0]  s_axi_awlen,
    input  wire [3:0]  s_axi_awid,

    // AXI W
    output wire s_axi_wready,
    input  wire s_axi_wvalid,
    input  wire s_axi_wlast,
    input  wire [AXI_DWIDTH - 1:0] s_axi_wdata,

    // AXI B
    input  wire s_axi_bready,
    output wire s_axi_bvalid,
    output wire [3:0] s_axi_bid,
    output wire [1:0] s_axi_bresp,

    // AXI AR
    output wire s_axi_arready,
    input  wire s_axi_arvalid,
    input  wire [63:0] s_axi_araddr,
    input  wire [7:0]  s_axi_arlen,
    input  wire [3:0]  s_axi_arid,

    // AXI R
    input  wire s_axi_rready,
    output wire s_axi_rvalid,
    output wire s_axi_rlast,
    output wire [AXI_DWIDTH - 1:0] s_axi_rdata,
    output wire [3:0]  s_axi_rid,
    output wire [1:0]  s_axi_rresp
    );

wire [TASK_SIZE - 1 : 0]      axi2ibufferData;
wire                          axi2ibufferValid;
wire [RESULT_SIZE - 1 : 0]    obuffer2axiData;
wire                          obuffer2axiValid;
wire                          obuffer2axiRemaining;
wire                          axi2obufferReady;
  
wire                          module2ibufferReady;
wire                          ibuffer2ModuleValid;
wire [TASK_SIZE - 1 : 0]      ibuffer2ModuleData;
  
wire                          module2obufferValid;
wire [RESULT_SIZE - 1 : 0]    module2obufferData;

wire [COUNTER_WIDTH - 1 : 0]  runtimeCount;

wire user_rstn;

axi_wrapper #(
    MAX_BATCH_THRESHOLD,
    AXI_DWIDTH,
    TASK_SIZE,
    RESULT_SIZE,
    COUNTER_WIDTH
) axi_wrapper_instance (
    .clk(clk),
    .rstn(rstn),

    .s_axi_awready(s_axi_awready),
    .s_axi_awvalid(s_axi_awvalid),
    .s_axi_awaddr(s_axi_awaddr),
    .s_axi_awlen(s_axi_awlen),
    .s_axi_awid(s_axi_awid),
    
    .s_axi_wready(s_axi_wready),
    .s_axi_wvalid(s_axi_wvalid),
    .s_axi_wlast(s_axi_wlast),
    .s_axi_wdata(s_axi_wdata),

    .s_axi_bready(s_axi_bready),
    .s_axi_bvalid(s_axi_bvalid),
    .s_axi_bid(s_axi_bid),
    .s_axi_bresp(s_axi_bresp),

    .s_axi_arready(s_axi_arready),
    .s_axi_arvalid(s_axi_arvalid),
    .s_axi_araddr(s_axi_araddr),
    .s_axi_arlen(s_axi_arlen),
    .s_axi_arid(s_axi_arid),

    .s_axi_rready(s_axi_rready),
    .s_axi_rvalid(s_axi_rvalid),
    .s_axi_rlast(s_axi_rlast),
    .s_axi_rdata(s_axi_rdata),
    .s_axi_rid(s_axi_rid),
    .s_axi_rresp(s_axi_rresp),

    .wbuffer_valid(axi2ibufferValid),
    .wbuffer_data(axi2ibufferData),

    .obuffer_valid(obuffer2axiValid),
    .obuffer_data(obuffer2axiData),
    .obuffer_ready(axi2obufferReady),
    .obuffer_remaining(obuffer2axiRemaining),
    
    .user_rstn(user_rstn),

    .counter_in(runtimeCount)
);

input_buffer #(
    IBUFFER_SIZE,
    TASK_SIZE
) input_buffer_instance (
    .clk(clk),
    .rstn(user_rstn),
    .valid_in(axi2ibufferValid),
    .data_in(axi2ibufferData),

    .ready_in(module2ibufferReady),
    .valid_out(ibuffer2ModuleValid),
    .data_out(ibuffer2ModuleData)
);

output_buffer #(
    RESULT_SIZE,
    IBUFFER_SIZE,
    COUNTER_WIDTH
) output_buffer_instance (
    .clk(clk),
    .rstn(user_rstn),
    .valid_out(obuffer2axiValid),
    .data_out(obuffer2axiData),
    .ready_in(axi2obufferReady),
    .counter_in(runtimeCount),

    .valid_in(module2obufferValid),
    .data_in(module2obufferData),
    .remaining(obuffer2axiRemaining)
);

counter #(
    COUNTER_WIDTH
) counter_instance (
    .clk(clk),
    .rstn(user_rstn),
    .count(runtimeCount)
);

ModuleIFS_Mpeg2 #(
    TASK_SIZE,
    RESULT_SIZE,
    COUNTER_WIDTH
) ModuleIFS_Mpeg2_instance (
    .clk(clk),
    .rstn(user_rstn),

    .valid_in(ibuffer2ModuleValid),
    .data_in(ibuffer2ModuleData),
    .ready_out(module2ibufferReady),

    .counter_in(runtimeCount),

    .valid_out(module2obufferValid),
    .data_out(module2obufferData)
);

endmodule