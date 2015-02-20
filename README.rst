=======
libsei
=======

libsei is a library designed to automatically harden crash-tolerant distributed
systems. libsei does not require re-developing the system from scratch, enabling
existing code to be hardened with minimal effort.

Hardening an event handler using libsei only requires: (i) marking the beginning
and the end of an event handler using the macro functions ``__begin()`` and
``__end()``; (ii) calling ``__output_append(var, var len)`` to indicate that a
variable var is added to the current output messages; (iii) calling ``__output
done()`` to indicate that the output message is complete and its CRC can be
finalized and added to the output buffer; (iv) appending CRCs to output messages
after retrieving them by calling ``__crc pop()``; and finally (v) starting the
compiler as described below.  The developer must include all operations
modifying the state of the process as part of the event handler enclosed by
``__begin()`` and ``__end()``. During run time, the event handler is executed
twice with mechanism similar to setjmp/longjmp implemented in libsei. Event
dispatching and message sending are external to libsei and do not require
interaction with the library.

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

libsei interface
--------------------

``int __begin(const void* ptr, size_t size, uint32_t crc)``

Check input message and start handler execution. Returns 1 if the message passes the check, 0 otherwise.

``void __end()``

Marks end of an event handler.

``void __begin_nm()``

Should be used instead of ``__begin()`` if the hardened handler updates global state without receiving a message.

``int __begin_rw(const void* ptr, size_t size, uint32_t crc)``

Should be used instead of ``__begin()`` if the hardened handler modifies input message.

``void __output_append(const void* ptr, size_t size)``

Calculate a partial checksum of output message. 

``void __output_done()``

Finalize CRC of output message and add to the output buffer. Can be called multiple times for different messages.

``uint32_t __crc_pop()``

Retrieve CRC(s) of output message(s).

Hardening of an event handler can be done in the following way:
::

  if (__begin(...)) {
    //handler code

    __end();
  } else
    continue; // skip corrupted message

Note that ``__begin()`` should always be called within an if statement.

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
* `Towards Transparent Hardening of Distributed Systems
 <http://dl.acm.org/citation.cfm?id=2524230>`_ - Diogo Behrens, Christof Fetzer *Technische Universität Dresden*; Flavio P. Junqueira *Microsoft Research*, Marco Serafini *Qatar Computing Research Institute*), In Proceedings of the 9th Workshop on Hot Topics in Dependable Systems, ACM, 2013

.. _PASC: https://www.usenix.org/conference/usenixfederatedconferencesweek/practical-hardening-crash-tolerant-systems
