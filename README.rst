libsei
=======

libsei is a library designed to automatically harden crash-tolerant distributed
systems. libsei does not require re-developing the system from scratch, enabling
existing code to be hardened with minimal effort.


Compilation
-----------

Compile the library::

  % make [OPTIONS]


Options are:

- ``DEBUG=X``: ``X`` might be a value between 0 and 3, where 0 means no
  logging and 3 means very verbose. If ``DEBUG`` is not given, the
  library is compiled with inlining and ``-O3``.

- ``MODE=heap|cow``: ``heap`` use two heaps and ``cow`` uses only
  copy-on-write buffers.


Copy ``build/libasco.a`` to a directory in your LD_LIBRARY_PATH and copy
``include/*.h`` to an include directory used by your compiler. See
``examples/simple`` for a template Makefile that demonstrates compilation of the
target application against libsei.

COW mode
~~~~~~~~

COW mode comes in the following flavors:

- WB is a write-back algorithm similar to PASC
- WT is a write-through version which has cheap reads. WT can be combined with:

  - ASMREAD: to perform reads using custom assembly code
  - ROPURE: which makes tmasco wrappers of read-only methods transaction_pure
  - APPEND_ONLY: which is a faster version of the algorithm using an abuf insteaf of cow data
    structure

.. and ``instr`` only instruments the code.


libsei interface
----------------

if (__begin(...)) {
  //handler code

  __end();
} else 
  continue; // skip corrupted message

If the input message is modified during traversal, __begin_rw(...) and
__end_rw() should be used instead.

If the handler is local, i.e., it updates global state without receiving any
input message, __begin_nm() and __end_nm() can be used.

Examples
--------

See ``examples/simple`` and ``examples/ukv``.

..
   Step into example and call::

     % scons

   The example uses ``tmasco`` as transactional memory.

   Note that we have "glued" all files together (``glue.c``). The reason is
   the following:

   * The current implementation of gcc-tm requires each method accessed
     from within a transaction but compiled in another module to be
     declared with ``__attribute__((transaction_safe))``. In our example,
     the method ``foo()`` in ``foo.c``. That requires modifying a lot of
     method declarations in several files for a larger project.

   * Although it can compile, the current implemenation of llvm-tm does
     not support multiple modules at runtime. Adding the attribute above
     transactify the methods, but the application crashes if it accesses
     methods from another module.

   So in the moment the easiest way to to use ``tmasco`` is to compile all
   modules as a single big module, in our example ``glue.c``.


References
----------
* `Scalable error isolation for distributed systems
  <https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/behrens>`_ - Behrens et al.

* `Practical Hardening of Crash-Tolerant Systems
  <https://www.usenix.org/conference/usenixfederatedconferencesweek/practical-hardening-crash-tolerant-systems>`_ - Correia et al.
