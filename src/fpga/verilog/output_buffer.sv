`timescale 1ns / 1ps

module output_buffer#(
    parameter RESULT_SIZE       = 88,
    parameter IBUFFER_DEPTH     = 20,
    parameter COUNTER_WIDTH       = 64
) (
    input  wire                         clk,
    input  wire                         rstn,
    input  wire [RESULT_SIZE - 1 : 0]   data_in,
    input  wire                         valid_in,

    input  wire                         ready_in,
    input  wire [COUNTER_WIDTH - 1 : 0] counter_in,
    output wire [RESULT_SIZE - 1 : 0]   data_out,
    output wire                         valid_out,
    output wire                         remaining
    );

wire wea;
wire [7:0] addra;
wire [7:0] addrb;

wire [RESULT_SIZE - 1 : 0] write_data;

assign write_data[15:0]  = data_in[15:0];
assign write_data[79:16] = (counter_in >= data_in[79:16]) ? (counter_in - data_in[79:16]) : ((2**COUNTER_WIDTH - data_in[79:16]) + counter_in);
assign write_data[RESULT_SIZE - 1 : 80] = data_in[RESULT_SIZE - 1 : 80];


blk_mem_gen_1 obuffer(
    .addra(addra),
    .clka(clk),
    .dina(write_data),
    .ena(1'b1),
    .wea(wea),
    .addrb(addrb),
    .clkb(clk),
    .doutb(data_out),
    .enb(1'b1)
);

reg [IBUFFER_DEPTH - 1 : 0] obuffer_valid;
reg [7:0]   iwrite_ptr;
reg [7:0]   iwrite_last_ptr;
reg [7:0]   iread_ptr;

parameter W_IDLE = 2'b00;
parameter W_NOP  = 2'b01;
parameter W_BUSY = 2'b10;

reg [1:0] wstate;

assign wea = (valid_in && wstate == W_IDLE) ? 1 : 0;
assign addra = iwrite_ptr;
assign addrb = iread_ptr;

parameter R_IDLE = 2'b00;
parameter R_NOP  = 2'b01;
parameter R_BUSY = 2'b10;

reg [1:0] rstate;

assign valid_out = (rstate == R_BUSY);
assign remaining = (obuffer_valid[iread_ptr]);


always @(posedge clk or negedge rstn)
begin
    if (~rstn) begin
        rstate <= R_IDLE;
        iread_ptr  <= 'b0;
        wstate <= W_IDLE;
        iwrite_ptr <= 0;
        iwrite_last_ptr <= 0;
        obuffer_valid <= 0;
    end else begin
        // write mechanism
        case (wstate)
            W_IDLE:
                if (valid_in) begin
                    iwrite_ptr <= (iwrite_ptr == IBUFFER_DEPTH - 1) ? 5'b00000 : iwrite_ptr + 5'b1;
                    iwrite_last_ptr <= iwrite_ptr;
                    wstate <= W_NOP;
                end
            W_NOP:
                begin
                    wstate <= W_BUSY;
                end
            W_BUSY:
                begin
                    obuffer_valid[iwrite_last_ptr] <= 1'b1;
                    wstate <= W_IDLE;
                end
            default:
                wstate <= W_IDLE;
        endcase    
        
        // read mechanism
        case (rstate)
            R_IDLE:
            begin
                if (obuffer_valid[iread_ptr] && ready_in)
                begin
                    rstate <= R_NOP;
                end
            end
            R_NOP:
            begin
                rstate <= R_BUSY;
            end
            R_BUSY:
            begin
                obuffer_valid[iread_ptr] = 1'b0;
                iread_ptr <= (iread_ptr == IBUFFER_DEPTH - 1) ? 5'b00000 : iread_ptr + 5'b1;
                rstate <= R_IDLE;
            end
            default:
                rstate <= R_IDLE;
        endcase
    end
end
endmodule