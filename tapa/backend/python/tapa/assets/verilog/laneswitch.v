module laneswitch #(
  parameter DATA_WIDTH = 32,
  parameter ADDR_WIDTH = 6,
  parameter ADDR_RANGE = 64
)(
  input  wire clk,
  input  wire reset,
  input  wire req0,
  input  wire req1,

  // wires to 2-port memory
  output wire  [ADDR_WIDTH-1:0] laneswitch_mem_address0,
  output wire  [DATA_WIDTH-1:0] laneswitch_mem_d0,
  input  wire  [DATA_WIDTH-1:0] laneswitch_mem_q0,
  output wire                   laneswitch_mem_ce0,
  output wire                   laneswitch_mem_we0,
  output wire  [ADDR_WIDTH-1:0] laneswitch_mem_address1,
  output wire  [DATA_WIDTH-1:0] laneswitch_mem_d1,
  input  wire  [DATA_WIDTH-1:0] laneswitch_mem_q1,
  output wire                   laneswitch_mem_ce1,
  output wire                   laneswitch_mem_we1,

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

  reg switch_dly = 1'b0;
  reg lane = 1'b0;
  reg [1:0] reset_delay_counter = 2'b00;
  localparam STOP_COUNTER = 2'b11;
  localparam RESET_DONE = 2'b10;
  localparam TRIGGER = 2'b01;
  localparam ROUTE = 2'b10;

  // `reqX` signals are mutually exclusive, since buffer.n_sections is always 1.
  // Hence, `switch` is simply a notification of any switch request.
  assign switch     = req1 | req0;
  assign reset_exit = (reset_delay_counter == RESET_DONE);      // reset exit --> negative edges of reset
  assign trigger    = (reset_exit) | (switch & ~switch_dly);    // detect positive edges of `switch` signal

  // route memcore outputs to appropriate lane (0/1)
  assign laneswitch_lane0_q0  = (lane) ? {DATA_WIDTH{1'bZ}} : (laneswitch_mem_q0);
  assign laneswitch_lane0_q1  = (lane) ? {DATA_WIDTH{1'bZ}} : (laneswitch_mem_q1);
  assign laneswitch_lane1_q0  = (lane) ? (laneswitch_mem_q0) : {DATA_WIDTH{1'bZ}};
  assign laneswitch_lane1_q1  = (lane) ? (laneswitch_mem_q1) : {DATA_WIDTH{1'bZ}};
  // route memcore inputs to appropriate lane (0/1)
  assign laneswitch_mem_address0 = (lane) ? (laneswitch_lane1_address0) : (laneswitch_lane0_address0);
  assign laneswitch_mem_d0       = (lane) ? (laneswitch_lane1_d0)       : (laneswitch_lane0_d0);
  assign laneswitch_mem_ce0      = (lane) ? (laneswitch_lane1_ce0)      : (laneswitch_lane0_ce0);
  assign laneswitch_mem_we0      = (lane) ? (laneswitch_lane1_we0)      : (laneswitch_lane0_we0);
  assign laneswitch_mem_address1 = (lane) ? (laneswitch_lane1_address1) : (laneswitch_lane0_address1);
  assign laneswitch_mem_d1       = (lane) ? (laneswitch_lane1_d1)       : (laneswitch_lane0_d1);
  assign laneswitch_mem_ce1      = (lane) ? (laneswitch_lane1_ce1)      : (laneswitch_lane0_ce1);
  assign laneswitch_mem_we1      = (lane) ? (laneswitch_lane1_we1)      : (laneswitch_lane0_we1);

  // counter to notify reset-exit to pulse `trigger` once after reset-deassertion
  always @(posedge clk) begin
    if(reset) begin
      lane <= 1'b0;             // reset lane to producer
      reset_delay_counter <= 0; // reset `reset_delay_counter`
    end else begin
      if(trigger)
        lane <= ~lane;          // change lane
      if(reset_delay_counter != STOP_COUNTER) begin   // count upto 3 and stop
        reset_delay_counter <= reset_delay_counter + 1;
      end
    end
  end

  // get a delayed version of `switch` signal to detect trigger edges
  always @(posedge clk) begin
    if(reset) begin
      switch_dly <= ~switch;
    end else begin
      switch_dly <= switch;
    end
  end

endmodule
