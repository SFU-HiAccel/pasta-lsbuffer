import os
import re
from math import ceil, log2

from pyverilog.ast_code_generator import codegen
from pyverilog.vparser import ast, parser
import tapa.util

import logging
_logger = logging.getLogger().getChild(__name__)

###############################################################################
###############################################################################
### LOCAL HELPER FUNTIONS
###############################################################################

# generate non-blocking assignment
def generate_non_blocking_assignment(left, right):
  return ast.NonblockingSubstitution(ast.Lvalue(ast.Identifier(left)),
                                     ast.Rvalue(ast.Identifier(right)))


# generate an always block given a sensitivity type, name of signal and
# statements
def generate_always_block(sens_type, sens_signal, statements):
  return ast.Always(
      ast.SensList((ast.Sens(ast.Identifier(sens_signal), sens_type),)),
      ast.Block(statements))


# generate a Verilog file from the module object
def module_to_file(module, filename):
  output = codegen.ASTCodeGenerator().visit(module)
  with open(filename, "w") as fd:
    fd.write("`default_nettype none\n")
    fd.write(output)
    fd.write("`default_nettype wire\n")


# generate string output from multiple modules
def modules_to_str(modules):
  generator = codegen.ASTCodeGenerator()
  output = ''
  for module in modules:
    output += generator.visit(module)
    output += '\n'
  return output


# count total items in dims array
def count_dims(factors):
  total_items = 1
  for index in factors:
    total_items *= index
  return total_items


# takes dimensions array and creates a generator that gives
# strings. for example, given [2, 3] as input, it generates the
# following strings:
# 0_0_, 0_1_, 0_2_, 1_0_, 1_1_, 1_2_
def index_generator(factors):
  no_factors = len(factors)
  indices = [0] * len(factors)
  total_items = count_dims(factors)
  for n in range(total_items):
    to_yield = []
    for i, factor in enumerate(factors):
      if factors[i] != 1:
        to_yield.append(indices[i])
    res = "_".join([str(i) for i in to_yield])
    if res == "":
      yield ""
    else:
      yield res + "_"
    for i in range(no_factors - 1, -1, -1):
      if indices[i] == (factors[i] - 1):
        indices[i] = 0
      else:
        indices[i] += 1
        break

# shorthand for returning ast.Width given a string width
def astwidth(width: str):
  return ast.Width(ast.Minus(ast.Identifier(width), ast.IntConst('1')),
                    ast.IntConst('0'))

# generates an input/output module wire of a certain width
# and direction
def generate_io_wire(name, direction, width=None):
  kwargs = {"name": name}
  if width is not None:
    kwargs["width"] = astwidth(width)
  if direction == "input":
    cls = ast.Input
  else:
    cls = ast.Output
  return ast.Ioport(cls(**kwargs), second=ast.Wire(**kwargs))


# generates an input/output module reg of a certain width
# and direction
def generate_io_reg(name, direction, width=None):
  kwargs = {"name": name}
  if width is not None:
    kwargs["width"] = astwidth(width)
  if direction == "input":
    cls = ast.Input
  else:
    cls = ast.Output
  return ast.Ioport(cls(**kwargs), second=ast.Reg(**kwargs))


# generate a wire or a register of a particular width and length
def generate_decl(name, decl_type, width=None, length=None):
  if decl_type == "reg":
    cls = ast.Reg
  else:
    cls = ast.Wire
  if width is not None:
    width = astwidth(width)
  if length is not None:
    #length = ast.Dimensions((ast.Length(ast.Minus(ast.Identifier(length), ast.IntConst('1')), ast.IntConst('0')),))
    length = ast.Dimensions((ast.Length(ast.Identifier(length),
                                        ast.IntConst('0')),))
  return ast.Decl((cls(name, width=width, dimensions=length),))


# generates a module parameter given its name and value
def generate_const_parameter(name, value):
  return ast.Parameter(name, ast.Rvalue(ast.IntConst(str(value))))


# syntactic sugar to generate io ports from tuples of some parameters
def generate_ports_from_info(info):
  ports = []
  for name, direction, width, nettype in info:
    if nettype == "wire":
      ports.append(generate_io_wire(name, direction, width))
    else:
      ports.append(generate_io_reg(name, direction, width))
  return ports


# given a certain prefix, address width and data width, generate
# ap_memory ports
def generate_ap_memory_interface(prefix, address_width, data_width, memports=1):
  info = []
  if (memports == 1):
    info.extend([(f"{prefix}address0", "input", address_width, "wire"),
            (f"{prefix}d0", "input", data_width, "wire"),
            (f"{prefix}q0", "output", data_width, "wire"),
            (f"{prefix}ce0", "input", None, "wire"),
            (f"{prefix}we0", "input", None, "wire")])
  else:
    for memport in range(memports):
      info.extend([(f"{prefix}address{str(memport)}", "input", address_width, "wire"),
              (f"{prefix}d{str(memport)}", "input", data_width, "wire"),
              (f"{prefix}q{str(memport)}", "output", data_width, "wire"),
              (f"{prefix}ce{str(memport)}", "input", None, "wire"),
              (f"{prefix}we{str(memport)}", "input", None, "wire")])
  return generate_ports_from_info(info)


# generate ap_memory interface ports but with producer or consumer after the prefix
def generate_wire_ap_memory_interface(prefix, address_width, data_width,
                                      direction):
  if direction == "producer":
    info = [(f"{prefix}producer_address", "input", address_width, "wire"),
            (f"{prefix}producer_ce", "input", None, "wire"),
            (f"{prefix}producer_d", "input", data_width, "wire"),
            (f"{prefix}producer_we", "input", None, "wire"),
            (f"{prefix}producer_q", "output", data_width, "wire")]
  elif direction == "consumer":
    info = [(f"{prefix}consumer_address", "output", address_width, "wire"),
            (f"{prefix}consumer_ce", "output", None, "wire"),
            (f"{prefix}consumer_d", "output", data_width, "wire"),
            (f"{prefix}consumer_we", "output", None, "wire"),
            (f"{prefix}consumer_q", "input", data_width, "wire")]
  return generate_ports_from_info(info)


# generate ap_memory interface with producer/consumer after prefix but
# with output signals registered
def generate_registered_ap_memory_interface(prefix, address_width, data_width,
                                            direction):
  if direction == "producer":
    info = [(f"{prefix}producer_address", "input", address_width, "wire"),
            (f"{prefix}producer_ce", "input", None, "wire"),
            (f"{prefix}producer_d", "input", data_width, "wire"),
            (f"{prefix}producer_we", "input", None, "wire"),
            (f"{prefix}producer_q", "output", data_width, "reg")]
  elif direction == "consumer":
    info = [(f"{prefix}consumer_address", "output", address_width, "reg"),
            (f"{prefix}consumer_ce", "output", None, "reg"),
            (f"{prefix}consumer_d", "output", data_width, "reg"),
            (f"{prefix}consumer_we", "output", None, "reg"),
            (f"{prefix}consumer_q", "input", data_width, "wire")]
  return generate_ports_from_info(info)


