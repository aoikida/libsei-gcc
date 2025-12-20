*libsei*: scalable error isolation for distributed systems
============================================================

*libsei* is a library designed to automatically harden crash-tolerant
distributed systems against arbitrary state corruption (ASC) faults.
It employs the `transactional memory support`_ of GCC to instrument the source
code of distributed system processes, introducing both redundant execution and
additional verification steps to the original code.
*libsei* does not require re-developing the system from scratch, enabling
existing code to be hardened with minimal effort.

.. _transactional memory support: http://gcc.gnu.org/wiki/TransactionalMemory

*libsei* implements the Scalable Error isolation (SEI) algorithm described
in our `paper at NSDI'15
<https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/behrens>`_.
See our `technical report
<https://se.inf.tu-dresden.de/pubs/papers/behrens15b.pdf>`_ for the detailed algorithm, its fault model, and correctness proofs.

Check the following links for examples using *libsei*:

- the `examples <https://bitbucket.org/db7/libsei/src/tip/examples/>`_ directory;
- Hardened `Memcached <https://bitbucket.org/db7/libsei-memcached>`_; and
- Hardened `Deadwood DNS resolver <https://bitbucket.org/db7/libsei-deadwood>`_.

Software fault injections can be peformed with our Pintool
`BFI <https://bitbucket.org/db7/bfi>`_.

|

System model
------------

SEI targets event-based processes of distributed systems.
Processes consist of one or more threads that spin over three phases:

* Dispatching receives a new event (message) and selects an event handler;
* Handling executes the actual system logic;
* Output sends out messages produced by the event handler.

Threads read from and write to state variables, which collectively form the
state of the process.
These variables persist across the multiple event handling cycles and can be
shared among threads.
A thread might also have a local state, which encompasses the variables that
are instantiated every time a handler is executed but do not persist across
handler executions.
The state of a process includes all state that is directly observed by its
threads and used to determine their behavior.

The event handling logic is required to be deterministic, i.e., the state
updates and outputs it produces depend uniquely on the input message and the
values returned by its reads from the process state.
However, we do not require deterministic thread scheduling.
Threads can be scheduled in any order and preempted arbitrarily.

Threads can interact through shared variables, which are only accessed in
critical sections protected by locks.
While this requirement does not cover applications using lock-free state
sharing, it represents a very common approach.
We assume that threads use lock hierarchies, a standard technique to avoid
circular waits and deadlocks: there is a predefined total order among all
locks, and threads acquire and release the locks they need according to this
order.

Applying *libsei* to a code base
----------------------------------

Hardening an event handler using *libsei* only requires:

1. marking the beginning and the end of an event handler using the macro
   functions ``__begin()`` and ``__end()``;
2. calling ``__output_append(var, len)`` to indicate that a variable var is
   added to the current output messages;
3. calling ``__output_done()`` to indicate that the output message is complete
   and its CRC can be finalized and added to the output buffer;
4. appending CRCs to output messages after retrieving them by calling
   ``__crc_pop()``; and finally 
5. starting the compiler as described below. 

The developer must include all operations modifying the state of the process
as part of the event handler enclosed by ``__begin()`` and ``__end()``.
During run time, the event handler is executed twice with mechanism similar to
``setjmp/longjmp`` implemented in *libsei*.
Event dispatching and message sending are external to *libsei* and do not
require interaction with the library.
Please note that, at the moment, *libsei* only supports state stored in
memory.


Here is a simple example of what an code hardened with *libsei* looks
like::

    char     *imsg = NULL, *omsg = NULL;
    uint32_t crc;
    size_t   ilen, olen;

    while(1) {
        ilen = recv_msg_and_crc(&imsg, &crc);

        /* __begin checks if incoming message is correct
         * must be called within if-statement   */
        if (__begin(imsg, ilen, crc)) { 

            /* process the message */
            do_something_here(imsg); 

            /* create output message */
            omsg = create_a_message_here(&olen);

            /* calculate CRC of the output message; if only a part of the 
             * message is created, this function should be called again 
             * for other parts of the message */
            __output_append(omsg, olen); 

            /* Finalize CRC once a complete output message was created */
            __output_done(); 

            /* end of the hardened event handler */
            __end();
        } else /* discard invalid input */ 
            continue; 
        
        printf("counter: %zu\n", counter);
        
        /* read the calculated CRC and send it along with the message */
        send_msg_and_crc(omsg, olen, __crc_pop());
    }


