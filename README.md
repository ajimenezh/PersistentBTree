Permanent B-Tree
==========

Implementation of a B-Tree that saves the data in a file obtaining a permanent state.


B-Tree

----------

A B-Tree is a self-balancing tree data structure that keeps data sorted and allows searches, sequantial access, insertions and deletions in logarithmic time (https://en.wikipedia.org/wiki/B-tree). Here, I implemented a B-Tree that saves the data in a file in order to obtain a permanent index on disk storage. To do this, every node in the tree occupies a block of data in a file so it can be easily accessed if you know the offset from the begining of the file, and this offset is a linear function of the id of the node.

Memory Manager

---------

To help with the data handling, the memory manager returns the tree node given its id. The blocks of memory are mapped in a virtual adress space with mmap, and keep a reference counter, so when the node is no longer used, its destroyed. The manager keeps open all the memory blocks used at the same time. 