# given an indices array, address_width and data_width, generate both the
# producer and consumer side memory ports
def generate_buffer_memory_ports(address_width, data_width, indices, hybrid=False):
  ports = []
  for i in indices():
    for lane in ['producer', 'consumer']:
      # tag: SYNTAX_PORT_BUFFER
      io_port_name = f"buffer_core{i}{str(lane)}_"
      if(hybrid):
        ports.extend(
          generate_ap_memory_interface(io_port_name, address_width, data_width, memports=2))
      else:
        ports.extend(
          generate_ap_memory_interface(io_port_name, address_width, data_width, memports=1))
  return ports


# expand on the laneswitch-memcores-wiring info to get the actual AST
def generate_lsmcw_wires(prefix, address_width, data_width):
  info = []
  for memport in range(2):   # laneswitch is always implemented with 2 ports
    memport = str(memport)
    info.extend([(f"{prefix}_address{memport}",'wire',address_width),
                  (f"{prefix}_d{memport}",'wire',data_width),
                  (f"{prefix}_q{memport}",'wire',data_width),
                  (f"{prefix}_ce{memport}",'wire',None),
                  (f"{prefix}_we{memport}",'wire',None),])
  decls = []
  for itemname, itemtype, itemwidth in info:
    decls.append(generate_decl(itemname, itemtype, itemwidth))
  return decls
                  

# generate the laneswitch-memcores-wiring declarations to be placed inside the buffer
def generate_lsmcw_decls(address_width, data_width, indices):
  decls = []
  for i in indices():
    decl_name = f"locsig_i{i}laneswitches_memcores"
    decls.extend(generate_lsmcw_wires(decl_name, address_width, data_width))
  return decls


def generate_memcores_memory_ports(address_width, data_width, indices):
  ports = []
  for i in indices():
    # tag: SYNTAX_PORT_MEMCORES
    io_port_name = f"memcores_i{i}"
    ports.extend(
        generate_ap_memory_interface(io_port_name, address_width, data_width, memports=2))
  return ports

# generate IOs for the laneswitch module
def generate_laneswitches_ports(address_width, data_width, indices):
  ports = []
  # `indices` is a (lambda) function object that is a lazy iterable.
  # It has to be expanded by `list()` before the length can be calculated.
  # `count_dims` could have been used here, but the list is already being
  # created slowly by `indices`, so...
  listindices = list(indices())
  _logger.debug("Generating %d laneswitches ports" % len(listindices))
  
  # generate tracking-fifo ports required for switching logic
  info = []
  for lane in range(2):
    info.extend([(f"fifo_to_lane{str(lane)}_read", "input", None, "wire")])
  ports.extend(generate_ports_from_info(info))

  # generate memory lanes (coming from memcores and going into laneswitch modules)
  for i in listindices:
    io_port_name = f"laneswitches_i{i}mem_"
    ports.extend(
        generate_ap_memory_interface(io_port_name, address_width, data_width, memports=2))
    
  # generate i instances, each with 2 lanes, each lane with 2 mem ports 
  for i in listindices:
    for lane in range(2):
      io_port_name = f"laneswitches_i{i}lane{str(lane)}_"
      ports.extend(
          generate_ap_memory_interface(io_port_name, address_width, data_width, memports=2))
    
  return ports


###############################################################################
###############################################################################
### INSTANCE GENERATION
###############################################################################

# TODO: below two functions seem to be identical, no idea why @moazin wrote it with two
# names. Figure that out


# generates an instantiation of a module given:
# module_name, instance_name and two list of pairs of arguments:
# 1. parameter argument name -> argument
# 2. port_name -> port_arg
def generate_instance(module_name, instance_name, param_arg_pairs,
                      port_io_pairs):
  params_list = (ast.ParamArg(paramname=paramname,
                              argname=ast.Identifier(argname))
                 for paramname, argname in param_arg_pairs)
  portlist = (ast.PortArg(portname=portname, argname=ast.Identifier(argname))
              for portname, argname in port_io_pairs)
  instances = (ast.Instance(module_name,
                            instance_name,
                            portlist,
                            parameterlist=None),)
  instance_list = ast.InstanceList(module_name, params_list, instances)
  return instance_list


# generates an instantiation of a module given:
# module_name, instance_name and two list of pairs of arguments:
# 1. parameter argument name -> argument
# 2. port_name -> port_arg
def generate_instance_with_custom_ports(module_name, instance_name,
                                        param_arg_pairs, port_io_list):
  params_list = (ast.ParamArg(paramname=paramname,
                              argname=ast.Identifier(argname))
                 for paramname, argname in param_arg_pairs)
  portlist = (ast.PortArg(portname=portname, argname=argname)
              for portname, argname in port_io_list)
  instances = (ast.Instance(module_name,
                            instance_name,
                            portlist,
                            parameterlist=None),)
  instance_list = ast.InstanceList(module_name, params_list, instances)
  return instance_list

###############################################################################
###############################################################################
### MEMCORE GENERATION
###############################################################################


# generates some memcore instance with prefix given already, note that
# param arguments are passed in from the parent module with certain names
# user of the function must be careful that they are valid
def generate_memcore_instance(module_name, instance_name, io_prefix):
  params_list = [('DATA_WIDTH', 'DATA_WIDTH'), ('ADDRESS_WIDTH', 'ADDR_WIDTH'),
                 ('ADDRESS_RANGE', 'ADDR_RANGE'), ('IS_SIMPLE', 'IS_SIMPLE')]
  ports = [
      ('clk', 'clk'),
      ('reset', 'reset'),
      ('address0',  f'{io_prefix}address0'),
      ('ce0',       f'{io_prefix}ce0'),
      ('we0',       f'{io_prefix}we0'),
      ('q0',        f'{io_prefix}q0'),
      ('d0',        f'{io_prefix}d0'),
      ('address1',  f'{io_prefix}address1'),
      ('ce1',       f'{io_prefix}ce1'),
      ('we1',       f'{io_prefix}we1'),
      ('q1',        f'{io_prefix}q1'),
      ('d1',        f'{io_prefix}d1'),
  ]
  return generate_instance(module_name, instance_name, params_list, ports)


# generate a URAM or BRAM memcore instance while given a dims array
def generate_memcore_instances(dims, ram_style):
  items = []
  for integer in dims():
    module_name = 'memcore_uram' if ram_style == "URAM" else "memcore_bram"
    instance_name = f'core_{integer}'
    io_prefix = f'memcores_i{integer}'
    items.append(
        generate_memcore_instance(module_name, instance_name, io_prefix))
  return items


