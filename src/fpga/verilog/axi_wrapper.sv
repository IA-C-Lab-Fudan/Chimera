`timescale 1ns / 1ps

module axi_wrapper #(
    parameter MAX_BATCH_THRESHOLD = 3,
    parameter AXI_DWIDTH          = 128,
    parameter TASK_SIZE           = 144,
    parameter RESULT_SIZE         = 88,
    parameter COUNTER_WIDTH       = 64,
    parameter WBUFFER_WIDTH       = 16+TASK_SIZE*MAX_BATCH_THRESHOLD,
    parameter RBUFFER_WIDTH       = 16+RESULT_SIZE*MAX_BATCH_THRESHOLD,
    parameter MAX_RBEAT           = (RBUFFER_WIDTH-1)/AXI_DWIDTH+1
)(
    input wire rstn,
    input wire clk,
    // AXI AW interface
    output wire  s_axi_awready,
    input  wire s_axi_awvalid,
    input  wire [63:0] s_axi_awaddr,
    input  wire [7:0]  s_axi_awlen,
    input  wire [3:0]  s_axi_awid,
    
    // AXI W interface
    output wire  s_axi_wready,
    input  wire s_axi_wvalid,
    input  wire s_axi_wlast,
    input  wire [AXI_DWIDTH - 1:0] s_axi_wdata,
    
    // AXI B interface
    input  wire s_axi_bready,
    output wire s_axi_bvalid,
    output wire [3:0] s_axi_bid,
    output wire [1:0] s_axi_bresp,
    
    // AXI AR interface
    output wire s_axi_arready,
    input  wire s_axi_arvalid,
    input  wire [63:0] s_axi_araddr,
    input  wire [7:0]  s_axi_arlen,
    input  wire [3:0]  s_axi_arid,
    
    // AXI R interface
    input  wire s_axi_rready,
    output wire s_axi_rvalid,
    output wire s_axi_rlast,
    output reg [AXI_DWIDTH - 1:0] s_axi_rdata,
    output wire [3:0]  s_axi_rid,
    output wire [1:0]  s_axi_rresp,

    // ibuffer interface
    output reg wbuffer_valid,
    output reg [TASK_SIZE-1 : 0] wbuffer_data,

    // obuffer interface
    input  wire obuffer_valid,
    input  wire [RESULT_SIZE - 1 : 0] obuffer_data,
    output wire  obuffer_ready,
    input  wire obuffer_remaining,
    
    // user_rstn
    output wire user_rstn,

    input [COUNTER_WIDTH-1 : 0] counter_in
    );
    
// AXI WRITE state machine

parameter W_IDLE    = 3'b000;
parameter W_COLLECT = 3'b001;
parameter W_DECODE  = 3'b010;
parameter W_RESP    = 3'b011;
parameter W_THROUGH = 3'b100;

reg [2:0] wstate;

reg [3:0] wid = 4'b0;
reg [7:0] wcount = 8'b0;
// how about awaddr?

reg [7:0] batch_thread;

assign s_axi_awready = (wstate == W_IDLE);
assign s_axi_wready  = (wstate == W_COLLECT || wstate == W_THROUGH);
assign s_axi_bvalid  = (wstate == W_RESP);
assign s_axi_bid     = wid;
assign s_axi_bresp   = 2'b0;

assign user_rstn = ~(~rstn || (s_axi_awvalid && s_axi_awaddr == 64'h1000));


reg  [1:0] read_mode;

parameter M_DEFAULT = 2'b00;
parameter M_POLL    = 2'b01;
parameter M_WAIT    = 2'b10;

// task decode field
reg  [WBUFFER_WIDTH-1:0] wbuffer;
wire [3:0]   wbatch = wbuffer[11:8];
reg  [3:0]   wbatch_ptr;
reg  [7:0]   total_len;

reg  [7:0]   write_counter;

reg [63:0] data_plane_addr;

always @ (posedge  clk or negedge rstn) begin
    if (~rstn) begin
        wstate  <= W_IDLE;
        wid     <= 4'b0;
        wcount  <= 8'b0;
        wbuffer <= 0;
        wbatch_ptr <= 4'b0;
        wbuffer_valid <= 1'b0;
        wbuffer_data <= 0;
        write_counter <= 0;
        total_len <= 0;
        data_plane_addr <= 0;
        batch_thread <= 0;
        
        read_mode <= M_WAIT;
    end else begin
        case (wstate)
            W_IDLE:
            begin
                write_counter <= 0;
                wbuffer_data <= 0;
                wbuffer_valid <= 1'b0;
                wbuffer <= 0;
                wbatch_ptr <= 4'b0;
                
                if (s_axi_awvalid) begin
                    wid <= s_axi_awid;
                    wcount <= s_axi_awlen + 1'b1;
                    total_len <= s_axi_awlen + 1'b1;
                    
                    if (s_axi_awaddr >= 64'h1000000) begin
                        data_plane_addr <= s_axi_awaddr;
                        wstate <= W_THROUGH;
                    end else begin
                        wstate <= W_COLLECT;
                    end
                end
            end
            W_THROUGH:
            begin
                if (s_axi_wvalid) begin
                    if (write_counter < total_len) begin
                        wbuffer_data[0] <= 1;
                        wbuffer_data[2] <= 1;
                        wbuffer_data[143:80]  <= data_plane_addr;
                        wbuffer_data[207:144] <= s_axi_wdata;
                        wbuffer_valid <= 1'b1;
                        
                        data_plane_addr <= data_plane_addr + 8;
                    
                        write_counter <= write_counter + 1'b1;
                        wcount <= wcount - 8'd1;
                    end
                    
                    if (wcount == 8'd1 || s_axi_wlast) begin
                        write_counter <= 8'b0;
                        wstate <= W_RESP;
                    end
                end
            end
            W_COLLECT:
                begin
                    if (s_axi_wvalid) begin
                        if (write_counter < total_len) begin
                            wbuffer[(total_len - wcount) * AXI_DWIDTH +: AXI_DWIDTH] <= s_axi_wdata;
                            write_counter <= write_counter + 1'b1;
                            wcount <= wcount - 8'd1;
                        end
                        
                        if (wcount == 8'd1 || s_axi_wlast) begin
                            write_counter <= 8'b0;
                            if (s_axi_awaddr == 64'h1000) begin
                                wstate <= W_RESP;
                                wid <= 4'b0;
                                wcount <= 8'b0;
                                wbuffer <= 0;
                                wbatch_ptr <= 4'b0;
                                wbuffer_valid <= 1'b0;
                                wbuffer_data <= 0;
                                write_counter <= 0;
                                total_len <= 0;
                            end else if (s_axi_awaddr == 64'h1008) begin
                                read_mode <= M_POLL;
                                wstate <= W_RESP;
                            end else if (s_axi_awaddr == 64'h1010) begin
                                if (wbuffer[7:0] >= MAX_BATCH_THRESHOLD) begin
                                    batch_thread <= MAX_BATCH_THRESHOLD;
                                end else begin
                                    batch_thread <= wbuffer[7:0];
                                end
                                
                                wstate <= W_RESP;
                            end
                            else
                                wstate <= W_DECODE;
                        end
                    end
                end
            W_DECODE:
            begin
                if (wbatch == 0) begin
                    wstate <= W_RESP;
                end else begin
                    if (wbatch_ptr == wbatch - 4'b1)
                        wstate <= W_RESP;
                    wbuffer_data[15:0]  <= wbuffer[16 + wbatch_ptr * TASK_SIZE +: 16];
                    wbuffer_data[79:16] <= counter_in;
                    wbuffer_data[TASK_SIZE-1 : 80] <= wbuffer[16 + wbatch_ptr * TASK_SIZE+80 +: TASK_SIZE-80];
                    wbuffer_valid <= 1'b1;
                    wbatch_ptr <= (wbatch_ptr == wbatch - 4'b1) ? 4'b0000 : wbatch_ptr + 4'b1;
                end
            end
            W_RESP:
            begin
                if (s_axi_bready) begin
                    wstate <= W_IDLE;
                end
                wbuffer_valid <= 1'b0;
            end
            default:
                wstate <= W_IDLE;
        endcase
    end    
end


parameter R_IDLE = 2'b00;
parameter R_BUSY = 2'b01;
parameter R_COLLECT = 2'b10;
parameter R_NOP = 2'b11;

reg [1:0] rstate;

reg [3:0] rid;
reg [7:0] rcount;

reg  [RBUFFER_WIDTH-1:0] rbuffer;
reg  [3:0]   rbatch_ptr;
reg  [7:0]   rbeat_ptr;
reg  [7:0]   read_counter;
reg  [7:0]   read_toal_len;

reg  [1:0] test_pointer;

assign s_axi_arready = (rstate == R_IDLE);
assign s_axi_rvalid  = (rstate == R_BUSY);
assign s_axi_rlast   = (rstate == R_BUSY) && (rcount == 8'd0);
assign s_axi_rid     = rid;
assign s_axi_rresp   = 2'b0;
assign obuffer_ready = (rstate == R_COLLECT && obuffer_remaining && rbatch_ptr < batch_thread);

always @(posedge clk or negedge rstn) begin
    if (~rstn) begin
        rstate  <= R_IDLE;
        rid     <= 4'b0;
        rcount  <= 8'b0;
        rbuffer <= 0;
        s_axi_rdata <= 0;
        rbatch_ptr <= 0;
        rbeat_ptr <= 0;
        read_counter <= 0;
        read_toal_len <= 0;
        test_pointer <= 0;
    end else begin
        if (wstate == W_IDLE && s_axi_awvalid && s_axi_awaddr == 64'h1000) begin
            rstate  <= R_IDLE;
            rid     <= 4'b0;
            rcount  <= 8'b0;
            rbuffer <= 0;
            s_axi_rdata <= 0;
            rbatch_ptr <= 0;
            rbeat_ptr <= 0;
            read_counter <= 0;
            read_toal_len <= 0;
        end
        
        case (rstate) 
            R_IDLE:
                if (s_axi_arvalid) begin
                    rid    <= s_axi_arid;
                    rcount <= s_axi_arlen + 1;
                    read_toal_len <= s_axi_arlen + 1;

                    read_counter <= 0;
                    s_axi_rdata <= 0;

                    // note
                    if (rbeat_ptr == 0) begin
                        rbuffer <= 0;
                        rbatch_ptr <= 0;
                        test_pointer <= 1;
                        rstate <= R_COLLECT;
                    end else begin
                        test_pointer <= 2;
                        rstate <= R_NOP;
                    end
                end
            R_COLLECT:
            begin
                if (obuffer_valid) begin
                    rbuffer[16 + rbatch_ptr * RESULT_SIZE +: RESULT_SIZE] <= obuffer_data;
                    rbatch_ptr <= rbatch_ptr + 1'b1;
                end else begin
                    if (obuffer_remaining && rbatch_ptr < batch_thread) begin
                        rstate <= R_COLLECT;
                    end else begin
                        if (rbatch_ptr == 4'b0) begin
                            // rbuffer[1] = 1'b1;   // warning: this is a special rule
                            if ((read_mode == M_POLL) || (wstate == W_IDLE && s_axi_awvalid && s_axi_awaddr == 64'h2000)) begin
                                rbuffer[0] <= 1'b0;
                                rbuffer[15:8] <= 8'b0;
                                rstate <= R_NOP;
                            end else begin
                                rstate <= R_COLLECT;
                            end
                        end else begin
                            rbuffer[0] <= 1'b1;
                            rbuffer[15:8] <= rbatch_ptr;
                        
                            rbatch_ptr <= 4'b0;
                            rstate <= R_NOP;
                        end
                    end
                end
            
            end
            R_NOP:
                if (s_axi_rready) begin
                    s_axi_rdata <= rbuffer[rbeat_ptr * AXI_DWIDTH +: AXI_DWIDTH];
                    read_counter <= read_counter + 1'b1;
                    rbeat_ptr <= rbeat_ptr + 1'b1;
                    rcount <= rcount - 8'd1;
                    rstate <= R_BUSY;
                end
            R_BUSY:
                if (s_axi_rready) begin
                    if (rcount == 8'd0) begin
                        rstate <= R_IDLE;
                        // rbuffer <= 0;
                        s_axi_rdata <= 'b0;
                        read_counter <= 1'b0;

                        if (rbeat_ptr * AXI_DWIDTH >= RBUFFER_WIDTH) begin
                            rbeat_ptr <= 0;
                            rbuffer <= 0;
                        end
                    end else begin
                        if (read_counter < read_toal_len) begin
                            if (rcount == 8'd1)
                                s_axi_rdata <= rbuffer[rbeat_ptr * AXI_DWIDTH +: (RBUFFER_WIDTH-(MAX_RBEAT-1)*AXI_DWIDTH)];
                            else
                                s_axi_rdata <= rbuffer[rbeat_ptr * AXI_DWIDTH +: AXI_DWIDTH];
                            read_counter <= read_counter + 1'b1;
                        end else begin
                            s_axi_rdata <= 0;
                        end
                        rbeat_ptr <= rbeat_ptr + 1'b1;
                        rcount <= rcount - 8'd1;
                    end
                end
            default:
                rstate <= R_IDLE;
        endcase
    end
end

endmodule
