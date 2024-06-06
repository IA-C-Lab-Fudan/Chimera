module top(
    input          clk,
    input          rstn,
    input  [6:0]   xsize16,
    input  [6:0]   ysize16,
    input          i_en,
    input  [7:0]   i_Y0,
    input  [7:0]   i_Y1,
    input  [7:0]   i_Y2,
    input  [7:0]   i_Y3,
    input  [7:0]   i_U0,
    input  [7:0]   i_U1,
    input  [7:0]   i_U2,
    input  [7:0]   i_U3,
    input  [7:0]   i_V0,
    input  [7:0]   i_V1,
    input  [7:0]   i_V2,
    input  [7:0]   i_V3,
    input          sequence_stop,
    output         sequence_busy,
    output         o_en,
    output         o_last,
    output [255:0] o_data
);

mpeg2encoder #(
    .XL                 ( 6                   ),   // determine the max horizontal pixel count.  4->256 pixels  5->512 pixels  6->1024 pixels  7->2048 pixels .
    .YL                 ( 6                   ),   // determine the max vertical   pixel count.  4->256 pixels  5->512 pixels  6->1024 pixels  7->2048 pixels .
    .VECTOR_LEVEL       ( 3                   ),
    .Q_LEVEL            ( 2                   )
) mpeg2encoder_i (
    .rstn               ( rstn          ),
    .clk                ( clk                 ),
    // Video sequence configuration interface.
    .i_xsize16          ( xsize16       ),
    .i_ysize16          ( ysize16       ),
    .i_pframes_count    ( 8'd47               ),
    // Video sequence input pixel stream interface. In each clock cycle, this interface can input 4 adjacent pixels in a row. Pixel format is YUV 4:4:4, the module will convert it to YUV 4:2:0, then compress it to MPEG2 stream.
    .i_en               ( i_en          ),
    .i_Y0               ( i_Y0          ),
    .i_Y1               ( i_Y1          ),
    .i_Y2               ( i_Y2          ),
    .i_Y3               ( i_Y3          ),
    .i_U0               ( i_U0          ),
    .i_U1               ( i_U0          ),
    .i_U2               ( i_U2          ),
    .i_U3               ( i_U2          ),
    .i_V0               ( i_V0          ),
    .i_V1               ( i_V0          ),
    .i_V2               ( i_V2          ),
    .i_V3               ( i_V2          ),
    // Video sequence control interface.
    .i_sequence_stop    ( sequence_stop ),
    .o_sequence_busy    ( sequence_busy ),
    // Video sequence output MPEG2 stream interface.
    .o_en               ( o_en          ),
    .o_last             ( o_last        ),
    .o_data             ( o_data        )
);

endmodule;