# generate a memcores module given the parameter values, dims and ram_style
def generate_memcores_module(module_name, data_width, address_width,
                             address_range, dims, ram_style):
  parameters = [('DATA_WIDTH', data_width), ('ADDR_WIDTH', address_width),
                ('ADDR_RANGE', address_range), ('IS_SIMPLE', 0)]
  params = ast.Paramlist(
      [generate_const_parameter(k, v) for k, v in parameters])
  clk = ast.Ioport(ast.Input('clk'), second=ast.Wire('clk'))
  reset = ast.Ioport(ast.Input('reset'), second=ast.Wire('reset'))
  port_list = [clk, reset]
  port_list.extend(
      generate_memcores_memory_ports('ADDR_WIDTH', 'DATA_WIDTH',
                                   lambda: index_generator(dims)))
  ports = ast.Portlist(port_list)
  items = generate_memcore_instances(lambda: index_generator(dims), ram_style)
  return ast.ModuleDef(module_name, params, ports, items)


###############################################################################
###############################################################################
### LANESWITCHES GENERATION
###############################################################################

# generate the laneswitch instance
def generate_laneswitch_instance(module_name, instance_name, io_prefix, index):
  _logger.debug("laneswitch instance")
  params_list = [('DATA_WIDTH', 'DATA_WIDTH'), ('ADDR_WIDTH', 'ADDR_WIDTH'),
                 ('ADDR_RANGE', 'ADDR_RANGE')]
  ports = ([
      ('clk', 'clk'),
      ('reset', 'reset'),
      # io_prefix has the form "_iN_", where N is index. [-2:][0] selects 'N' from the string
      ('switch', f'switchbar[{str(index)}]'),])
  for memport in range(2):
    # tag: SYNTAX_PORT_LANESWITCHES
    ports.extend([
        (f'laneswitch_mem_address{str(memport)}',  f'laneswitches{io_prefix}mem_address{str(memport)}'),
        (f'laneswitch_mem_ce{str(memport)}',       f'laneswitches{io_prefix}mem_ce{str(memport)}'),
        (f'laneswitch_mem_we{str(memport)}',       f'laneswitches{io_prefix}mem_we{str(memport)}'),
        (f'laneswitch_mem_q{str(memport)}',        f'laneswitches{io_prefix}mem_q{str(memport)}'),
        (f'laneswitch_mem_d{str(memport)}',        f'laneswitches{io_prefix}mem_d{str(memport)}')])
  for memport in range(2):
    # tag: SYNTAX_PORT_LANESWITCHES
    ports.extend([
        (f'laneswitch_lane0_address{str(memport)}',  f'laneswitches{io_prefix}lane0_address{str(memport)}'),
        (f'laneswitch_lane0_we{str(memport)}',       f'laneswitches{io_prefix}lane0_we{str(memport)}'),
        (f'laneswitch_lane0_ce{str(memport)}',       f'laneswitches{io_prefix}lane0_ce{str(memport)}'),
        (f'laneswitch_lane0_d{str(memport)}',        f'laneswitches{io_prefix}lane0_d{str(memport)}'),
        (f'laneswitch_lane0_q{str(memport)}',        f'laneswitches{io_prefix}lane0_q{str(memport)}')])
  for memport in range(2):
    # tag: SYNTAX_PORT_LANESWITCHES
    ports.extend([
        (f'laneswitch_lane1_address{str(memport)}',  f'laneswitches{io_prefix}lane1_address{str(memport)}'),
        (f'laneswitch_lane1_we{str(memport)}',       f'laneswitches{io_prefix}lane1_we{str(memport)}'),
        (f'laneswitch_lane1_ce{str(memport)}',       f'laneswitches{io_prefix}lane1_ce{str(memport)}'),
        (f'laneswitch_lane1_d{str(memport)}',        f'laneswitches{io_prefix}lane1_d{str(memport)}'),
        (f'laneswitch_lane1_q{str(memport)}',        f'laneswitches{io_prefix}lane1_q{str(memport)}')])
  return generate_instance(module_name, instance_name, params_list, ports)


# generate a laneswitch instance given a dims array
def generate_laneswitch_instances(dims):
  _logger.debug("generating laneswitch instances")
  items = []
  count = 0
  for index in dims():
    module_name = "laneswitch"
    instance_name = f"laneswitch_{index}"
    io_prefix = f'_i{index}'
    items.append(
        generate_laneswitch_instance(module_name, instance_name, io_prefix, count))
    count+=1
  return items


