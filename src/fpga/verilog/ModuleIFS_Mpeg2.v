`timescale 1ns / 1ps

module ModuleIFS_Mpeg2 #(
    parameter TASK_SIZE       = 144,
    parameter RESULT_SIZE     = 88,
    parameter COUNTER_WIDTH   = 64
) (
    input wire                      clk,
    input wire                      rstn,
    input  wire [TASK_SIZE - 1 : 0]     data_in,
    input  wire                         valid_in,
    output wire                          ready_out,

    input  wire [COUNTER_WIDTH - 1 : 0] counter_in,

    output reg [RESULT_SIZE - 1 : 0]    data_out,
    output reg                          valid_out
    );

reg          mpeg2_rstn;

wire         mpeg2_sequence_busy;
reg          mpeg2_sequence_stop;

reg  [  6:0] mpeg2_xsize16;
reg  [  6:0] mpeg2_ysize16;

reg          mpeg2_i_en;
reg [7:0]    mpeg2_i_Y0;
reg [7:0]    mpeg2_i_Y1;
reg [7:0]    mpeg2_i_Y2;
reg [7:0]    mpeg2_i_Y3;
reg [7:0]    mpeg2_i_U0;
reg [7:0]    mpeg2_i_U2;
reg [7:0]    mpeg2_i_V0;
reg [7:0]    mpeg2_i_V2;

wire         mpeg2_o_en;
wire         mpeg2_o_last;
wire [255:0] mpeg2_o_data;

reg [14:0]   mpeg2_o_head;
reg [14:0]   mpeg2_o_tail;
reg          mpeg2_o_over;

wire wea;
wire [255:0] doutb;
reg recved_stop;
reg stop_signal;

assign wea            = (mpeg2_o_en || (recved_stop && ~mpeg2_sequence_busy));

//assign data_out[0]    = 1'b1;
reg [63:0] input_cnt;
reg [63:0] output_cnt;

blk_mem_gen_3 out_buf(
    .addra(mpeg2_o_head),
    .clka(clk),
    .dina(mpeg2_o_data),
    .ena(1'b1),
    .wea(wea),
    .addrb(mpeg2_o_tail),
    .clkb(clk),
    .doutb(doutb),
    .enb(1'b1)
);

assign ready_out = 1;

always @(posedge clk or negedge rstn) begin
    if (~rstn) begin
        mpeg2_rstn           <= 0;
        mpeg2_sequence_stop  <= 0;

        mpeg2_xsize16 <= 0;
        mpeg2_ysize16 <= 0;

        mpeg2_i_en    <= 0;
        mpeg2_i_Y0    <= 0;
        mpeg2_i_Y1    <= 0;
        mpeg2_i_Y2    <= 0;
        mpeg2_i_Y3    <= 0;
        mpeg2_i_U0    <= 0;
        mpeg2_i_U2    <= 0;
        mpeg2_i_V0    <= 0;
        mpeg2_i_V2    <= 0;

        mpeg2_xsize16 <= 0;
        mpeg2_ysize16 <= 0;
        
        input_cnt  <= 0;
                
    end else begin
        if (valid_in) begin
            mpeg2_i_en <= 0;
            mpeg2_sequence_stop <= 0;

            if (data_in[143:80] == 32'h00000000) begin
                mpeg2_rstn          <= data_in[144];
                mpeg2_sequence_stop <= data_in[145];
            end else if (data_in[143:80] == 32'h00000008) begin
                mpeg2_xsize16 <= data_in[150:144];
                mpeg2_ysize16 <= data_in[184:176];
            end else if (data_in[143:80] >= 32'h01000000) begin
                mpeg2_i_en <= 1;
                { mpeg2_i_V2, mpeg2_i_Y3, mpeg2_i_U2, mpeg2_i_Y2,
                mpeg2_i_V0, mpeg2_i_Y1, mpeg2_i_U0, mpeg2_i_Y0 } <= data_in[207:144];
                
                input_cnt <= input_cnt + 1;
            end else if (data_in[143:80] == 32'h00000010) begin
                input_cnt <= 0;
            end
        end else begin
            mpeg2_i_en <= 0;
        end
    end
end

always @(posedge clk or negedge rstn) begin
    if (~rstn) begin
        mpeg2_o_head <= 0;
        mpeg2_o_over <= 0;
        output_cnt <= 0;
    end else begin
        if (valid_in && data_in[143:80] == 32'h00000010) begin
            mpeg2_o_head <= 0;
            mpeg2_o_over <= 0;
            output_cnt <= 0;
        end
        
        if (mpeg2_o_en) begin
            if ((mpeg2_o_head == 32767 ? 0 : mpeg2_o_head + 1) != mpeg2_o_tail) begin  // ring buffer logic
//                out_buf[mpeg2_o_head] <= mpeg2_o_data;
                mpeg2_o_head <= (mpeg2_o_head == 32767 ? 0 : mpeg2_o_head + 1);
            end else begin
                mpeg2_o_over <= 1;
                mpeg2_o_head <= (mpeg2_o_head == 32767 ? 0 : mpeg2_o_head + 1);
            end
            output_cnt <= output_cnt + 1;
        end
    end
end

parameter R_IDLE = 2'b00;
parameter R_WAIT = 2'b01;
parameter R_STOP = 2'b10;

reg [1:0] rstate;

always @(posedge clk or negedge rstn) begin
    if (~rstn) begin
        valid_out <= 0;
        mpeg2_o_tail <= 0;
        rstate <= R_IDLE;
        data_out <= 0;
        recved_stop <= 0;
        stop_signal <= 0;
    end else begin
        if (valid_in && data_in[143:80] == 32'h00000010) begin
            mpeg2_o_tail <= 0;
        end

        if (valid_in && data_in[143:80] == 32'h00000000 && data_in[145] == 1) begin
            recved_stop <= 1;
        end

        if (recved_stop && ~mpeg2_sequence_busy) begin
            stop_signal <= 1;
            recved_stop <= 0;
        end
        
        case (rstate)
        R_IDLE:
        begin
            valid_out     <= 0;
            if (mpeg2_o_head != mpeg2_o_tail) begin
                rstate <= R_WAIT;
            end else begin
                if (stop_signal) begin
                    rstate <= R_STOP;
                    stop_signal <= 0;
                end
            end
        end
        
        R_WAIT:
        begin
            valid_out     <= 1;
            data_out[0] <= 1'b1;
            data_out[1] <= 1'b0;
            data_out[79:2] <= 0;
            data_out[335:80] <= doutb;
            mpeg2_o_tail <= (mpeg2_o_tail == 32767 ? 0 : mpeg2_o_tail + 1);
            rstate <= R_IDLE;
        end

        R_STOP:
        begin
            valid_out <= 1;
            data_out[0] <= 1'b1;
            data_out[1] <=  1'b1;
            data_out[79:2] <= 0;
            data_out[335:80] <= 0;
            rstate <= R_IDLE;
        end
        
        endcase
    end
end


mpeg2encoder #(
    .XL                 ( 6                   ),   // determine the max horizontal pixel count.  4->256 pixels  5->512 pixels  6->1024 pixels  7->2048 pixels .
    .YL                 ( 6                   ),   // determine the max vertical   pixel count.  4->256 pixels  5->512 pixels  6->1024 pixels  7->2048 pixels .
    .VECTOR_LEVEL       ( 3                   ),
    .Q_LEVEL            ( 2                   )
) mpeg2encoder_i (
    .rstn               ( mpeg2_rstn          ),
    .clk                ( clk                 ),
    // Video sequence configuration interface.
    .i_xsize16          ( mpeg2_xsize16       ),
    .i_ysize16          ( mpeg2_ysize16       ),
    .i_pframes_count    ( 8'd47               ),
    // Video sequence input pixel stream interface. In each clock cycle, this interface can input 4 adjacent pixels in a row. Pixel format is YUV 4:4:4, the module will convert it to YUV 4:2:0, then compress it to MPEG2 stream.
    .i_en               ( mpeg2_i_en          ),
    .i_Y0               ( mpeg2_i_Y0          ),
    .i_Y1               ( mpeg2_i_Y1          ),
    .i_Y2               ( mpeg2_i_Y2          ),
    .i_Y3               ( mpeg2_i_Y3          ),
    .i_U0               ( mpeg2_i_U0          ),
    .i_U1               ( mpeg2_i_U0          ),
    .i_U2               ( mpeg2_i_U2          ),
    .i_U3               ( mpeg2_i_U2          ),
    .i_V0               ( mpeg2_i_V0          ),
    .i_V1               ( mpeg2_i_V0          ),
    .i_V2               ( mpeg2_i_V2          ),
    .i_V3               ( mpeg2_i_V2          ),
    // Video sequence control interface.
    .i_sequence_stop    ( mpeg2_sequence_stop ),
    .o_sequence_busy    ( mpeg2_sequence_busy ),
    // Video sequence output MPEG2 stream interface.
    .o_en               ( mpeg2_o_en          ),
    .o_last             ( mpeg2_o_last        ),
    .o_data             ( mpeg2_o_data        )
);

endmodule
