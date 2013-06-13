ukv - A dead-simple micro key-value store
=========================================

ukv is an example application for libasco and libtmasco. In the moment
ukv allows a single client to connect to the server and support three
commands: set, get and delete.

Compiling and running
---------------------

To compile ukv type::

  make

In the ``build`` subdirectory two binaries will be created:
``ukv-server`` and ``ukv-server.tmasco``.

To run the service type::

  ./ukv-server[.tmasco] 10000

And then start a telnet client::

  telnet localhost 10000

Try passing some commands via telnet such as ``+k,v`` and then ``?k``.


The telnet interface
--------------------

To set a key send ``+key,value`` via telnet. There are two possible
answers. A successful set returns ``!``, and an unsuccessful set returns
``!old value``.

To read a key send ``?key`` via telnet. If the key is empty, ukv returns
``!`` otherwise ``!value``.

Finally to delete a key simply send ``-key``. ukv always returns ``!`` on
delete.