# generate a laneswitch module for this specific memcores module
def generate_laneswitches_module(module_name, data_width, address_width,
                             address_range, dims):
  _logger.debug("generating laneswitches")
  parameters = [('DATA_WIDTH', data_width), ('ADDR_WIDTH', address_width),
                ('ADDR_RANGE', address_range)]
  params = ast.Paramlist(
      [generate_const_parameter(k, v) for k, v in parameters])
  clk = ast.Ioport(ast.Input('clk'), second=ast.Wire('clk'))
  reset = ast.Ioport(ast.Input('reset'), second=ast.Wire('reset'))
  port_list = [clk, reset]
  port_list.extend(
      generate_laneswitches_ports('ADDR_WIDTH', 'DATA_WIDTH', lambda: index_generator(dims)))
  ports = ast.Portlist(port_list)
  items = generate_laneswitch_instances(lambda: index_generator(dims))

  ### BEGIN DECLARATIONS ###
  # switchbar register
  dimswidth = str(count_dims(dims))
  total_switches = str(count_dims(dims))
  decl_reg_switchbar = generate_decl("switchbar", "reg", dimswidth)
  items.append(decl_reg_switchbar)

  # FSM related declarations
  thisstate = ast.Identifier('this_state')
  nextstate = ast.Identifier('next_state')
  state_LANE0 = ast.Identifier('LANE0')
  state_LANE1 = ast.Identifier('LANE1')
  if_reset_cond = ast.Identifier('reset')
  items.append(ast.Decl([ast.Reg('this_state',ast.Value('[1:0]'),value=ast.Rvalue(ast.Value("2\'b00")))]))
  items.append(ast.Decl([ast.Reg('next_state',ast.Value('[1:0]'),value=ast.Rvalue(ast.Value("2\'b01")))]))
  items.append(ast.Decl([ast.Localparam('LANE0',ast.Rvalue(ast.Value("2\'b00")))]))
  items.append(ast.Decl([ast.Localparam('LANE1',ast.Rvalue(ast.Value("2\'b01")))]))
  ### END DECLARATIONS ###

  ### BEGIN FSM ###
  # create the cases' if-else statements
  if_fifolane1read_cond = ast.Identifier('fifo_to_lane1_read')
  if_fifolane1read_true = ast.NonblockingSubstitution(
                            ast.Lvalue(ast.Identifier('switchbar')),
                            ast.Rvalue(ast.Value(f'{total_switches}\'b'+f'1'*int(total_switches))))
  stateswitch_LANE1 = ast.NonblockingSubstitution(ast.Lvalue(nextstate),
                                                  ast.Rvalue(state_LANE1))
  if_fifolane1read = ast.IfStatement(cond=if_fifolane1read_cond,
                                     true_statement=ast.Block([if_fifolane1read_true, stateswitch_LANE1]),
                                     false_statement=None)
  if_fifolane0read_cond = ast.Identifier('fifo_to_lane0_read')
  if_fifolane0read_true = ast.NonblockingSubstitution(
                            ast.Lvalue(ast.Identifier('switchbar')),
                            ast.Rvalue(ast.Value(f'{total_switches}\'b'+f'0'*int(total_switches))))
  stateswitch_LANE0 = ast.NonblockingSubstitution(ast.Lvalue(nextstate),
                                                  ast.Rvalue(state_LANE0))
  if_fifolane0read = ast.IfStatement(cond=if_fifolane0read_cond,
                                     true_statement=ast.Block([if_fifolane0read_true, stateswitch_LANE0]),
                                     false_statement=None)
  default_switchbar_assignment = ast.NonblockingSubstitution(
                            ast.Lvalue(ast.Identifier('switchbar')),
                            ast.Rvalue(ast.Value(f'{total_switches}\'b'+f'X'*int(total_switches))))
  # create the individual case statements
  case_LANE0_statements = ast.Block([if_fifolane1read])
  case_LANE1_statements = ast.Block([if_fifolane0read])
  case_LANE0 = ast.Case([state_LANE0],case_LANE0_statements)
  case_LANE1 = ast.Case([state_LANE1],case_LANE1_statements)
  case_LANE1_statements = ast.Block([if_fifolane0read])
  case_default_statements = ast.Block([default_switchbar_assignment])
  case_default = ast.Case([ast.Identifier('default')], case_default_statements)
  # create main FSM
  caseblock_mainfsm = ast.CaseStatement(thisstate,([case_LANE0, case_LANE1, case_default]))
  always_switchlogic_caseblock = ast.Block([caseblock_mainfsm])
  always_switchlogic_if_reset_statement = ast.NonblockingSubstitution(
                                            ast.Lvalue(thisstate),
                                            ast.Rvalue(state_LANE0))
  always_switchlogic_if_statement = ast.IfStatement(cond=if_reset_cond,
                                    true_statement=ast.Block([always_switchlogic_if_reset_statement]),
                                    false_statement=always_switchlogic_caseblock)
  always_switchlogic_block = ast.Block([always_switchlogic_if_statement])
  
  # make the sensitivity list for the FSM. Next-State should change only on any of the requested reads.
  always_switchlogic_senslist = ast.SensList([ast.Sens(ast.Identifier('fifo_to_lane0_read'), type='posedge'),
                                              ast.Sens(ast.Identifier('fifo_to_lane1_read'), type='posedge')])
  always_switchlogic = ast.Always(
                          sens_list=always_switchlogic_senslist,
                          statement=always_switchlogic_block)
  items.append(always_switchlogic)

  # create FSM driver (this_state <= next_state)
  always_fsmdrive_statement = ast.NonblockingSubstitution(ast.Lvalue(thisstate),
                                                          ast.Rvalue(nextstate))
  ### BEGIN RESET ###
  # create the reset condition (next_state <= LANE0)
  if_reset_true = ast.NonblockingSubstitution(
                           ast.Lvalue(ast.Identifier('next_state')),
                           ast.Rvalue(state_LANE0))
  bootstrap_switchbar = ast.NonblockingSubstitution(ast.Lvalue(ast.Identifier('switchbar')),
                                                    ast.Rvalue(ast.Value(f'{total_switches}\'b'+f'0'*int(total_switches))))
  resetlogic_statement = ast.Block([if_reset_true, bootstrap_switchbar])
  ### END RESET ###

  always_fsmdrive_block = ast.IfStatement(cond=if_reset_cond,
                                    true_statement=resetlogic_statement,
                                    false_statement=ast.Block([always_fsmdrive_statement]))
  always_fsmdrive_senslist = ast.Sens(ast.Identifier('clk'), type='posedge')
  always_fsmdrive = ast.Always(
                          sens_list=always_fsmdrive_senslist,
                          statement=ast.Block([always_fsmdrive_block]))
  items.append(always_fsmdrive)
  ### END FSM ###

  return ast.ModuleDef(module_name, params, ports, items)

###############################################################################
###############################################################################
### FIFO GENERATION
###############################################################################

# generates fifo io ports given a prefix and fifo data width
def generate_fifo_port(prefix, fifo_data_width):
  info = [
      (f'{prefix}_full_n', "output", None, "wire"),
      (f'{prefix}_write_ce', "input", None, "wire"),
      (f'{prefix}_write', "input", None, "wire"),
      (f'{prefix}_din', "input", fifo_data_width, "wire"),
      (f'{prefix}_empty_n', "output", None, "wire"),
      (f'{prefix}_read_ce', "input", None, "wire"),
      (f'{prefix}_read', "input", None, "wire"),
      (f'{prefix}_dout', "output", fifo_data_width, "wire"),
  ]
  return generate_ports_from_info(info)


# generate FIFOs for ping-pong buffer module
def generate_double_buffer_fifo_ports():
  ports = []
  ports.extend(generate_fifo_port("fifo_free_buffers", "FIFO_DATA_WIDTH"))
  ports.extend(generate_fifo_port("fifo_occupied_buffers", "FIFO_DATA_WIDTH"))
  return ports


# generate a FIFO instance given module_name, instance_name, prefix, widths
# depth and level
# level indicates the level of pipelining and when that is present, init_length
# is expected as a parameter named `FREE_FIFO_RESET_LENGTH`
def generate_fifo_instance(module_name,
                           instance_name,
                           prefix,
                           data_width,
                           addr_width,
                           depth,
                           level=None):
  params_list = [('DATA_WIDTH', data_width), ('ADDR_WIDTH', addr_width),
                 ('DEPTH', depth)]
  if level is not None:
    params_list.append(('LEVEL', level))
  if module_name == "initialized_fifo" or module_name == "initialized_relay_station":
    params_list.append(('INIT_LENGTH', 'FREE_FIFO_RESET_LENGTH'))
  ports = [
      ('clk', 'clk'),
      ('reset', 'reset'),
      ('if_full_n', f'{prefix}_full_n'),
      ('if_write_ce', f'{prefix}_write_ce'),
      ('if_write', f'{prefix}_write'),
      ('if_din', f'{prefix}_din'),
      ('if_empty_n', f'{prefix}_empty_n'),
      ('if_read_ce', f'{prefix}_read_ce'),
      ('if_read', f'{prefix}_read'),
      ('if_dout', f'{prefix}_dout'),
  ]
  return generate_instance(module_name, instance_name, params_list, ports)

