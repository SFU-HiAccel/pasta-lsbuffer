module laneswitch #(
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 6,
  parameter ADDR_RANGE = 64
)(
  input  wire clk,
  input  wire reset,
  input  wire switch,
  output wire active,
  output wire fault,

  // wires to 2-port memory
  output reg  [ADDR_WIDTH-1:0]  laneswitch_mem_address0,
  output reg  [DATA_WIDTH-1:0]  laneswitch_mem_d0,
  input  wire [DATA_WIDTH-1:0]  laneswitch_mem_q0,
  output reg                    laneswitch_mem_ce0,
  output reg                    laneswitch_mem_we0,
  output reg  [ADDR_WIDTH-1:0]  laneswitch_mem_address1,
  output reg  [DATA_WIDTH-1:0]  laneswitch_mem_d1,
  input  wire [DATA_WIDTH-1:0]  laneswitch_mem_q1,
  output reg                    laneswitch_mem_ce1,
  output reg                    laneswitch_mem_we1,

  // wires from lane0 and lane1
  input  wire [ADDR_WIDTH-1:0]  laneswitch_lane0_address0,
  input  wire [DATA_WIDTH-1:0]  laneswitch_lane0_d0,
  output wire [DATA_WIDTH-1:0]  laneswitch_lane0_q0,
  input  wire                   laneswitch_lane0_ce0,
  input  wire                   laneswitch_lane0_we0,
  input  wire [ADDR_WIDTH-1:0]  laneswitch_lane0_address1,
  input  wire [DATA_WIDTH-1:0]  laneswitch_lane0_d1,
  output wire [DATA_WIDTH-1:0]  laneswitch_lane0_q1,
  input  wire                   laneswitch_lane0_ce1,
  input  wire                   laneswitch_lane0_we1,
  input  wire [ADDR_WIDTH-1:0]  laneswitch_lane1_address0,
  input  wire [DATA_WIDTH-1:0]  laneswitch_lane1_d0,
  output wire [DATA_WIDTH-1:0]  laneswitch_lane1_q0,
  input  wire                   laneswitch_lane1_ce0,
  input  wire                   laneswitch_lane1_we0,
  input  wire [ADDR_WIDTH-1:0]  laneswitch_lane1_address1,
  input  wire [DATA_WIDTH-1:0]  laneswitch_lane1_d1,
  output wire [DATA_WIDTH-1:0]  laneswitch_lane1_q1,
  input  wire                   laneswitch_lane1_ce1,
  input  wire                   laneswitch_lane1_we1
);

assign active = laneswitch_mem_ce0 | laneswitch_mem_ce1;  // active transaction
assign fault  = switch & active;                          // not expecting to switch while a transaction is active


assign laneswitch_lane0_q0  = (switch) ? {DATA_WIDTH{1'b0}} : laneswitch_mem_q0;
assign laneswitch_lane0_q1  = (switch) ? {DATA_WIDTH{1'b0}} : laneswitch_mem_q1;
assign laneswitch_lane1_q0  = (switch) ? laneswitch_mem_q0 : {DATA_WIDTH{1'b0}};
assign laneswitch_lane1_q1  = (switch) ? laneswitch_mem_q1 : {DATA_WIDTH{1'b0}};

always@(posedge clk)
begin
  if(switch)
  begin
    // bind both ports to lane 1
    laneswitch_mem_address0 <= laneswitch_lane1_address0;
    laneswitch_mem_d0       <= laneswitch_lane1_d0;
    laneswitch_mem_ce0      <= laneswitch_lane1_ce0;
    laneswitch_mem_we0      <= laneswitch_lane1_we0;
    laneswitch_mem_address1 <= laneswitch_lane1_address1;
    laneswitch_mem_d1       <= laneswitch_lane1_d1;
    laneswitch_mem_ce1      <= laneswitch_lane1_ce1;
    laneswitch_mem_we1      <= laneswitch_lane1_we1;
    // disable lane 0 chip-enables. Other stuff can be left dangling
    // laneswitch_lane0_ce0 <= 0;
    // laneswitch_lane0_ce1 <= 0;
  end
  else    // this is the default state
  begin
    // bind both ports to lane 0
    laneswitch_mem_address0 <= laneswitch_lane0_address0;
    laneswitch_mem_d0       <= laneswitch_lane0_d0;
    laneswitch_mem_ce0      <= laneswitch_lane0_ce0;
    laneswitch_mem_we0      <= laneswitch_lane0_we0;
    laneswitch_mem_address1 <= laneswitch_lane0_address1;
    laneswitch_mem_d1       <= laneswitch_lane0_d1;
    laneswitch_mem_ce1      <= laneswitch_lane0_ce1;
    laneswitch_mem_we1      <= laneswitch_lane0_we1;
    // disable lanenel 1 chip-enables. Other stuff can be left dangling
    // laneswitch_lane1_ce0 <= 0;
    // laneswitch_lane1_ce1 <= 0;
  end
end

endmodule