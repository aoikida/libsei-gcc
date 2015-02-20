=======
libsei
=======

libsei is a library designed to automatically harden crash-tolerant distributed
systems. libsei does not require re-developing the system from scratch, enabling
existing code to be hardened with minimal effort.

|
|

Compilation
----------------

Compile the library::

    % make [OPTIONS]


Options are:

- ``DEBUG=X``: ``X`` might be a value between 0 and 3, where 0 means no
  logging and 3 means very verbose. If ``DEBUG`` is not given, the
  library is compiled with inlining and ``-O3``.

- ``MODE=heap|sbuf``: ``heap`` uses two heaps and ``sbuf`` uses only
  snapshot buffers.

|

Copy ``build/libasco.a`` to a directory in your LD_LIBRARY_PATH and copy
``include/*.h`` to an include directory used by your compiler. See
``examples/simple`` for a template Makefile that demonstrates compilation of the
target application against libsei.

|

SBUF mode
~~~~~~~~~~

SBUF mode comes in the following flavors:

- WB is a write-back algorithm similar to PASC_
- WT is a write-through version which has cheap reads. 

..
  WT can be combined with:

  - ASMREAD: to perform reads using custom assembly code
  - ROPURE: which makes tmasco wrappers of read-only methods transaction_pure
  - APPEND_ONLY: which is a faster version of the algorithm using an abuf insteaf of cow data
    structure

  and ``instr`` only instruments the code.

|
|

libsei interface
--------------------

``int __begin(const void* ptr, size_t size, uint32_t crc)``

Check input message and start handler execution. Returns 1 if the message passes the check, 0 otherwise.

``void __end()``

Marks end of an event handler.

``void __begin_nm()``

Should be used instead of ``__begin()`` if the hardened handler updates global state without receiving a message

``int __begin_rw(const void* ptr, size_t size, uint32_t crc)``

Should be used instead of ``__begin()`` if the hardened handler modifies input message

``void __output_append(const void* ptr, size_t size);``

Calculate a partial checksum of output message. Can be called multiple times.

``void __output_done();``

Finalize CRC.

``uint32_t __crc_pop();``

Read CRC.

Hardening of an event handler can be done in the following way:
::

  if (__begin(...)) {
    //handler code

    __end();
  } else
    continue; // skip corrupted message

Note that ``__begin()`` should always be called within an if statement.

|
|

Examples
--------------

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

|

References
----------------
* `Scalable error isolation for distributed systems
  <https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/behrens>`_ -  Diogo Behrens, *Technische Universität Dresden*; Marco Serafini, *Qatar Computing Research Institute*; Flavio P. Junqueira, *Microsoft Research*; and Sergei Arnautov and Christof Fetzer, *Technische Universität Dresden*. To appear in 12th USENIX Symposium on Networked Systems Design and Implementation (NSDI'15)

.. _PASC: https://www.usenix.org/conference/usenixfederatedconferencesweek/practical-hardening-crash-tolerant-systems