For complete examples, see the
`examples <https://bitbucket.org/db7/libsei/src/tip/examples/>`_ directory.

|

Compilation
-----------


Compile the library simply call::

    make [OPTIONS]


Currently, the supported options are:

- ``DEBUG=X``: ``X`` might be a value between 0 and 3, where 0 means no
  logging and 3 means very verbose. If ``DEBUG`` is not given, the
  library is compiled with inlining and ``-O3``.

- ``ROLLBACK=1``: Enable automatic rollback on error detection with CPU core
  isolation, SDC (Silent Data Corruption) detection, and SIGSEGV recovery.
  When enabled, transactions are executed with redundancy verification. If SDC
  is detected or a segmentation fault occurs within a transaction, the faulty
  core is blacklisted and the transaction is automatically rolled back and
  retried on a different core. The process terminates only when all available
  cores have been blacklisted.

- ``EXECUTION_CORE_REDUNDANCY=1``: Execute different phases on different CPU
  cores (requires ``ROLLBACK=1``). When enabled, each execution phase runs on
  a different CPU core to improve detection of hardware-specific faults. If SDC
  is detected, all involved cores are blacklisted and the transaction is retried
  on new cores.

- ``CRC_CORE_REDUNDANCY=1``: Compute CRC on different CPU cores (requires
  ``ROLLBACK=1``). When enabled, input message CRC verification is performed
  on different cores using modular redundancy. If CRC mismatch is detected
  between cores, the involved cores are blacklisted and verification is retried
  on new cores. This is independent of ``CRC_REDUNDANCY`` and can be used
  together with ``EXECUTION_CORE_REDUNDANCY``.

- ``EXECUTION_REDUNDANCY=N``: Configure N-way execution redundancy (default: 2,
  range: 2-10). Transactions are executed N times and all N executions must
  produce identical results for commit to succeed. Higher N values provide
  stronger fault detection at the cost of increased execution overhead.
  Can be combined with ``ROLLBACK``, ``CRC_REDUNDANCY``, and core redundancy
  flags.

- ``CRC_REDUNDANCY=N``: Configure N-way CRC redundancy (range: 2-10).
  Input message CRC verification is performed N times and all N computations
  must match. Can be combined with ``EXECUTION_REDUNDANCY`` and ``ROLLBACK``.
  Note: Cannot be used together with ``CRC_CORE_REDUNDANCY`` as they are
  mutually exclusive approaches.

- ``FAULT_INJECT=1``: Enable fault injection for testing error recovery
  mechanisms. When enabled, faults can be injected at runtime using environment
  variables. Requires ``ROLLBACK=1`` for recovery testing.

**Valid Flag Combinations:**

The following combinations are supported and tested:

1. **Core redundancy flags** (can be combined):

   - ``CRC_CORE_REDUNDANCY`` + ``EXECUTION_CORE_REDUNDANCY``
   - Both require ``ROLLBACK=1``

2. **Value redundancy flags** (can be combined):

   - ``CRC_REDUNDANCY`` + ``EXECUTION_REDUNDANCY`` + ``ROLLBACK``

**Invalid Combinations:**

- ``CRC_CORE_REDUNDANCY`` and ``CRC_REDUNDANCY`` are mutually exclusive

.. - ``MODE=heap|sbuf``: ``heap`` uses two heaps and ``sbuf`` uses only
  snapshot buffers.

An application instrumented with *libsei* has to be linked with ``-lsei`` and
compiled with the ``-fgnu-tm`` flag.
*libsei* has been tested with GCC version 4.7 in Linux environments.

See ``examples/simple`` for a template Makefile that demonstrates compilation
of the target application against *libsei*.

