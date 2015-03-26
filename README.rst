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

For examples using *libsei* check:

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


Currently, the only supported options is: 

- ``DEBUG=X``: ``X`` might be a value between 0 and 3, where 0 means no
  logging and 3 means very verbose. If ``DEBUG`` is not given, the
  library is compiled with inlining and ``-O3``.

.. - ``MODE=heap|sbuf``: ``heap`` uses two heaps and ``sbuf`` uses only
  snapshot buffers.

An application instrumented with *libsei* has to be linked with ``-lsei`` and
compiled with the ``-fgnu-tm`` flag.
*libsei* has been tested with GCC version 4.7 in Linux environments.

See ``examples/simple`` for a template Makefile that demonstrates compilation
of the target application against *libsei*.


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

|

References
----------------
* `Scalable error isolation for distributed systems
  <https://www.usenix.org/conference/nsdi15/technical-sessions/presentation/behrens>`_ -  Diogo Behrens, Marco Serafini, Sergei Arnautov, Flavio P. Junqueira, and Christof Fetzer. *12th USENIX Symposium on Networked Systems Design and Implementation (NSDI'15)*

..
  * `Towards Transparent Hardening of Distributed Systems
  <http://dl.acm.org/citation.cfm?id=2524230>`_ - Diogo Behrens, Christof Fetzer, Flavio P. Junqueira, Marco Serafini, In Proceedings of the 9th Workshop on Hot Topics in Dependable Systems, ACM, 2013

