from math import ceil, log2
from typing import Dict, List, Tuple


class PartitionDim:

  def __init__(self, type: str, factor: int):
    self.type = type
    self.factor = factor

  def __dict__(self) -> Dict[str, str]:
    return {"type": self.type, "factor": self.factor}

  def __eq__(self, other: 'PartitionDim') -> bool:
    return self.type == other.type and self.factor == other.factor

  def __hash__(self) -> int:
    return hash((self.type, self.factor))


class BufferConfig:
  """Stores the configuration of a buffer channel

  Attributes:
    width: Width of one word in the buffer
    type: A string indicating the type of a word (e.g. float, int)
    dims: A list containing the size of each dimension
    partitions: A list containing PartitionConfig of each dimension
  """

  class DIR:
    INPUT = 'input'
    OUTPUT = 'output'


  def __init__(self, obj: Dict):
    self.width = obj["width"]
    self.type = obj["type"]
    self.dims = obj["dims"]
    self.n_sections = obj["n_sections"]
    self.partitions = []
    for partition in obj["partitions"]:
      self.partitions.append(
          PartitionDim(partition["type"], partition["factor"]))
    self.memcore_type = obj["memcore_type"]


  def __eq__(self, other: 'BufferConfig') -> bool:
    return self.width == other.width and \
            self.type == other.type and \
            self.dims == other.dims and \
            self.n_sections == other.n_sections and \
            self.memcore_type == other.memcore_type and \
            all([left == right for left, right in zip(self.partitions, other.partitions)])


  def __hash__(self) -> int:
    return hash((self.width, self.type, tuple(self.dims), self.n_sections,
                 tuple(self.partitions), self.memcore_type))


  def get_dim_patterns(self) -> List[int]:
    dims_patterns = []
    for dim, partition in zip(self.dims, self.partitions):
      if partition.type == "normal":
        dims_patterns.append(1)
      elif partition.type == "complete":
        dims_patterns.append(dim)
      else:
        dims_patterns.append(partition.factor)
    return dims_patterns


  def get_memcore_size(self) -> int:
    size_memcore = 1
    dims_patterns = self.get_dim_patterns()
    for dim, partition in zip(self.dims, dims_patterns):
      size_memcore *= ceil(dim / partition)
    size_memcore *= self.n_sections
    return size_memcore


  def get_no_memcores(self) -> int:
    dims_patterns = self.get_dim_patterns()
    no_memcores = 1
    for pattern in dims_patterns:
      no_memcores *= pattern
    return no_memcores


  def get_addr_width(self) -> int:
    return ceil(log2(self.get_memcore_size()))


  def get_producer_fifo_port_names(self) -> Tuple[str]:
    return (
        'fifo_free_buffers_empty_n',
        'fifo_free_buffers_read',
        'fifo_free_buffers_dout',
        'fifo_occupied_buffers_full_n',
        'fifo_occupied_buffers_write',
        'fifo_occupied_buffers_din',
        'fifo_free_buffers_read_ce',
        'fifo_occupied_buffers_write_ce',
    )


  def get_consumer_fifo_port_names(self) -> Tuple[str]:
    return (
        'fifo_occupied_buffers_empty_n',
        'fifo_occupied_buffers_read',
        'fifo_occupied_buffers_dout',
        'fifo_free_buffers_full_n',
        'fifo_free_buffers_write',
        'fifo_free_buffers_din',
        'fifo_occupied_buffers_read_ce',
        'fifo_free_buffers_write_ce',
    )


  def get_buffer_port_names(self) -> Tuple[str]:
    """
    @see add_buffer_instance in module.py
    The caller attaches `<index>_<producer/consumer>_` to the strings to
    fetch the buffer ports.
    """
    # tag: SYNTAX_PORT_BUFFER
    if (self.n_sections == 1):
      returntuple = (
        'buffer_core{}address0',
        'buffer_core{}ce0',
        'buffer_core{}d0',
        'buffer_core{}we0',
        'buffer_core{}q0',
        'buffer_core{}address1',
        'buffer_core{}ce1',
        'buffer_core{}d1',
        'buffer_core{}we1',
        'buffer_core{}q1',
      )
    else:
      returntuple = (
        'buffer_core{}address0',
        'buffer_core{}ce0',
        'buffer_core{}d0',
        'buffer_core{}we0',
        'buffer_core{}q0',
      )
    return returntuple

  #############################################################################
  #############################################################################
  ### SUFFIX GENERATION
  #############################################################################

  def get_consumer_fifo_suffixes(self) -> Tuple[Tuple[str, int]]:
    return (
        ('_fifo_occupied_buffers_empty_n', 1, BufferConfig.DIR.INPUT,
         '_src_empty_n', True),
        ('_fifo_occupied_buffers_read', 1, BufferConfig.DIR.OUTPUT, '_src_read',
         True),
        ('_fifo_occupied_buffers_dout', 32, BufferConfig.DIR.INPUT, '_src_dout',
         True),
        ('_fifo_free_buffers_full_n', 1, BufferConfig.DIR.INPUT, '_sink_full_n',
         True),
        ('_fifo_free_buffers_write', 1, BufferConfig.DIR.OUTPUT, '_sink_write',
         True),
        ('_fifo_free_buffers_din', 32, BufferConfig.DIR.OUTPUT, '_sink_din',
         True),
    )


  def get_producer_fifo_suffixes(self) -> Tuple[Tuple[str, int]]:
    return (
        ('_fifo_free_buffers_empty_n', 1, BufferConfig.DIR.INPUT,
         '_src_empty_n', True),
        ('_fifo_free_buffers_read', 1, BufferConfig.DIR.OUTPUT, '_src_read',
         True),
        ('_fifo_free_buffers_dout', 32, BufferConfig.DIR.INPUT, '_src_dout',
         True),
        ('_fifo_occupied_buffers_full_n', 1, BufferConfig.DIR.INPUT,
         '_sink_full_n', True),
        ('_fifo_occupied_buffers_write', 1, BufferConfig.DIR.OUTPUT,
         '_sink_write', True),
        ('_fifo_occupied_buffers_din', 32, BufferConfig.DIR.OUTPUT, '_sink_din',
         True),
    )


  # suffix, width, wire_dir, port_suffix, _
  def get_producer_memory_suffixes(self) -> Tuple[Tuple[str, int]]:
    # tag: SYNTAX_PORT_BUFFER
    if (self.n_sections == 1):
      # create 2 port implementation
      returntuple = (
      ('buffer_core{}producer_address0', ceil(log2(self.get_memcore_size())), BufferConfig.DIR.OUTPUT, '_data_{}address0', True),
      ('buffer_core{}producer_ce0', 1, BufferConfig.DIR.OUTPUT, '_data_{}ce0', True),
      ('buffer_core{}producer_d0', self.width, BufferConfig.DIR.OUTPUT, '_data_{}d0', True),
      ('buffer_core{}producer_we0', 1, BufferConfig.DIR.OUTPUT, '_data_{}we0', True),
      ('buffer_core{}producer_q0', self.width, BufferConfig.DIR.INPUT, '_data_{}q0', False),
      # entire 2nd port is marked with `required=False` since some tasks might use only 1 port.
      ('buffer_core{}producer_address1', ceil(log2(self.get_memcore_size())), BufferConfig.DIR.OUTPUT, '_data_{}address1', False),
      ('buffer_core{}producer_ce1', 1, BufferConfig.DIR.OUTPUT, '_data_{}ce1', False),
      ('buffer_core{}producer_d1', self.width, BufferConfig.DIR.OUTPUT, '_data_{}d1', False),
      ('buffer_core{}producer_we1', 1, BufferConfig.DIR.OUTPUT, '_data_{}we1', False),
      ('buffer_core{}producer_q1', self.width, BufferConfig.DIR.INPUT, '_data_{}q1', False),
      )
    else:
      returntuple = (
      ('buffer_core{}producer_address0', ceil(log2(self.get_memcore_size())), BufferConfig.DIR.OUTPUT, '_data_{}address0', True),
      ('buffer_core{}producer_ce0', 1, BufferConfig.DIR.OUTPUT, '_data_{}ce0', True),
      ('buffer_core{}producer_d0', self.width, BufferConfig.DIR.OUTPUT, '_data_{}d0', True),
      ('buffer_core{}producer_we0', 1, BufferConfig.DIR.OUTPUT, '_data_{}we0', True),
      ('buffer_core{}producer_q0', self.width, BufferConfig.DIR.INPUT, '_data_{}q0', False),
      )

    return returntuple


  # suffix, width, wire_dir, port_suffix, required
  def get_consumer_memory_suffixes(self) -> Tuple[Tuple[str, int]]:
    # tag: SYNTAX_PORT_BUFFER
    if (self.n_sections == 1):
      # create 2 port implementation
      returntuple = (
      ('buffer_core{}consumer_address0', ceil(log2(self.get_memcore_size())), BufferConfig.DIR.OUTPUT, '_data_{}address0', True),
      ('buffer_core{}consumer_ce0', 1, BufferConfig.DIR.OUTPUT, '_data_{}ce0', True),
      ('buffer_core{}consumer_d0', self.width, BufferConfig.DIR.OUTPUT, '_data_{}d0', False),
      ('buffer_core{}consumer_we0', 1, BufferConfig.DIR.OUTPUT, '_data_{}we0', False),
      ('buffer_core{}consumer_q0', self.width, BufferConfig.DIR.INPUT, '_data_{}q0', True),
      # entire 2nd port is marked with `required=False` since some tasks might use only 1 port.
      ('buffer_core{}consumer_address1', ceil(log2(self.get_memcore_size())), BufferConfig.DIR.OUTPUT, '_data_{}address1', False),
      ('buffer_core{}consumer_ce1', 1, BufferConfig.DIR.OUTPUT, '_data_{}ce1', False),
      ('buffer_core{}consumer_d1', self.width, BufferConfig.DIR.OUTPUT, '_data_{}d1', False),
      ('buffer_core{}consumer_we1', 1, BufferConfig.DIR.OUTPUT, '_data_{}we1', False),
      ('buffer_core{}consumer_q1', self.width, BufferConfig.DIR.INPUT, '_data_{}q1', False),
      )
    else:
      returntuple = (
      ('buffer_core{}consumer_address0', ceil(log2(self.get_memcore_size())), BufferConfig.DIR.OUTPUT, '_data_{}address0', True),
      ('buffer_core{}consumer_ce0', 1, BufferConfig.DIR.OUTPUT, '_data_{}ce0', True),
      ('buffer_core{}consumer_d0', self.width, BufferConfig.DIR.OUTPUT, '_data_{}d0', False),
      ('buffer_core{}consumer_we0', 1, BufferConfig.DIR.OUTPUT, '_data_{}we0', False),
      ('buffer_core{}consumer_q0', self.width, BufferConfig.DIR.INPUT, '_data_{}q0', True),
      )

    return returntuple


  def get_fifo_suffixes(self, direction: str) -> Tuple[Tuple[str, int]]:
    if direction == "produced_by":
      return self.get_producer_fifo_suffixes()
    elif direction == "consumed_by":
      return self.get_consumer_fifo_suffixes()


  def get_memory_suffixes(self, direction: str) -> Tuple[Tuple[str, int]]:
    if direction == "produced_by":
      return self.get_producer_memory_suffixes()
    elif direction == "consumed_by":
      return self.get_consumer_memory_suffixes()
