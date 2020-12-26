# Removal of X11 ConnectionNumber xfd in Exult
December 2020 by Dragon Baroque.

## Analysis

The X11 `ConnectionNumber` is a Unix file descriptor used deep in the SDL Event Loop to be notified of incoming X11 User events.
Exult used also the file descriptor as part of a timed `select` call in two places :

1. `gumps/gump_util.h`, function `Delay()`. The purpose of the `Delay()` function, which might more properly have been named `Yield(),`
was to create a short duration - 20 to 50 milliseconds - timed out wait, using a `select` UNIX call that would complete either at the end of
the timeout or upon an incoming X11 event, whichever occurred first. The function was written to use the `xfd` X11 file descriptor
and the `select` call in the `XWIN` configuration. In the `_WIN32` configuration however, the same `Delay()` function performed
a simple `SDL_Delay(10)` for a pure 10 milliseconds wait with no heed to incoming user events.
2. `server/server.cc`, function `Server_delay()`. Very similar to `Delay()` in the `XWIN` configuration, but with the UNIX sockets
for an incoming connection or message from ExultSudio added to `xfd` in the `select` call.

The advantage of the X11 file descriptor `xfd` is its ability to be part of a `select` statement, which a boolean returning C function cannot be.
Its problem is that it ties the non Windows platforms to use X11, and that is becoming a burden :

* The `macOS` platform is a kind of Unix, but its system graphics is not X11, it is Quartz, and they are quite different.
One can install an Open Source package called `XQuartz` over Quartz to obtain a X11 compatibility layer, but it causes a performance penalty.
* The Linux platform is moving away from X11 towards Wayland, and while the X11 stack is probably remaining in the Linux distributions for some time,
the Wayland stack claims better performance.
* It might be nice to have one source code only without any platform specifics for these two services in all platforms, Windows included.

## Proposal

I propose to replace each function `Delay()` and `Server_delay()` by a ten turns - to be more reactive - or fifty turns - to respect the 50 milliseconds
original design - of a loop with two actions :

1. A call to `SDL_PumpEvents()` to refresh the SDL Event Queue followed immediately by a `SDL_PeepEvents()` fo check whether any SDL Event has shown up on
the Event Queue. It is deep in `SDL_PumpEvents()` that SDL itself makes use of the X11 file descriptor.
2. A simple `SDL_Delay(1)` for a 1 millisecond wait, deaf to incoming Events, for `Delay()`, or a `select` call over the ExultStudio sockets, timed out at
1 millisecond, for `Server_delay()`, which would terminate upon an incoming connection or message from ExultStudio, *but not a X11 event*.

My main reason for this proposal is the increase of precision of the hardware timers and the increase of power in the CPUs.
Indeed, high precision hardware timers based delays should incur very little drift even at the minimum scale of 1 millisecond supported by SDL.

## State of the code change

* `studio.cc` : The declaration of and the assignment to `xfd` are removed.
* `gumps/gump_util.h` : The function `Delay()` does not depend on the configuration, it uses the same loop of pure SDL calls `SDL_PumpEvents`, `SDL_PeepEvents`
and `SDL_Delay` on all platforms.
* `server/server.cc` : The function `Server_delay()` in the `not _WIN32` configuration is rewritten to use the loop of `SDL_PumpEvents`, `SDL_PeepEvents` and
UNIX `select`. The function `Set_highest_fd` does not use `xfd` any more.

The code change compiles under Linux. It should compile under macOS and under Windows.
The code change runs under Linux with no visible performance penalty, nor any visible CPU load increase. The latest could have been expected, considering that
the code change replaces a simple 10 or 50 millisecond wait by an active loop for 10 milliseconds that awakens and performs some work every 1 millisecond.

## Side notice

The `XWIN` configuration variable seems to have two different meanings :

1. A broad meaning of *Any platform Unix-like*, where its complement is `_WIN32`,
2. A narrow meaning of *Any Unix-like platform whose system graphics is X11*.

MacOS using Quartz is sitting uneasily on the boundary between the two meanings, and Linux using Wayland shall be soon.

Dragon Baroque, December 26, 2020.