###############################################################################
###############################################################################
### BUFFER GENERATION
###############################################################################


# generates a memcores instance inside final buffer
def generate_memcores_instance(module_name, instance_name, dims, level=None, hybrid=False):
  params_list = [('DATA_WIDTH', 'MEMORY_DATA_WIDTH'),
                 ('ADDR_WIDTH', 'MEMORY_ADDR_WIDTH'),
                 ('ADDR_RANGE', 'MEMORY_ADDR_RANGE'),
                 ('IS_SIMPLE', 'IS_SIMPLE')]
  if level is not None:
    params_list.append(('LEVEL', level))
  ports = [
      ('clk', 'clk'),
      ('reset', 'reset'),
  ]
  # connect memcores instance with laneswitch
  if(hybrid): # hybrid buffer. Connect `memcores` to `laneswitches`.
    for prefix in index_generator(dims):
      for memport in range(2):
        # tag: SYNTAX_PORT_MEMCORES, SYNTAX_DECL_LANESWITCHES
        ports.extend([
          (f'memcores_i{prefix}address{str(memport)}', f'locsig_i{prefix}laneswitches_memcores_address{str(memport)}'),
          (f'memcores_i{prefix}we{str(memport)}',      f'locsig_i{prefix}laneswitches_memcores_we{str(memport)}'),
          (f'memcores_i{prefix}ce{str(memport)}',      f'locsig_i{prefix}laneswitches_memcores_ce{str(memport)}'),
          (f'memcores_i{prefix}d{str(memport)}',       f'locsig_i{prefix}laneswitches_memcores_d{str(memport)}'),
          (f'memcores_i{prefix}q{str(memport)}',       f'locsig_i{prefix}laneswitches_memcores_q{str(memport)}'),
      ])
  else: # standard buffer. Connect `memcores` with prod/cons (1 port each)
    for prefix in index_generator(dims):
      # tag: SYNTAX_PORT_MEMCORES, SYNTAX_PORT_BUFFER
      ports.extend([
        (f'memcores_i{prefix}address0', f'buffer_core{prefix}producer_address0'),
        (f'memcores_i{prefix}we0',      f'buffer_core{prefix}producer_we0'),
        (f'memcores_i{prefix}ce0',      f'buffer_core{prefix}producer_ce0'),
        (f'memcores_i{prefix}d0',       f'buffer_core{prefix}producer_d0'),
        (f'memcores_i{prefix}q0',       f'buffer_core{prefix}producer_q0'),
        (f'memcores_i{prefix}address1', f'buffer_core{prefix}consumer_address0'),
        (f'memcores_i{prefix}we1',      f'buffer_core{prefix}consumer_we0'),
        (f'memcores_i{prefix}ce1',      f'buffer_core{prefix}consumer_ce0'),
        (f'memcores_i{prefix}d1',       f'buffer_core{prefix}consumer_d0'),
        (f'memcores_i{prefix}q1',       f'buffer_core{prefix}consumer_q0'),
      ])
  return generate_instance(module_name, instance_name, params_list, ports)


# generates a laneswitches instance inside final buffer
def generate_laneswitches_instance(module_name, instance_name, dims, level=None):
  params_list = [('DATA_WIDTH', 'MEMORY_DATA_WIDTH'),
                 ('ADDR_WIDTH', 'MEMORY_ADDR_WIDTH'),
                 ('ADDR_RANGE', 'MEMORY_ADDR_RANGE')]
  if level is not None:
    params_list.append(('LEVEL', level))
  ports = [
      ('clk', 'clk'),
      ('reset', 'reset'),
  ]
  ports.extend([(f'fifo_to_lane0_read',f'fifo_free_buffers_read')])
  ports.extend([(f'fifo_to_lane1_read',f'fifo_occupied_buffers_read')])
  for prefix in index_generator(dims):
    for memport in range(2):
      # tag: SYNTAX_PORT_LANESWITCHES, SYNTAX_DECL_LANESWITCHES
      ports.extend([
          (f'laneswitches_i{prefix}mem_address{str(memport)}',  f'locsig_i{prefix}laneswitches_memcores_address{str(memport)}'),
          (f'laneswitches_i{prefix}mem_we{str(memport)}',       f'locsig_i{prefix}laneswitches_memcores_we{str(memport)}'),
          (f'laneswitches_i{prefix}mem_ce{str(memport)}',       f'locsig_i{prefix}laneswitches_memcores_ce{str(memport)}'),
          (f'laneswitches_i{prefix}mem_d{str(memport)}',        f'locsig_i{prefix}laneswitches_memcores_d{str(memport)}'),
          (f'laneswitches_i{prefix}mem_q{str(memport)}',        f'locsig_i{prefix}laneswitches_memcores_q{str(memport)}')])
    for memport in range(2):
      # tag: SYNTAX_PORT_BUFFER, SYNTAX_PORT_LANESWITCHES
      ports.extend([
          (f'laneswitches_i{prefix}lane0_address{str(memport)}', f'buffer_core{prefix}producer_address{str(memport)}'),
          (f'laneswitches_i{prefix}lane0_we{str(memport)}',      f'buffer_core{prefix}producer_we{str(memport)}'),
          (f'laneswitches_i{prefix}lane0_ce{str(memport)}',      f'buffer_core{prefix}producer_ce{str(memport)}'),
          (f'laneswitches_i{prefix}lane0_d{str(memport)}',       f'buffer_core{prefix}producer_d{str(memport)}'),
          (f'laneswitches_i{prefix}lane0_q{str(memport)}',       f'buffer_core{prefix}producer_q{str(memport)}')])
    for memport in range(2):
      # tag: SYNTAX_PORT_BUFFER, SYNTAX_PORT_LANESWITCHES
      ports.extend([
          (f'laneswitches_i{prefix}lane1_address{str(memport)}', f'buffer_core{prefix}consumer_address{str(memport)}'),
          (f'laneswitches_i{prefix}lane1_we{str(memport)}',      f'buffer_core{prefix}consumer_we{str(memport)}'),
          (f'laneswitches_i{prefix}lane1_ce{str(memport)}',      f'buffer_core{prefix}consumer_ce{str(memport)}'),
          (f'laneswitches_i{prefix}lane1_d{str(memport)}',       f'buffer_core{prefix}consumer_d{str(memport)}'),
          (f'laneswitches_i{prefix}lane1_q{str(memport)}',       f'buffer_core{prefix}consumer_q{str(memport)}')])
  return generate_instance(module_name, instance_name, params_list, ports)


