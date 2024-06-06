`timescale 1ns / 1ps

module input_buffer#(
    parameter IBUFFER_DEPTH   = 20,
    parameter TASK_SIZE       = 144
) (
    input wire                     clk,
    input wire                     rstn,
    input wire                     valid_in,
    input wire [TASK_SIZE - 1 : 0] data_in,

    input wire                     ready_in,
    output reg                     valid_out,
    output wire [TASK_SIZE - 1 : 0] data_out
    );

// input buffer

wire wea;
reg [7:0] addra = 'b0;
reg [7:0] addrb = 'b0;

assign wea = valid_in;

// width: 256  depth: 32
blk_mem_gen_0 ibuffer(
    .addra(iwrite_ptr),
    .clka(clk),
    .dina(data_in),
    .ena(1'b1),
    .wea(wea),
    .addrb(addrb),
    .clkb(clk),
    .doutb(data_out),
    .enb(1'b1)
);

reg [IBUFFER_DEPTH - 1 : 0] ibuffer_valid;
reg [7:0]   iwrite_ptr;
reg [7:0]   iread_ptr;

// read mechanism
parameter R_IDLE   = 2'b00;
parameter R_WDATA1 = 2'b01;
parameter R_WDATA2 = 2'b10;
parameter R_BUSY   = 2'b11;

reg [1:0] rstate;

reg [7:0] last_iread_ptr;


always @(posedge clk or negedge rstn) begin
    if (~rstn) begin
        rstate <= R_IDLE;
        valid_out <= 1'b0;
        iread_ptr <= 5'b00000;
        last_iread_ptr <= 5'b00000;
        iwrite_ptr <= 0;
        ibuffer_valid <= 'b0;
    end else begin
        if (valid_in) begin
            ibuffer_valid[iwrite_ptr +: 1] <= 1'b1;
            iwrite_ptr <= (iwrite_ptr == IBUFFER_DEPTH - 1) ? 0 : iwrite_ptr + 1;
        end

        case (rstate)
            R_IDLE:
            begin
                if (ibuffer_valid[iread_ptr] && ready_in) begin
                    rstate <= R_BUSY;
                    addrb <= iread_ptr;
                    
                    rstate <= R_WDATA1;
                end
            end
            
            R_WDATA1:
                rstate <= R_WDATA2;
                
            R_WDATA2:
            begin
                rstate <= R_BUSY;
                valid_out <= 1'b1; 
            end  
             
            R_BUSY:
            begin
                rstate <= R_IDLE;
                valid_out <= 1'b0;
                ibuffer_valid[iread_ptr] <= 1'b0;
                iread_ptr = (iread_ptr == IBUFFER_DEPTH - 1) ? 5'b00000 : iread_ptr + 5'b1;
            end
            default: 
                rstate <= R_IDLE;
        endcase
    end
end


endmodule