Example build commands::

    # Basic build (2-way execution redundancy, no rollback)
    make

    # N-way execution redundancy without rollback
    EXECUTION_REDUNDANCY=3 make
    EXECUTION_REDUNDANCY=5 make
    EXECUTION_REDUNDANCY=10 make

    # With rollback (2-way execution redundancy by default)
    ROLLBACK=1 make

    # N-way execution redundancy with rollback
    ROLLBACK=1 EXECUTION_REDUNDANCY=3 make
    ROLLBACK=1 EXECUTION_REDUNDANCY=5 make

    # With execution core redundancy (different cores for each phase)
    ROLLBACK=1 EXECUTION_CORE_REDUNDANCY=1 make

    # With CRC core redundancy (different cores for CRC computation)
    ROLLBACK=1 CRC_CORE_REDUNDANCY=1 make

    # With both core redundancy flags (maximum fault detection)
    ROLLBACK=1 EXECUTION_CORE_REDUNDANCY=1 CRC_CORE_REDUNDANCY=1 make

    # N-way execution with both core redundancies
    ROLLBACK=1 EXECUTION_CORE_REDUNDANCY=1 CRC_CORE_REDUNDANCY=1 EXECUTION_REDUNDANCY=5 make

    # CRC redundancy with execution redundancy
    CRC_REDUNDANCY=3 EXECUTION_REDUNDANCY=3 make
    ROLLBACK=1 CRC_REDUNDANCY=3 EXECUTION_REDUNDANCY=5 make

    # Debug build with all redundancy features
    DEBUG=3 ROLLBACK=1 CRC_REDUNDANCY=3 EXECUTION_REDUNDANCY=5 make

    # Build with fault injection for testing
    ROLLBACK=1 FAULT_INJECT=1 make


Fault Injection
~~~~~~~~~~~~~~~

When built with ``FAULT_INJECT=1``, faults can be injected at runtime using
environment variables to test the error recovery mechanisms.

**Environment Variables:**

- ``SEI_FAULT_TYPE``: Type of fault to inject

  - ``0``: Corrupt first abuf entry (SDC, default)
  - ``1``: Corrupt random abuf entry (SDC)
  - ``2``: Corrupt last abuf entry (SDC)
  - ``3``: Corrupt multiple abuf entries (SDC)
  - ``5``: Trigger SIGSEGV (segmentation fault)

- ``SEI_FAULT_INJECT_AFTER_TXN=N``: Inject fault after N transactions
- ``SEI_FAULT_INJECT_DELAY_MS=N``: Inject fault after N milliseconds

**Example usage:**
::

    # Build with fault injection enabled
    ROLLBACK=1 FAULT_INJECT=1 make
    cd examples/ukv && ROLLBACK=1 FAULT_INJECT=1 make

    # Test SDC recovery (corrupt first entry after 3 transactions)
    SEI_FAULT_TYPE=0 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000

    # Test SIGSEGV recovery (trigger segfault after 3 transactions)
    SEI_FAULT_TYPE=5 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000

    # Test with time-based injection (inject after 5 seconds)
    SEI_FAULT_TYPE=5 SEI_FAULT_INJECT_DELAY_MS=5000 ./build_sei/ukv-server.sei 10000

|

