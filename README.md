# enet, sokol, nuklear, pthreads example

This is an example program using:
[enet](https://github.com/lsalzman/enet)
[sokol](https://github.com/floooh/sokol)
[nuklear](https://github.com/Immediate-Mode-UI/Nuklear)
[pthreads](https://en.wikipedia.org/wiki/Pthreads)

If you find any issues or would like to see something implemented (something smalle),
make an issue. I would like for this to be useful to someone, so if it can be better,
that would be great :)


## Building

This was only built on linux, no idea if it works on other platforms.
You will need the meson build system, and some backend for meson (probably ninja).


**Dependencies**

pthreads is the only thing you might(?) need to install.
enet, sokol, and nuklear are included in the 'libs' folder.


**Build**

```
meson build
ninja -C build
```

The binary 'enet_test' will be in the build directory


## Running

There will be 4 windows when you run the program, the server window and 3 clients.
Launch the server with the 'launch' button.
Client 0 is always available (it runs on the main thread).
Clients 1 and 2 have to be launched (similar to the server), as they run on their
own threads.
Clients can connect or disconnect to the server with the given buttons.

In the server window, the 'send packet to all clients' will send a packet to all
of the clients connected to the server.
Below each listed client in the server window, the 'send packet' button will send
a packet to that individual client.

In the client windows, 'send packet' will send a packet to the server.
'send packet to all' sends a packet to the server, which triggers the server to
send a packet to all connected clients.

Notices about sent and received packets, as well as launched/shutting down
servers/clients, are printed on the console.

To exit, press ESC or close the window.
