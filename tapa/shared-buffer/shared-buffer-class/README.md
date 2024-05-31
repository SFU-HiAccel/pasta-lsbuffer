## Task Flow

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

