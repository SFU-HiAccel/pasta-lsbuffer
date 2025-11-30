# Laneswitching Buffers

Laneswitch (LS) Buffering is a feature enhancement to PASTA. If you're not familiar with the PASTA project please refer to the [PASTA](https://github.com/SFU-HiAccel/pasta/tree/main) repo.

## Summary of Usage

TAPA supports FIFO stream based communication channels between tasks. With PASTA, buffer channel support was enabled.  
With the LS-buffering feature, a mutexed buffer can serve data on both memory ports to interface a producer/consumer.  
The ports are switched automatically at runtime based on which requestor needs access.  
This creates the illusion of 4-port memory while using 2-port memory underneath.

LS buffers are best utilised in dataflow scenarios as a glue buffer between two processing elements that don't have 
the same data order and have imbalanced producer/consumer speeds.  
Hence, to recognise if your design can benefit from LS-buffers, **check if there exist imbalanced producer-consumer relationships**.

# Installation

An installer script is provided.  
We recommend using a Conda environment for the project. If a different environment is being used, please follow the detailed installation steps to build from source.

### Download the repository

```
git clone https://github.com/SFU-HiAccel/pasta.git
### OR ###
git clone git@github.com:SFU-HiAccel/pasta.git
```

### Setup a Conda environment
[Install Miniconda](https://docs.conda.io/projects/miniconda/en/latest/miniconda-install.html)  

```
conda create -y --name "pasta"
conda activate pasta
```

### Build PASTA
Once you have a custom conda environment loaded, navigate to the `installer` folder and run the `install` script.

```
cd <repo_root>/installer
./install
```

Once the installation is complete, a `setup` file will be created in the project directory.  
Please use `source setup` to setup the current shell with the required PATHs.

---

<details>
<summary>Expand this to see example usage.</summary>

### Buffer channel configuration
An LS-buffer channel declaration looks like the following:
```cpp
tapa::buffer<float[NX][NY][NZ],      // the type, followed by the dimensions and their sizes
             1,                      // the no of sections (must be 1 to enable LS buffering).
             tapa::array_partition<  // a list of partition strategy for each dimension
                tapa::normal,        // normal partitioning (no partitioning)
                tapa::cyclic<2>,     // cyclic partition with factor of 2, block is also supported
                tapa::complete       // complete partitioning
              >,
             tapa::memcore<tapa::bram> // the memcore to use, can be BRAM and URAM
             >
```

When the number of sections in the declaration is 1, the tool will automatically convert the regular buffer into a LS buffer.

## Citation

If you use HiSpMM in your research, please cite:

```bibtex
@article{10.1145/3771938,
author = {Baranwal, Akhil Raj and Fang, Zhenman},
title = {PoCo: Extending Task-Parallel HLS Programming with Shared Multi-Producer Multi-Consumer Buffer Support},
year = {2025},
publisher = {Association for Computing Machinery},
address = {New York, NY, USA},
issn = {1936-7406},
url = {https://doi.org/10.1145/3771938},
doi = {10.1145/3771938},
abstract = {Advancements in High-Level Synthesis (HLS) tools have enabled task-level parallelism on FPGAs. However, prevailing frameworks predominantly employ single-producer-single-consumer (SPSC) models for task communication, thus limiting application scenarios. Analysis of designs becomes non-trivial with an increasing number of tasks in task-parallel systems. Adding features to existing designs often requires re-profiling of several task interfaces, redesign of the overall inter-task connectivity, and describing a new floorplan. This paper proposes PoCo, a novel framework to design scalable multi-producermulti- consumer (MPMC) models on task-parallel systems. PoCo introduces a shared-buffer abstraction that facilitates dynamic and high-bandwidth access to shared on-chip memory resources, incorporates latency-insensitive communication, and implements placement-aware design strategies to mitigate routing congestion. The frontend provides convenient APIs to access the buffer memory, while the backend features an optimized and pipelined datapath. Empirical evaluations demonstrate that PoCo achieves up to 50\% reduction in on-chip memory utilization on SPSC models without performance degradation. Additionally, three case studies on distinct real-world applications reveal up to 1.5\texttimes{} frequency improvements and simplified dataflow management in heterogeneous FPGA accelerator designs.},
note = {Just Accepted},
journal = {ACM Trans. Reconfigurable Technol. Syst.},
month = oct,
keywords = {Multi-producer multi-consumer, buffer optimization, floorplan optimization, multi-die FPGA, high-level synthesis}
}
```