To install *libsei*, simply copy ``build/libsei.a`` to a directory in your
``LD_LIBRARY_PATH`` and copy ``include/*`` to an include directory used by
your compiler.

|

.. SBUF mode
.. ~~~~~~~~~~
..
.. SBUF mode comes in the following flavors:
..
.. - WB is a write-back algorithm similar to PASC_
.. - WT is a write-through version which has cheap reads. 
..
.. ..
..   WT can be combined with:
..
..   - ASMREAD: to perform reads using custom assembly code
..   - ROPURE: which makes tmi wrappers of read-only methods transaction_pure
..   - APPEND_ONLY: which is a faster version of the algorithm using an abuf insteaf of cow data
..     structure
..
..   and ``instr`` only instruments the code.
..
.. |

*libsei* interface
--------------------

Main interface
~~~~~~~~~~~~~~

``int __begin(const void* ptr, size_t size, uint32_t crc)``

Check input message and start handler execution. Returns 1 if the message passes the check, 0 otherwise.

``void __end()``

Marks end of an event handler.

Hardening of an event handler can be done in the following way:
::

  if (__begin(...)) {
    //handler code

    __end();
  } else
    continue; // skip corrupted message

Note that ``__begin()`` should always be called within an if statement.


``void __output_append(const void* ptr, size_t size)``

Calculate a partial checksum of output message. 

``void __output_done()``

Finalize CRC of output message and add to the output buffer. Can be called multiple times for different messages.

``uint32_t __crc_pop()``

Retrieve CRC(s) of output message(s).


Extended interface
~~~~~~~~~~~~~~~~~~

``void __begin_nm()``

Should be used instead of ``__begin()`` if the hardened handler updates global state without receiving a message.

``int __begin_rw(const void* ptr, size_t size, uint32_t crc)``

Should be used instead of ``__begin()`` if the hardened handler modifies input message.


Dynamic N-way execution interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following APIs allow runtime configuration of execution redundancy levels, enabling different transactions to use different redundancy levels (N values) within the same process. The compile-time ``EXECUTION_REDUNDANCY=N`` setting determines the maximum N value that can be used at runtime.

**Requirements:**

- Runtime N value must not exceed compile-time ``EXECUTION_REDUNDANCY`` value
- When using ``ROLLBACK=1`` with ``EXECUTION_REDUNDANCY=3`` or higher, runtime N should be 3 or higher for optimal compatibility
- Runtime N=2 may cause failures in ``ROLLBACK=1`` environments compiled with ``EXECUTION_REDUNDANCY=3`` or higher due to assertion constraints in verification code

**APIs:**

``int __begin_n(const void* ptr, size_t size, uint32_t crc, int N)``

Check input message and start handler execution with N-way redundancy. Returns 1 if the message passes the check, 0 otherwise. The transaction will execute N times and all N executions must produce identical results.

``void __begin_nm_n(int N)``

Should be used instead of ``__begin_n()`` if the hardened handler updates global state without receiving a message. Executes with N-way redundancy.

``int __begin_rw_n(const void* ptr, size_t size, uint32_t crc, int N)``

Should be used instead of ``__begin_n()`` if the hardened handler modifies input message. Executes with N-way redundancy.

**Example usage:**
::

  // Compile with maximum N=5
  // make ROLLBACK=1 EXECUTION_REDUNDANCY=5

  // Initialize with N=2 redundancy
  __begin_nm_n(2);
  initialize_system();
  __end();

  // Process high-priority messages with N=5 redundancy
  if (__begin_n(msg1, len1, crc1, 5)) {
    process_critical_operation(msg1);
    __end();
  }

  // Process normal messages with N=3 redundancy
  if (__begin_n(msg2, len2, crc2, 3)) {
    process_normal_operation(msg2);
    __end();
  }

**Recommended patterns:**

- **Pattern N=3/3/3**: Consistent 3-way redundancy for all operations (requires ``EXECUTION_REDUNDANCY=3``)
- **Pattern N=2/3/2**: Lightweight initialization (N=2), critical processing (N=3), cleanup (N=2) - **RECOMMENDED** for balanced performance and reliability (requires ``EXECUTION_REDUNDANCY=3``)
- **Pattern N=2/5/2**: Maximum critical protection (N=5) with lightweight initialization - requires ``EXECUTION_REDUNDANCY=5``

See ``build_configurations.md`` for detailed testing results and compatibility matrix.

|

References
----------------
* `Scalable error isolation for distributed systems
  <https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/behrens>`_ -  Diogo Behrens, Marco Serafini, Sergei Arnautov, Flavio P. Junqueira, Christof Fetzer. *12th USENIX Symposium on Networked Systems Design and Implementation (NSDI'15)*
* `Scalable error isolation for distributed systems: modeling, correctness proofs, and additional experiments
  <https://se.inf.tu-dresden.de/pubs/papers/behrens15b.pdf>`_ - Diogo Behrens, Marco Serafini, Sergei Arnautov, Flavio P. Junqueira, Christof Fetzer. *Technical report, Technische Universität Dresden, Fakultät Informatik, 2015. TUD-FI15-01-Februar 2015, ISSN 1430-211X*
* `Towards transparent hardening of distributed systems
  <http://dl.acm.org/citation.cfm?id=2524230>`_ - Diogo Behrens, Christof Fetzer, Flavio P. Junqueira, Marco Serafini, *9th ACM Workshop on Hot Topics in Dependable Systems (HotDep'13)*