# generate ping-pong buffer module given parameter values, dims
def generate_double_buffer_module(module_name, data_width, address_width,
                                  address_range, no_partitions, dims,
                                  memcores_name, laneswitches_name=None):
  fifo_depth = no_partitions
  fifo_addr_width = max(1, ceil(log2(no_partitions)))
  parameters = [('MEMORY_DATA_WIDTH', data_width),
                ('MEMORY_ADDR_WIDTH', address_width),
                ('MEMORY_ADDR_RANGE', address_range), ('FIFO_DATA_WIDTH', 32),
                ('FIFO_ADDR_WIDTH', fifo_addr_width),
                ('FIFO_DEPTH', no_partitions),
                ('FREE_FIFO_RESET_LENGTH', no_partitions), ('IS_SIMPLE', 0)]
  params = ast.Paramlist(
      [generate_const_parameter(k, v) for k, v in parameters])
  clk = generate_io_wire("clk", "input")
  reset = generate_io_wire("reset", "input")
  ports_list = [clk, reset]
  ports_list.extend(generate_double_buffer_fifo_ports())

  # specify hybrid buffer mode for buffer ports
  # standard mode => 1 port  each for prod/cons
  # hybrid mode   => 2 ports each for prod/cons
  if(no_partitions == 1):
    ports_list.extend(
        generate_buffer_memory_ports('MEMORY_ADDR_WIDTH',
                                     'MEMORY_DATA_WIDTH',
                                     lambda: index_generator(dims),
                                     hybrid=True))
  else:
    ports_list.extend(
        generate_buffer_memory_ports('MEMORY_ADDR_WIDTH',
                                     'MEMORY_DATA_WIDTH',
                                     lambda: index_generator(dims),
                                     hybrid=False))
  ports = ast.Portlist(ports_list)
  items = []
  # add declarations 
  if(no_partitions == 1): # add lsmcw (laneswitch-memcores-wires) in hybrid buffer mode
    items.extend(generate_lsmcw_decls('MEMORY_ADDR_WIDTH',
                                             'MEMORY_DATA_WIDTH',
                                             lambda: index_generator(dims)))
  # add instances
  if(no_partitions == 1): # only add laneswitch in hybrid buffer mode
    items.append(generate_laneswitches_instance(laneswitches_name, 'laneswitches', dims))
    items.append(generate_memcores_instance(memcores_name, 'memcores', dims, None, True))
  else:
    items.append(generate_memcores_instance(memcores_name, 'memcores', dims, None, False))
  items.append(
      generate_fifo_instance('fifo', 'occupied_buffers',
                             'fifo_occupied_buffers', 'FIFO_DATA_WIDTH',
                             'FIFO_ADDR_WIDTH', 'FIFO_DEPTH'))
  items.append(
      generate_fifo_instance('initialized_fifo', 'free_buffers',
                             'fifo_free_buffers', 'FIFO_DATA_WIDTH',
                             'FIFO_ADDR_WIDTH', 'FIFO_DEPTH'))
  
  return ast.ModuleDef(module_name, params, ports, items)


# geneate ping-pong buffer module with pipelining
def generate_relay_double_buffer_module(module_name, data_width, address_width,
                                        address_range, no_partitions, dims,
                                        memcores_name, default_level):
  fifo_depth = no_partitions
  fifo_addr_width = max(1, ceil(log2(no_partitions)))
  parameters = [('MEMORY_DATA_WIDTH', data_width),
                ('MEMORY_ADDR_WIDTH', address_width),
                ('MEMORY_ADDR_RANGE', address_range), ('FIFO_DATA_WIDTH', 32),
                ('FIFO_ADDR_WIDTH', fifo_addr_width),
                ('FIFO_DEPTH', no_partitions),
                ('FREE_FIFO_RESET_LENGTH', no_partitions),
                ('LEVEL', default_level), ('IS_SIMPLE', 0)]
  params = ast.Paramlist(
      [generate_const_parameter(k, v) for k, v in parameters])
  clk = generate_io_wire("clk", "input")
  reset = generate_io_wire("reset", "input")
  ports_list = [clk, reset]
  ports_list.extend(generate_double_buffer_fifo_ports())
  ports_list.extend(
      generate_buffer_memory_ports('MEMORY_ADDR_WIDTH', 'MEMORY_DATA_WIDTH',
                                   lambda: index_generator(dims)))
  ports = ast.Portlist(ports_list)
  items = []
  items.append(
      generate_fifo_instance('relay_station', 'occupied_buffers',
                             'fifo_occupied_buffers', 'FIFO_DATA_WIDTH',
                             'FIFO_ADDR_WIDTH', 'FIFO_DEPTH', 'LEVEL'))
  items.append(
      generate_fifo_instance('initialized_relay_station', 'free_buffers',
                             'fifo_free_buffers', 'FIFO_DATA_WIDTH',
                             'FIFO_ADDR_WIDTH', 'FIFO_DEPTH', 'LEVEL'))
  items.append(generate_memcores_instance(memcores_name, 'memcores', dims, 'LEVEL'))
  return ast.ModuleDef(module_name, params, ports, items)


# generate a relay module for a memcore
def generate_relay_memcore_reg(module_name, data_width, addr_width, addr_range,
                               dims):
  parameters = [('DATA_WIDTH', data_width), ('ADDR_WIDTH', addr_width),
                ('ADDR_RANGE', addr_range), ('IS_SIMPLE', 0)]
  params = ast.Paramlist(
      [generate_const_parameter(k, v) for k, v in parameters])
  clk = ast.Ioport(ast.Input('clk'), second=ast.Wire('clk'))
  reset = ast.Ioport(ast.Input('reset'), second=ast.Wire('reset'))
  port_list = [clk, reset]
  for prefix in index_generator(dims):
    port_list.extend(
        generate_registered_ap_memory_interface(f'mem_{prefix}', 'ADDR_WIDTH',
                                                'DATA_WIDTH', 'producer'))
    port_list.extend(
        generate_registered_ap_memory_interface(f'mem_{prefix}', 'ADDR_WIDTH',
                                                'DATA_WIDTH', 'consumer'))
  ports = ast.Portlist(port_list)

  statements = []
  for prefix in index_generator(dims):
    statements.append(
        generate_non_blocking_assignment(f'mem_{prefix}1producer_q',
                                         f'mem_{prefix}2consumer_q'))
    statements.append(
        generate_non_blocking_assignment(f'mem_{prefix}consumer_address',
                                         f'mem_{prefix}producer_address'))
    statements.append(
        generate_non_blocking_assignment(f'mem_{prefix}consumer_ce',
                                         f'mem_{prefix}producer_ce'))
    statements.append(
        generate_non_blocking_assignment(f'mem_{prefix}consumer_d',
                                         f'mem_{prefix}producer_d'))
    statements.append(
        generate_non_blocking_assignment(f'mem_{prefix}consumer_we',
                                         f'mem_{prefix}producer_we'))

  items = [generate_always_block("posedge", "clk", statements)]
  return ast.ModuleDef(module_name, params, ports, items)


