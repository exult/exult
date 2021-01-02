# Removal of X11 Drag and Drop xdnd in Exult
January 2021 by Dragon Baroque.

## Analysis

The Drag and Drop between ExultStudio and Exult uses a specialized X11 code in the receiver part in Exult on all Unix platforms.

On Windows platforms, Exult also contains an OLE Drag and Drop, that could also be unified into the framework presented in the current
document. This has not been evaluated, however. Moreover, the emitting part of the Drag and Drop in ExultStudio also contains a Windows
specialized variant, whose purpose is not evaluated either.

The use of X11 as the Drag and Drop protocol on Unix platforms forces the installation of the X11 emulator `XQuartz` as a prerequisite
to the installation of Exult packaged with ExultStudio on macOS. This is the requirement that this work attempts to remove.

Likewise, Linux platforms are moving away from X11 toward Wayland, and the X11 dependency from the Drag and Drop is a longer range
constraint to be removed.

## Proposal

SDL, the graphical application layer used by Exult - whereas ExultStudio uses GTK+ -, contains a very limited support of the Drag and Drop.
Limited, but powerful enough to justify the current proposal of removing the X11 specific code for the Drag and Drop in Exult and use the
SDL Drag and Drop instead.

SDL provides only `SDL_DROPFILE` as the Drag and Drop on all platforms. On X11 platforms only, it also provides `SDL_DROPTEXT`.
Thus `SDL_DROPFILE` has been chosen as the Drag and Drop medium for the communication between ExultStudio and Exult, and to avoid having
two different protocols in ExultStudio, for the internal Drag and Drop of ExultStudio too.

`SDL_DROPFILE` is a pure application of the Drag and Drop between, say, a File Explorer, and an SDL application. The user Drags a file from
the File Explorer into the SDL application. It uses the well-known target `text/uri-list`. Technically, and fortunately for Exult,
only the file identity is transferred, its existence is not even checked. Provided that ExultStudio can encode its data into an artificial
but valid file name, ExultStudio can emit its Drag and Drop data to the `text/uri-list` target, and the Exult and ExultStudio receivers
can extract the Dragged object data from the file name.

The use of `SDL_DROPFILE` has the obvious advantage of removing the need for an X11 specialized code - and should be checked for the
opportunity of removing the Windows specialized code, too - but it has several drawbacks :

1. Only the Drop event of the Drag and Drop protocol is reported to Exult. Whereas GTK+ also reports the Mouse Drag Motion events, too.
And the present X11 specialized code also handled the Mouse Drag Motion events, to provide a green grid to help the user to fine position
the dragged shape before actually dropping it.
2. The ExultStudio internal Drag and Drop as well as the ExultStudio to Exult Drag and Drop defined 4 targets, for the 4 different kind
of items that can be Dragged and Dropped, namely the NPCs, the Chunks, the Combos, and the regular Shapes. The `SDL_DROPFILE` uses only
one and well-known target `text/uri-file`. The code of ExultStudio has been altered along this requirement. But the single target delays
the acceptation or the rejection of a Shape into a dedicated Shape window - for example the Face and Body icons of the NPC Editor window -
until the user actually drops the Shape into the window.
3. The `SDL_DROPFILE` Drop event does not provide the mouse location. This is reasonable considering the actual purpose of the Drag and
Drop of files. Thus the `SDL_DROPFILE` event has to obtain the mouse current location to complete the Drop operation - to create a new
NPC or Chunk or Combo or Shape at the right place within the display window of the game.

## State of the code change

* `Makefile.am` : `xdrag.h` and `xdrag.cc` are no longer sources to be compiled and linked to Exult.
* `studio.cc` : The declaration of and the assignment to `xdnd` are removed. The handling of the `SYS_WMEVENT` events is removed.
The `SDL_DROPFILE` is handled, with a handling code borrowed from `xdrag.cc`, with the additional requirement of recognizing from
the data - thus from the file name - which of the 4 kinds of objects it has to handle.
* `shapes/u7drag.h` and `shapes/u7drag.cc` : The binary data marshalling is replaced by a `file name compatible` data marshalling.
Essentially, the artificial file name built for the `SDL_DROPFILE` is like **file:///U7SHAPEID._filenum_._shapenum_._framenum_** where
where the first token of the file name designates the kind of Dropped object, and each subsequent number is appended in decimal,
signed if needed, behind a separation dot.
Since the target no longer designates the kind of Dropped object, functions have been added to detect the kind of Dropped object
from the file name.
* `mapedit/chunklst.cc`, `mapedit/combo.cc`,`mapedit/npclst.cc`,`mapedit/shapedraw.cc` and `mapedit/shapelst.cc` : The Drag and Drop
target names have been replaced by the generic target name `text/uri-list`. However the `info` field is kept with its kind of object
specific value. This permits an early detection of the kind of object for the Drag and Drop receivers in ExultStudio. This feature
depends on the proper support of the `info` field on all platforms : _to be properly checked_. The size of the artificial file name
is computed from a C macro provided by `u7drag.h`. All internal Drag and Drop receivers double check the proper kind of Dropped object
using the new functions in `u7drag.cc`.

The code change compiles under Linux. It should compile under macOS. It should be evaluated for use under Windows too.

The code change runs under Linux, with X11 as the backend, with no apparent unexpected effect of the target name change, on all paths
of the Drag and Drop that I could test.

Dragon Baroque, January 2, 2021.
