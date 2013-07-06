libasco
=======


Compile the library::

  % make [OPTIONS]


Then copy ``build/libasco.a`` to your somewhere in your
LD_LIBRARY_PATH and copy ``include/*.h`` to an include directory used
by your compiler.

Options are:
* ``DEBUG=X``: `X` might be a value between 0 and 3, where 0 means no
  logging and 3 means very verbose. If ``DEBUG`` is not given, the
  library is compiled with inlining and ``-O3``.

* ``MODE=heap|cow``: ``heap`` use two heaps and ``cow`` uses only
  copy-on-write buffers.

.. and ``instr`` only instruments the code.


Example:
--------

See ``examples/ukv``.

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