# generate assignment `left[left_ptr] = right[right_ptr]`
# where the ptrs are indexing
def generate_assignment(left, left_ptr, right, right_ptr):
  if left_ptr is None:
    left = ast.Lvalue(ast.Identifier(left))
  elif isinstance(left_ptr, str):
    left = ast.Pointer(var=ast.Identifier(left), ptr=ast.Identifier(left_ptr))
  elif isinstance(left_ptr, int):
    left = ast.Pointer(var=ast.Identifier(left), ptr=ast.IntConst(left_ptr))
  if right_ptr is None:
    right = ast.Lvalue(ast.Identifier(right))
  elif isinstance(right_ptr, str):
    right = ast.Pointer(var=ast.Identifier(right),
                        ptr=ast.Identifier(right_ptr))
  elif isinstance(right_ptr, int):
    right = ast.Pointer(var=ast.Identifier(right), ptr=ast.IntConst(right_ptr))
  return ast.Assign(left=left, right=right)


# generate a relay based memcores module
def generate_relay_memcore(module_name, memcore_reg_name, memcore_name,
                           data_width, addr_width, addr_range, levels, dims):
  parameters = [('DATA_WIDTH', data_width), ('ADDR_WIDTH', addr_width),
                ('ADDR_RANGE', addr_range), ('LEVEL', levels), ('IS_SIMPLE', 0)]
  params = ast.Paramlist(
      [generate_const_parameter(k, v) for k, v in parameters])
  clk = ast.Ioport(ast.Input('clk'), second=ast.Wire('clk'))
  reset = ast.Ioport(ast.Input('reset'), second=ast.Wire('reset'))
  port_list = [clk, reset]
  for prefix in index_generator(dims):
    port_list.extend(
        generate_wire_ap_memory_interface(f'mem_{prefix}', 'ADDR_WIDTH',
                                          'DATA_WIDTH', 'producer'))
    port_list.extend(
        generate_wire_ap_memory_interface(f'mem_{prefix}', 'ADDR_WIDTH',
                                          'DATA_WIDTH', 'consumer'))
  ports = ast.Portlist(port_list)
  items = []
  for prefix in index_generator(dims):
    items.append(
        generate_decl(f'mem_{prefix}address', 'wire', 'ADDR_WIDTH', 'LEVEL'))
    items.append(generate_decl(f'mem_{prefix}d', 'wire', 'DATA_WIDTH', 'LEVEL'))
    items.append(generate_decl(f'mem_{prefix}q', 'wire', 'DATA_WIDTH', 'LEVEL'))
    items.append(generate_decl(f'mem_{prefix}ce', 'wire', None, 'LEVEL'))
    items.append(generate_decl(f'mem_{prefix}we', 'wire', None, 'LEVEL'))

  items.append(ast.Decl((ast.Genvar('i'),)))

  assignment_statements = []
  for prefix in index_generator(dims):
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}address', 0,
                            f'mem_{prefix}producer_address', None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}ce', 0, f'mem_{prefix}producer_ce',
                            None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}d', 0, f'mem_{prefix}producer_d',
                            None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}we', 0, f'mem_{prefix}producer_we',
                            None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}producer_q', None, f'mem_{prefix}q',
                            0))
  for prefix in index_generator(dims):
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}address', 'LEVEL',
                            f'mem_{prefix}consumer_address', None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}ce', 'LEVEL',
                            f'mem_{prefix}consumer_ce', None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}d', 'LEVEL',
                            f'mem_{prefix}consumer_d', None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}we', 'LEVEL',
                            f'mem_{prefix}consumer_we', None))
    assignment_statements.append(
        generate_assignment(f'mem_{prefix}consumer_q', None, f'mem_{prefix}q',
                            'LEVEL'))

  instance_param_pairs = [('DATA_WIDTH', 'DATA_WIDTH'),
                          ('ADDR_WIDTH', 'ADDR_WIDTH'),
                          ('ADDR_RANGE', 'ADDR_RANGE'),
                          ('IS_SIMPLE', 'IS_SIMPLE')]

  portlist_pairs = [
      ('clk', ast.Identifier('clk')),
      ('reset', ast.Identifier('reset')),
  ]
  tmp = ast.Plus(ast.Identifier('i'), ast.IntConst(1))
  for prefix in index_generator(dims):
    portlist_pairs.append(
        (f'mem_{prefix}producer_address',
         ast.Pointer(var=ast.Identifier(f'mem_{prefix}address'),
                     ptr=ast.Identifier('i'))))
    portlist_pairs.append((f'mem_{prefix}producer_ce',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}ce'),
                                       ptr=ast.Identifier('i'))))
    portlist_pairs.append((f'mem_{prefix}producer_d',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}d'),
                                       ptr=ast.Identifier('i'))))
    portlist_pairs.append((f'mem_{prefix}producer_we',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}we'),
                                       ptr=ast.Identifier('i'))))
    portlist_pairs.append((f'mem_{prefix}producer_q',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}q'),
                                       ptr=ast.Identifier('i'))))
  for prefix in index_generator(dims):
    portlist_pairs.append(
        (f'mem_{prefix}consumer_address',
         ast.Pointer(var=ast.Identifier(f'mem_{prefix}address'), ptr=tmp)))
    portlist_pairs.append((f'mem_{prefix}consumer_ce',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}ce'),
                                       ptr=tmp)))
    portlist_pairs.append((f'mem_{prefix}consumer_d',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}d'),
                                       ptr=tmp)))
    portlist_pairs.append((f'mem_{prefix}consumer_we',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}we'),
                                       ptr=tmp)))
    portlist_pairs.append((f'mem_{prefix}consumer_q',
                           ast.Pointer(var=ast.Identifier(f'mem_{prefix}q'),
                                       ptr=tmp)))

  items.append(
      ast.GenerateStatement(items=[
          ast.IfStatement(
              cond=ast.GreaterThan(ast.Identifier('LEVEL'), ast.IntConst(0)),
              true_statement=ast.Block([
                  ast.ForStatement(
                      pre=ast.BlockingSubstitution(
                          left=ast.Lvalue(ast.Identifier('i')),
                          right=ast.Rvalue(ast.IntConst(0))),
                      cond=ast.LessThan(ast.Identifier('i'),
                                        ast.Identifier('LEVEL')),
                      post=ast.BlockingSubstitution(
                          left=ast.Lvalue(ast.Identifier('i')),
                          right=ast.Rvalue(
                              ast.Plus(ast.Identifier('i'), ast.IntConst(1)))),
                      statement=ast.Block(
                          [
                              ast.IfStatement(
                                  cond=ast
                                  .LessThan(
                                      ast.Identifier('i'),
                                      ast
                                      .Minus(ast.Identifier('LEVEL'),
                                             ast
                                             .IntConst(1))
                                  ),
                                  true_statement=ast.Block([
                                      generate_instance_with_custom_ports(
                                          memcore_reg_name, 'unit',
                                          instance_param_pairs, portlist_pairs)
                                  ]),
                                  false_statement=ast.Block([
                                      generate_instance_with_custom_ports(
                                          memcore_name, 'unit',
                                          instance_param_pairs, portlist_pairs)
                                  ]))
                          ],
                          scope='inst')), *assignment_statements
              ]),
              false_statement=None)
      ]))

  return ast.ModuleDef(module_name, params, ports, items)


