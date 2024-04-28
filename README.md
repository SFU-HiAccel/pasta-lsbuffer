# Hybrid Buffer

Hybrid Buffering is a feature enhancement to PASTA. If you're not familiar with the PASTA project please refer to the [PASTA](https://github.com/SFU-HiAccel/pasta/tree/main) repo.

# Installation

The installation process is the same as PASTA, and can be installed using the installer script.
We recommend using a Conda environment for the PASTA project. If a different environment is being used, please follow the detailed installation steps to build from source.

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

## Installation
PASTA can be installed using the installer script already provided.
We recommend using a Conda environment for the PASTA project. If a different environment is being used, please follow the detailed installation steps to build from source.

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

## Installation
PASTA can be installed using the installer script already provided.
We recommend using a Conda environment for the PASTA project. If a different environment is being used, please follow the detailed installation steps to build from source.

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

## Summary of Usage

TAPA supports FIFO stream based communication channels between tasks. With PASTA, buffer channel support was enabled.  
With the hybrid-buffering feature, a mutexed pasta-buffer can serve data on both memory ports to interface a producer/consumer.  
The ports are switched automatically at runtime based on which requestor needs access.

Hybrid buffers are best utilised in dataflow scenarios as a glue buffer between two processing elements that don't have the same data order and are hitting the memory roofline.  
Hence, to recognise if your design can benefit from hybrid-buffers, **check if the memory roofline has already been achieved**.


<details>
<summary>Expand this to see example usage.</summary>

### Buffer channel configuration
A hybrid-buffer channel declaration looks like the following:
```cpp
tapa::buffer<float[NX][NY][NZ],      // the type, followed by the dimensions and their sizes
             1,                      // the no of sections (must be 1 to enable hybrid buffering).
             tapa::array_partition<  // a list of partition strategy for each dimension
                tapa::normal,        // normal partitioning (no partitioning)
                tapa::cyclic<2>,     // cyclic partition with factor of 2, block is also supported
                tapa::complete       // complete partitioning
              >,
             tapa::memcore<tapa::bram> // the memcore to use, can be BRAM and URAM
             >
```

When the number of sections is 1, the tool will automatically convert the regular PASTA buffer into a hybrid buffer.


