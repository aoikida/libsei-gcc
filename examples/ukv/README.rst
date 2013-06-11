ukv - A dead-simple micro key-value store
=========================================

ukv is an example application for libasco and tmasco. In the moment
ukv allows a single client to connect to the server and support three
commands: set, get and delete.

The telnet interface
--------------------

To set a key send `+key,value` via telnet. There are two possible
answers. A successful set returns `!`, and an unsuccessful set returns
`!old value`.

To read a key send `?key` via telnet. If the key is empty, ukv returns
`!` otherwise `!value`.

Finally to delete a key simply send `-key`. ukv always returns `!` on
delete.


Compiling and running
---------------------

To compile ukv type::

  scons

To run ukv type::

  ./ukv-server 10000

And then start a client::

  telnet localhost 10000



