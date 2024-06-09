# Blocks

## RQR (Request Router)

Receives requests from all xctrs on sb\_rxqs and routes them in either the control(free/grab) or data(read/write) queues corresponding to the xctr


## RQP (Request Parser)

Parses all control/data queues and:
* communicates with PGM for control packets
* communicates with IOHD and RSG for data packets


## PGM (Page Manager)

Manages the pages and performs all necessary bookkeeping


## IHD (Input Handler)

Manages reads


## OHD (Output Handler)

Manages Writes


## RSG (Response Generator)

Generates the response for all xctrs on sb\_txqs.


---

# Progress

## Alpha

* [x] Loopback
* [x] Flow and Connectivity
* [x] Multiple XCTRs
* [x] Buffers
* [x] Establish shared buffering on hardcoded page

## Beta

* [ ] PGM with Book-keeping
* [ ] Establish shared buffering in general

## Gamma

* [ ] Performance Optimisations for IOHD queues
* [ ] II=1
* [ ] Resource Reduction


---

# Task Flow

```
          +------------+             +------------+
          | Mmap2Stream|             | Mmap2Stream|
          | vector_b  o|             | vector_a  o|
          +------------+             +------------+
                |                           |      
                | vector_b                  | vector_a      
                v                           v      
          +-----------+             +-----------+  
          |           |   page_info |           |------------> CRAZY STUFF  
          |  task2    <--------------  task1    |                 |
          |           |             |           |<-----------------  
          +-----------+             +-----------+  
                |
                | vector_c
                v
          +-----------+
          |           |
          |  Stream2  |
          |   Mmap    |
          |           |
          +-----------+
                |      
                +
                |      
                v      

```