def instance_keep_true_add(match):
  spaces = match.group(1)
  output = 'end else begin\n\n'
  output += spaces
  output += '(* keep = "true" *)\n' + spaces
  output += 'memcore'
  return output


def reg_keep_true_add(match):
  spaces = match.group(1)
  output = '\n'
  output += spaces
  output += '(* keep = "true" *)\n' + spaces
  output += 'output reg'
  return output


def add_keep_true_attributes(file_contents):
  file_contents = re.sub(r'end else begin\n\n(\s*?)memcore',
                         instance_keep_true_add,
                         file_contents,
                         flags=re.MULTILINE | re.DOTALL)
  file_contents = re.sub(r'\n(\s*?)output reg',
                         reg_keep_true_add,
                         file_contents,
                         flags=re.MULTILINE | re.DOTALL)
  return file_contents


def generate_relay_memcore_file(module_name, file_name, memcore_name,
                                data_width, addr_width, addr_range, latency,
                                dims):
  """
  Generates a relayed memcore module file
    module_name: Name of the relay memcore module
    file_name: Name of the verilog file to be generated
    memcore_name: Name of the actual non-relayed memcore that we need to use
    data_width: Numerical width of the data bus
    addr_width: Numerical width of the address bus
    addr_range: Range of the address bus
    latency: Default latency to have from the producer side
    dims: An array showing the iteration pattern for signal naming
  """
  module_reg = generate_relay_memcore_reg(f'{module_name}_reg', data_width,
                                          addr_width, addr_range, dims)
  module = generate_relay_memcore(module_name, f'{module_name}_reg',
                                  memcore_name, data_width, addr_width,
                                  addr_range, latency, dims)
  output_str = ''
  output_str += modules_to_str([module, module_reg])
  output_str = add_keep_true_attributes(output_str)
  with open(file_name, "w") as fd:
    fd.write("`default_nettype none\n")
    fd.write(output_str)
    fd.write("`default_nettype wire\n")


# Let's define a naming convention and specify all the files we need to generate
# Given:
#   buffer_name, dims_pattern, data_width, addr_width, addr_range, default_latency,
#   core_type, base_path, no_partitions
#   no_partitions is the number of 'sections' in the buffer, not to be confused with
#       the number of partitions in the memory. This terminology has been preserved
#       only in this file. Otherwise, the conventional `sections` term is used.
# we need to generate all the needed files, namely,
#   - memcores_{buffer_name}: memcores_{buffer_name}.v <-- the internal memcores
#   - buffer_{buffer_name}: buffer_{buffer_name}.v <-- the non-relayed buffer module
#   - relay_memcores_{buffer_name}_reg: relay_memcores_{buffer_name}.v
#   - relay_memcores_{buffer_name}: relay_memcores_{buffer_name}.v
#   - relay_buffer_{buffer_name}: relay_buffer_{buffer_name}.v
# `module_name` here refers to the unique ID that is generated for each buffer.
# PASTA framework builds separate buffers and memcores modules based on this ID.

def generate_buffer_files(buffer_name, dims_pattern, data_width, addr_width,
                          addr_range, default_latency, core_type, no_partitions,
                          base_path):
  memcores_name = f'memcores_{buffer_name}'
  buffer_module_name = f'buffer_{buffer_name}'
  relay_memcores_name = f'relay_memcores_{buffer_name}'
  relay_buffer_name = f'relay_buffer_{buffer_name}'
  laneswitches_name = f'laneswitches_{buffer_name}'

  module_to_file(
      generate_memcores_module(module_name=memcores_name,
                               data_width=data_width,
                               address_width=addr_width,
                               address_range=addr_range,
                               dims=dims_pattern,
                               ram_style=core_type),
      os.path.join(base_path, f'{memcores_name}.v'))
  if(no_partitions == 1): # only create a laneswitches module if hybrid buffer is required
    module_to_file(
        generate_laneswitches_module(module_name=laneswitches_name,
                                data_width=data_width,
                                address_width=addr_width,
                                address_range=addr_range,
                                dims=dims_pattern),
        os.path.join(base_path, f'{laneswitches_name}.v'))
  module_to_file(
      generate_double_buffer_module(module_name=buffer_module_name,
                                    data_width=data_width,
                                    address_width=addr_width,
                                    address_range=addr_range,
                                    no_partitions=no_partitions,
                                    dims=dims_pattern,
                                    memcores_name=memcores_name,
                                    laneswitches_name=laneswitches_name),
      os.path.join(base_path, f'{buffer_module_name}.v'))
  generate_relay_memcore_file(module_name=relay_memcores_name,
                              file_name=os.path.join(
                                  base_path, f'{relay_memcores_name}.v'),
                              memcore_name=memcores_name,
                              data_width=data_width,
                              addr_width=addr_width,
                              addr_range=addr_range,
                              latency=default_latency,
                              dims=dims_pattern)
  module_to_file(
      generate_relay_double_buffer_module(module_name=relay_buffer_name,
                                          data_width=data_width,
                                          address_width=addr_width,
                                          address_range=addr_range,
                                          no_partitions=no_partitions,
                                          dims=dims_pattern,
                                          memcores_name=relay_memcores_name,
                                          default_level=default_latency),
      os.path.join(base_path, f'{relay_buffer_name}.v'))


###############################################################################
###############################################################################
### ENTRY POINT
###############################################################################

def generate_buffer_from_config(buffer_unique_name, buffer_config, base_path, work_dir):
  tapa.util.setup_logging(1, 0, work_dir)
  buffer_name = buffer_unique_name

  # prepare the dims_pattern to generate the names correctly
  dims_patterns = []
  for dim, partition in zip(buffer_config.dims, buffer_config.partitions):
    if partition.type == "normal":
      dims_patterns.append(1)
    elif partition.type == "complete":
      dims_patterns.append(dim)
    else:
      dims_patterns.append(partition.factor)
  data_width = buffer_config.width

  # find no of memcores
  no_memcores = 1
  for factor in dims_patterns:
    no_memcores *= factor

  # find size of each memcore
  size_memcore = 1
  for dim, partition in zip(buffer_config.dims, dims_patterns):
    size_memcore *= ceil(dim / partition)
  size_memcore *= buffer_config.n_sections

  address_width = ceil(log2(size_memcore))
  core_type = buffer_config.memcore_type

  generate_buffer_files(buffer_name, dims_patterns, data_width, address_width,
                        size_memcore, 2, core_type, buffer_config.n_sections,
                        base_path)
  # tapa.util.setup_logging(2, 1, work_dir)
