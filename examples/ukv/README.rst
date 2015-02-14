ukv - A dead-simple micro key-value store
=========================================

ukv is an example application for libsei. In the moment ukv allows a single
client to connect to the server and supports three commands: set, get and
delete.

Compiling and running
---------------------

To compile ukv type::

  make

In the ``build`` subdirectory three binaries will be created:
``ukv-server``,  ``ukv-server.sei`` and ``ukv-client.sei``.

To run the service type::

  ./ukv-server.sei 10000

And then start a client::

  ./ukv-client.sei localhost 10000

Try passing some commands such as ``+k,v`` and then ``?k``.

Telnet client can be used with not hardened variant ``ukv-server``::

  telnet localhost 10000


The interface
--------------------

To set a key send ``+key,value``. There are two possible answers. A successful
set returns ``!``, and an unsuccessful set returns ``!old value``.

To read a key send ``?key``. If the key is empty, ukv returns ``!`` otherwise
``!value``.

Finally to delete a key simply send ``-key``. ukv always returns ``!`` on
delete.
