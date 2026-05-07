# CSC-364-Assignment-2  

## MustangChat

MustangChat (UDP-based client/server chat application). The client connects to a
server, automatically joins the `Common` channel, lets the user join and leave
channels, and sends messages to the current active channel.

## Build

```bash
make
```

## Run

If you'd like to test/run, here are the instructions:

Start the server in one terminal:

```bash
./server localhost 5000
```

Start one or more clients in separate terminals:

```bash
./client localhost 5000 alice
./client localhost 5000 bob
```

The client takes exactly three arguments:

```text
./client <host> <port> <username>
```

The server takes exactly two arguments:

```text
./server <host> <port>
```

## Supported Client Commands

```text
/exit              Log out and exit the client
/join <channel>    Join a channel and make it the active channel
/leave <channel>   Leave a channel
/list              List existing channels
/who <channel>     List users in a channel
/switch <channel>  Switch the active channel locally
```

## Example Test Run

Terminal 1:

```bash
./server localhost 5000
```

Terminal 2:

```bash
./client localhost 5000 alice
```

Terminal 3:

```bash
./client localhost 5000 bob
```

Example commands from Alice's client:

```text
> /list
Existing channels:
  Common

> /who Common
Users on channel Common:
  alice
  bob

> hello everyone
[Common][alice]: hello everyone

> /join Games
> hello games
[Games][alice]: hello games

> /switch Common
> hello common
[Common][alice]: hello common

> /leave Common
> /switch Games
> hello games again
[Games][alice]: hello games again

> /exit
```

## Notes

I implement this assignment in C. The client uses a connected UDP socket so it
can use `send()` and `recv()`, while the server uses `recvfrom()` and `sendto()`
so it can track each client by UDP address.

Notably, this implementation is more manual than if I had chosen something like
Python, but it also gives clearer control over memory layout, packet sizes, and
socket behavior. I wanted to implement a network protocol directly.