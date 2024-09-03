# Changelog (post PASTA)

The original method to acquire and release in PASTA is to do it within specific 
contexts. This limits the usage of the buffer to certain task loops and is not 
helpful when dealing with detached tasks that want to hold a buffer over 
multiple iterations.

Explicit-Release changes this according to how the nomenclature goes.
The acquire/release is slightly different now.

To create the generic variable (in the parent of the program context that the 
buffer is to be used) that holds the reference of the section, the API
`create_section()` is used on the buffer. Unlike the previous implementations, 
no token is read from the `free\_sections` FIFO until the actual `acquire()` 
call is made.

To release a section, the API `release_section()` can be used on the global 
section that was acquired in the parent's context.



