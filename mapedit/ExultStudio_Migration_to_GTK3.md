# Migration of ExultStudio from GTK+ 2 to GTK+ 3
November and December 2020 by Dragon Baroque.

## Process of Migration
I followed the recommended process shown in
[Migrating from GTK+ 2.x to GTK+ 3: GTK+ 3 Reference Manual] (https://developer.gnome.org/gtk3/stable/gtk-migrating-2-to-3.html).
The advice is good, it consists of modifying progressively the application while remaining in GTK+ 2, so that the application remains compilable and runnable.
The final switch to GTK+ 3 requires only a few more changes to the application.

GTK+ provides several `#define` macros which make GTK+ stricter and stricter at the compilation and cause compilation errors that can be fixed one at a time.
After a compilation error has been fixed, deactivating the corresponding `#define` macro returns GTK+  to lenient, thus the application can be compiled and tested again.
This play while in GTK+ 2, with activating the strictness macros, identifying and fixing one problem and deactivating the strictness macro to verify
that the application compiles and runs, goes on until all strictness macros are activated, no compilation errors or warnings remain, and the application works correctly.
At that point, the application does not use any GTK+ 2 deprecated feature any more.

The application can switch to GTK+ 3 at that time only. Compiling the application against GTK+ 3 with the strictness macros being deactivated causes new compilation errors,
but few in number, since most of them have been fixed before. Fixing all of them is a (short) time of blindness, since the required changes are not supported by GTK+ 2,
and since the application cannot be compiled or tested until all have been fixed.

Then the process of activating progressively the strictness macros takes place again, to ensure at the end that the application does not use any GTK+ 3 deprecated feature either.

## Migration of ExultStudio : Starting point

When I started the process, I was greatly helped by the fact that ExultStudio, a complex GUI application written in C++, compiled against GTK+ 2 without any warning,
even though many warnings were activated, actually many more than the standard `gcc` warnings requirements `-Wall` and `-Wextra`.
Moreover, ExultStudio could be compiled with both `GNU GCC g++` and `LLVM CLang clang++` compilers, adding another reason for confidence in the absence of warnings.

In addition, ExultStudio did not use any tricky system service that would have made the migration harder. Indeed the communication sockets that bind ExultStudio and Exult proper
were wrapped into GLib IO stream watches, perfectly integrated with GTK+ event management.

The absence of warnings even using GTK+ C style services in a C++ application compiled with `-Wold-style-cast` is explained with the magical `gtk_redefines.h`,
part of ExultStudio, that overrides some low level GTK+ and GLib macros to convert them from C style casts to C++ style casts.

## Migration of ExultStudio : The Strictness Macros of GTK+ 2 and 3
The Strictness Macros of GTK+ 2 and 3 are actually the same. They are

* `GTK_DISABLE_SINGLE_INCLUDES` : GTK+ requires a single `#include <gtk/gtk.h>` instead of various subcomponent `#include`. This macro causes a compilation error when a subcomponent
`#include` occurs in the application code. There remains an independent `#include <gdk/gdkkeysyms.h>` for the symbolic names of the keys of the keyboard.
* `GSEAL_ENABLE` : GTK+ no longer allows the direct access to the members in the `C struct` that define the various GTK+ objects. Instead, the `C struct` members can either be
accessed with accessor C function calls or as GLib properties. I used the C function calls.
* `GNOME_DISABLE_DEPRECATED` and `GTK_DISABLE_DEPRECATED` and `GDK_DISABLE_DEPRECATED` : These macros forbid the use of deprecated features in GTK+ and GNOME.
The GTK+ migration guide mentions only the GTK and GDK macros, but one should add the GNOME macro if the application uses some `libgnomeui` calls in addition to GTK+.
Otherwise the lenient GNOME `#include` will make use of services forbidden by strict GTK+, producing many compilation warnings and errors.

## Migration to GTK+ 3 : The Case of ExultStudio, GTK+ 2

### The symbolic names of the keyboard keys
As explained in the Migration document, the symbolic names of the keys receive an additional `_KEY` in their names.
ExultStudio used few such names in even fewer places, `GDK_Return` became  `GDK_KEY_Return`, `GDK_Delete` became  `GDK_KEY_Delete`, `GDK_Insert` became  `GDK_KEY_Insert`.

### The `GTK_DISABLE_SINGLE_INCLUDES` strictness macro
With `GTK_DISABLE_SINGLE_INCLUDES` enabled, all `#include` of GTK+ were concentrated into a single `#include <gtk/gtk.h>` in `studio.h`,
which is in turn `#included` by every `.cc` file, immediately after the `#include <config.h>` of the `GNU Autotools`.

In `studio.h` the `#include <gtk/gtk.h>` is wrapped in warning deactivation `#pragma GCC diagnostic ignored`
directives, which avoid warnings - many about the C style casts as opposed to the C++ style casts - which come because GTK+ is C oriented, not C++ oriented.
The `#include <gdk/gdkx.h>` and all `#include <gtk/gtk..some widget...h>` have been removed.

__Comment__ : Task rather quick to perform.

### The `GSEAL_ENABLE` strictness macro
With `GSEAL_ENABLE` enabled, many compilation errors occurred but few different kinds of error, such that each kind of error could be corrected across all source files.
All of them involved the access to a member of a `C struct` to be replaced by an accessor C function call.
The online Documentation of GTK+ 2 was very helpful in pointing out the recommended replacement function.

* `->allocation` to obtain the `GtkAllocation` of any `GtkWidget`, with replacement `gtk_widget_get_allocation`.
* `->window` to obtain the `GdkWindow` - not to be confused with the `GtkWindow` - of any `GtkWidget`, with replacement `gtk_widget_get_window`.
* `->upper`, `->lower`, `->value`, `->step_increment`, `->page_increment` and `->page_size` of a `GtkAdjustment`. Their replacement were the `gtk_adjustment_get...` and `gtk_adjustment_set...`
function calls. Special care was needed for `->value` in expressions with `+=` or `++` which would involve a call to the get function and a call to the set function.
* `->data`, `->type`, `->format` and `->length` on a Drag and Drop `GtkSelectionData`, with replacements `gtk_selection_data_get...`.
* `->children` on any `GtkContainer`, with replacement `gtk_container_get_children`.
* `->child` on any `GtkBin`, with replacement `gtk_bin_get_child`.
* Various `GtkWidget` flag setting and querying macros with internal `->flags`, with replacement functions `gtk_widget_set/has...`.
* `->active` on a `GtkCheckMenuItem`, with replacement `gtk_check_menu_item_get/set_active`.
* `->focus_widget` on a `GtkWindow`, with replacement `gtk_window_get_focus`.
* `->colorsel` on a Color Selection Dialog `GtkColorSelectionDialog` to retrieve the data bearer `GtkColorSelection`. The replacement was `gtk_color_selection_dialog_get_color_selection`.
* `->ok_button` and `->cancel_button` on a Color Selection Dialog, to place proper callbacks on the buttons.
There were no direct function replacement, instead the access to the buttons involved a GLib property.
That was actually before I realized that the Color Selection Dialog was a true `GtkDialog` and therefore could be simply managed with a simple `gtk_dialog_run` function call instead
of micro-managing the buttons.

__Comment__ : Easy task but tedious.

### The `GNOME + GTK + GDK_DISABLE_DEPRECATED` strictness macros - 1 Obsolete functions
The GTK+ 2 has deprecated a set of functions and types which were only wrappers around some GLib function or type, so that each replacement was the GLib function or type.

* `gtk_object_get/set_user_data` were wrappers around `g_object_get/set_data`.
* `gtk_signal_connect` and `gtk_signal_connect_object` were wrappers around `g_signal_connect` and `g_signal_connect_swapped`.
* `gtk_signal_emit_by_name` was a wrapper around `g_signal_emit_by_name`.
* `GTK_OBJECT` and `GtkObject` were wrappers around `G_OBJECT` and `GObject`.
* `GTK_SIGNAL_FUNC` and `GtkSignalFunc` were wrappers around `G_CALLBACK` and `GCallback`.
* `gtk_timeout_add` and `gtk_timeout_remove` were wrappers around `g_timer_add` and `g_timeout_remve`.
* `gdk_bitmap_unref` and `gdk_pixmap_unref` were wrappers around `g_object_unref`.
* `gtk_widget_ref` was a wrapper around `g_object_ref`.

ExultStudio navigated the rather obscure `GtkBoxChild` and `GtkTableChild` internal objects to find the children widgets of a `GtkBox` or a `GtkTable`.
These internal objects have been discarded, or made hidden, and the navigation of the children to any `GtkContainer` uses the `gtk_container_get_children` function.
The function provides directly the GList of the children `GtkWidget`. However, the GList is dynamically built and should be freed after use,
hence the additional `g_list_free` function calls in ExultStudio.

External inputs and outputs, and that means Unix file descriptors, which were handled directly by GTK+ 2 before, have been refactored as GLib IO Channels,
and the GTK+ callbacks have been replaced by GIOChannel watches, with a simultaneous change of callback signature. `execbox.cc` is impacted :

* `gdk_input_add` is replaced by a call to `g_io_channel_unix_new` to make a GIOChannel out of the file descriptor and to `g_io_add_watch` to setup the callback,
* The event callback extracts the file descriptor from the GIOChannel using the call `g_io_channel_unix_get`,
* `gdk_input_remove` is replaced by `g_source_remove`.

Some widget-specific C functions have been refactored to another - more generic - GTK+ object.

* `gtk_drawing_area_size` on a `GtkDrawingArea` is replaced by `gtk_widget_set_size_request` on any `GtkWidget`
* `gtk_widget_set_usize` on a `GtkWidget` is replaced by `gtk_widget_set_size_request` on any `GtkWidget`
* `gdk_window_get_colormap` on a `GdkWindow` is renamed `gdk_drawable_get_colormap` on the same `GdkWindow` seen as a `GdkDrawable`.
* `gtk_radio_menu_item_group` on a `GtkRadioMenuItem` is renamed `gtk_radio_menu_item_get_group`.
* `gtk_menu_append` on a `GtkMenu` is replaced by `gtk_menu_shell_append` on a `GtkMenuShell`.
* `gtk_button_box_set_spacing` on a `GtkButtonBox` is replaced by `gtk_box_set_spacing` on any `GtkBox`

I commented out calls to `gtk_range_set_update_policy` in mode `GTK_UPDATE_DELAYED` that GTK+ no longer supports.
I have not seen any visible impact of this change.

The `gtk_combo_box_get_active_text` is replaced by a rather complex sequence involving 

* Access to the underlying GtkTreeModel of the GtkComboBox with `gtk_combo_box_get_model`,
* Access to a Tree Iterator placed at the active item of the Combo Box with `gtk_combo_box_get_active_iter`,
* Content extraction from the GtkTreeModel by the GtkTreeIter with `gtk_tree_model_get`

The immediate effect repaint query `gtk_widget_draw` is replaced by a `gtk_widget_queue_draw` which GTK+ handles when available.
One could obtain the immediate processing of the Draw Area with a - now deprecated `gdk_window_process_updates` -, but leaving the scheduling
of the repaints to GTK+ is more natural, and the actual delay is not noticeable.

__Comment__ : More difficult task, slower. Each function involves an online search on the GTK+ official site, and its resolution depends on the replacement
function, if it exists, or the replacement code sequence, otherwise.

### The `GNOME + GTK + GDK_DISABLE_DEPRECATED` strictness macros - 2 Obsolete objects and switch to Cairo
GTK+ 2 has offloaded the GDK Drawing and Painting - of lines, curves, rectangles, images - to Cairo,
but has kept the GDK drawing-painting primitives as deprecated but supported.
GTK+ 3 no longer supports the GDK drawing-painting primitives.

#### The management of the `cairo_t` Graphic Context, some principles

From the outside, each GDK drawing-painting primitive appears properly mapped onto a Cairo corresponding primitive.
The key difference, which matters for the entire architecture of the application moving from GTK+ 2 to GTK+ 3, is the *lifetime of the drawing context*.
This object, a `GdkGC` in GDK+ 2, and a `cairo_t` in GTK+ 3 / Cairo, is tied to the target Drawing Area, and is required for each drawing-painting call onto the Drawing Area.

However, the lifetime of the `GdkGC` is close to eternal, one can be created in advance for each Drawing Area of the application, whereas the `cairo_t` is valid only
during the **expose-event** callback of the Drawing Area. It means that the Drawing Area can be drawn or painted upon only when it is exposed - displayed first, or
redisplayed in part or in full after it has been minimized or hidden. Any `cairo_t` Graphic Context created or used to draw or paint outside the **expose-event**
callback is without effect.

#### The management of the `cairo_t` Graphic Context, the large Chooser Drawing Areas

In ExultStudio, most drawing primitives are called either directly from the **expose-event** callback of the Drawing Area, or in the `show` method of the `Shape_draw`
C++ class or one of its inheritors. Whereas the drawing primitives located in the **expose-event** only have to be translated to Cairo, the drawing primitives of the
`show` method have to be switched on only when the `show` method is called by an **expose-event** of the Drawing Area. The **expose-event** callback
calls the `show` method, but is not alone in doing so.

Some outside events which intend to change the displayed picture pass through a `render` method which rebuilds a picture to be drawn, and call the `show` method to display it.
At that moment, outside the **expose-event** of the Drawing Area, the `cairo_t` Graphic Context is invalid, the picture is **not** rebuilt, and the Drawing Area is **not** refreshed.

The `show` method does not receive directly the Graphic Context as a parameter, instead it fetches it from the `Shape_draw` instance.
In the instance the `GdkGC` graphic context `drawgc` is simply replaced by the `cairo_t` Graphic Context.
The Graphic Context is managed differently, however, as the `GdkGC` was created once and for all during the
**configure** callback of the Drawing Area, whereas the `cairo_t` Graphic Context is now maintained to `nullptr` except while in the **expose-event** callback.

The `show` method has been altered to only use a not-`nullptr` `cairo_t` Graphic Context. The **expose-event** callback creates a `cairo_t` Graphic Context into `drawgc`
with a `gdk_cairo_create` at entry, destroys it with a `cairo_destroy` and resets the `drawgc` to `nullptr` at exit..

The outside events call a `render` method to rebuild the image that has to be displayed, but do not call directly the `show` method any more.
Instead the `render` method uses a ~~gdk_window_invalidate_rectangle~~ `gtk_widget_queue_draw` call to request a repaint of the Drawing Area.
In turn, the repaint request schedules an **expose-event** onto the Drawing Area, and the `show` method called at that time by the **expose-event**
callback is in the right conditions to obtain the picture built by `render` and to display it with the then valid `cairo_t` Graphic Context.

On this respect, the `show` and `render` method of the `Shape_chooser` and the other chooser classes, are neatly separate. `render` builds a pixel image out of the various
`Shapes`, taking however into account the size of the target Drawing Area, and `show` displays the pixel image onto the Drawing Area using Cairo primitives.
Thus, whereas `render` can be called at any time, `show` only use is within the **expose-event** callback of the Drawing Area, when the Graphic Context is valid.

#### The management of the `cairo_t` Graphic Context in the smaller other Drawing Areas

The secondary Drawing Areas, within the various Editors of ExultStudio, do not present this proper separation. Indeed the Combo Editor displays a large Drawing
Area using a single method `render_area`, and the various Editors display a total of ten Drawing Areas of single Shape size using `show_...` methods to manage them.

These methods all rebuild the picture then issue the call to `Shape_draw::show` to display it. The methods are called both from the **expose-event**
of the corresponding Drawing Areas, and from the outside events - keyboard, mouse - that alter the Shape. The cost of rebuilding the picture of a single or of
a few shapes was considered minimal, at least compared to the complex management of the Shapes in the large Drawing Areas of the `Shape_chooser`. 
Thus these methods do not split naturally into `render` and `show` so well.

Their architecture with GTK+ 3 has been altered thus :

1. If they are called from the **expose-event** callback, they perform the picture build and pass it to `Shape_draw::show` for display,
2. Otherwise they only perform a `gdk_widget_queue_draw` onto their private Drawing Area and return.

#### The smaller set of picture formats supported by Cairo

A second significant difference between the painting by GDK and by Cairo is the breadth of the GDK supported image formats compared to Cairo. In essence Cairo supports
only True Color RGB24 or ARGB32 pictures, where each pixel has a color in 256 levels Red and 256 levels Green and 256 levels Blue - and optionally, 256 levels Transparency or Alpha.

To ExultStudio, which manages essentially `PC Bios mode 13` pictures, where each pixel has a color among 256 in a 256 entries color table or palette, this means
that the final picture, acceptable to Cairo, has to be the True Color RGB24 24 bits per pixel transform by the palette of the original 8 bits per pixel picture.
Whereas GDK supports directly the 8 bits per pixel picture format and its palette in the `gdk_draw_indexed_image` primitive, Cairo does not support the format.

As a consequence, all GDK palette primitives and classes have become irrelevant, are flagged as deprecated in GTK+ 2 Lenient, and do not exist any more in GTK+ 2 Strict or GTK+ 3.
Not only `GdkGC` but also `GdkRgbCmap`, `GdkBitmap`, `GdkPixmap`, and all primitive functions named like `gdk_draw...`, `gdk_rgb...`, `gdk..cmap...`, `gdk_bitmap...` and
`gdk_pixmap...` have been withdrawn.
Since ExultStudio still needs a palette class or 256 colors, the removed `GdkRgbCmap` used by ExultStudio to store the game palettes has been replaced by an `ExultRgbCmap`
with the exact same layout and content.

#### How ExultStudio displays images

1. ExultStudio uses Cairo to display updatable images : all categories of Shapes, and the Palettes.
2. ExultStudio binds static images to the Drag and Drop cursor and to the Prompt dialog, without Cairo.

ExultStudio uses Cairo :

1. To paint the `Shapes` in the various `Chooser` classes. Be they `Npcs`, `Chunks`, `Eggs`, `Containers` or regular `Shapes`, all are painted by the `show` method of the `Shape_draw` class.
`Shape_draw::show` builds a RGB 24 without transparency `GdkPixbuf` out of the 8 bit per pixel picture from `render`, binds it to a Cairo source surface,
and applies ~~cairo_paint~~ `cairo_fill` to the clip rectangle from the Cairo surface source. *To remind,* GdkPixbuf comes from the package `gdk-pixbuf`,
which is separate and quite independent from GTK+. GDK provides GdkPixbuf binds to Cairo, indeed the GdkPixbuf is the approved way to import and export picture binary data into and from Cairo.
2. To frame the various `Shapes` in a yellow rectangle when the `Shape` is selected by the user. This is done by the `show` methods of the various `Chooser` classes
right after the call to `Shape_draw::show` which paints the `Shapes`.
The Cairo primitives are simpler than for painting the `Shapes` : `cairo_set_source_rgb` for he color, `cairo_rectangle` and `cairo_stroke` for the
rectangle. Very similar is the drawing of the matrix of squares in `Locator::render` with the additional use of `cairo_move_to` and `cairo_line_to` to draw the straight horizontal and vertical lines.
3. To display single color rectangles in `Palette_edit::show`, where I replaced the `gdk_draw_indexed_image` of an 8 bit picture with palette built in `Palette_edit::render`,
by the direct painting of 256 rectangles using `cairo_set_source_rgb` for the color, `cairo_rectangle` and `cairo_fill` for the rectangle.
Similar to this is the painting of the background in `Locator::render`, the font color sample in `Shape_chooser::on_new_shape_font_color_draw_expose_event`
and the sample background color in `ExultStudio::on_prefs_background_expose_event`.

ExultStudio does not use Cairo :

1. In `Shape_draw::set_drag_icon` to build the Drag and Drop cursor attached icon.
The picture is built, very much like in `Shape_draw::show`, as a GdkPixmap in True Color ARGB32 - with transparency to let show the background map
during the Drag and Drop move.
The `GdkPixbuf` is then bound to the current Drag and Drop Context `GdkDragContext` with a `gdk_drag_set_icon_pixbuf`, for the lifetime of the Drag and
Drop operation.
2. The Prompt Dialog displays an `Ankh` symbol on the left. A `GdkPixbuf` is built from the `Ankh` icon in XPM format, embedded in the C++ source,
then the `GdkPixbuf` data is used to build a `GtkImage` widget with `gtk_image_new_from_pixbuf`, and finally the `GdkImage` is inserted into the
`GtkBox` of the `prompt3_dialog` about to be displayed.

A side, not Cairo related code change is the replacement of `gtk_color_selection_get/set_color` with the color being a C array 4 doubles,
by a `gtk_color_selection_get/set_current_color` with the color being `GdkColor`.

__Comment__ : This stage is the heart of the Migration process. This is where the most time is spent, although the application does not even use GTK+ 3 yet.
It is also the most mind challenging and the most interesting, because
it requires a deep understanding of the **expose-event** and **gdk_widget_queue_draw** logic, otherwise the Drawing Areas remain black, or
cause the whole display to flash like crazy if the user moves, or resizes, or hides and redisplays the application window.

__Comment__ : At the end of this stage, the application fully compiles, without errors, without warnings, against Strict GTK+ 2.
And the application fully works, too.

_It is time for a pause, and a solid backup. Because the switch to GTK+ 3 is_ **now**.

## Migration to GTK+ 3 : The Case of ExultStudio, GTK+ 3

### GTK+ 3 Lenient, the Compatibility issues
At this point, I switched the GTK+ package to GTK+ 3. And I returned to lenient GTK+, more exactly, I kept the `GTK_DISABLE_SINGLE_INCLUDES` and the `GSEAL_ENABLE` and
I removed the three `GNOME_DISABLE_DEPRECATED` and `GTK_DISABLE_DEPRECATED` and `GDK_DISABLE_DEPRECATED`.

GTK+ 3 Lenient is not totally compatible with GTK+ 2 Strict :

* There are some new GTK+ 3 widgets replacing some GTK+ 2 widgets,
* There are some new GDK 3 objects replacing some GDK 2 objects,
* There is a major API change : the **expose-event** event and callback become the **draw** event and callback, and the callback has a different parameter signature :
the `GdkEventExpose` Event argument is replaced by a valid `cairo_t` Graphic Context.

I had the objective to keep a single set of source files, compatible with both GTK+ 2 and GTK+ 3. With the changes in the widgets, the objects, the **expose-event** callback
name and argument signature, I needed to introduce _dual code_, like :

```c
#if GTK_CHECK_VERSION(3, 0, 0) // GTK+ 3
... GTK+ 3 variant
#else
... GTK+ 2 variant
#endif // GTK+ 3
```

#### The easy part, first.

The color in GTK+ 2 is stored into a `GdkColor` a `C struct` where the 3 colors Red, Green and Blue are held in a `guint16`, equivalent to an `unsigned short`.
The color in GTK+ 3 is preferably stored into a `GdkRGBA`, a `C struct` where the 3 colors Red, Green and Blue, plus the Alpha are held in a `gdouble`, 
equivalent to a `double`.

Better precision, support for transparency, these are good advantages to switch, and even though lenient GTK+ 3 supports the `GdkColor`,
the new Color Choosing widget does not. Consequence : _dual code_, but simple, with the replacement of bit masks over `unsigned short` by floating point divides by `255.0`.

The Color Choosing widget then. In GTK+ 2, this was the `GtkColorSelectionDialog`. In GTK+ 3 this is the `GtkColorChooserDialog`. Neither exist in the world
of the other one. _Dual code_ again, but simple too, because apart from the object name and the switch from `GdkColor` to `GdkRGBA`, the new `GtkColorChooserDialog`
and the older `GtkColorSelectionDialog`, being two `GtkDialog` that support `gtk_dialog_run`, are almost exact replacement.

`GtkColorChooserDialog` is even marginally simpler as there is not longer a need to a function call like `gtk_color_selection_dialog_get_color_selection`
to reach the `GtkColorSelection` from the `GtkColorSelectionDialog`.
Instead a simple GTK+ style cast `GTK_COLOR_CHOOSER` gives the `GtkColorChooser` from the `GtkColorChooserDialog`.

#### Some minor incompatibilities, then.

`gtk_drag_begin` is replaced by `gtk_drag_begin_with_coordinates` with the same parameter signature, plus two arguments at the end that are defaulted with `-1`.
Simple _dual code_.

The **cursor-changed** callback on a `GtkTreeView` may be called while the `GtkTreeView` is still empty. Anyway, the `gtk_tree_view_get_cursor` may return
a null `GtkTreePath` as well as a null `GtkTreeViewColumn`. And ExultStudio in GTK+ 2 checked only that the `GtkTreeViewColumn` was valid - non null. In GTK+ 3
it has to check that the `GtkTreePath` is valid too. The two checks are reasonable even in GTK+ 2, thus there is no need for _dual code_.

#### The switch from **expose-event** to **draw**, to finish.

Switching to **draw** required _dual code_ too. Worse, most callbacks are defined in the XML Glade file, and there are no way to obtain _dual code_ in XML.i
Thus I had to choose between :

* Reintegrate all **expose-event** `g_signal_connect` in the C++ source code, where I could place then in _dual code_, or
* Assume that I would end up with two different Glade files, one for GTK+ 3, one for GTK+ 2.

I preferred the first one, to remain with a single source Glade file, even though the end of the migration process forced onto me to have two different Glade files.
By reintegrating the `g_signal_connect` of the **expose-event** / **draw** callbacks in the C++ code, I could also give them a C++ instance `user_data` and define
them as C++ class methods, ready to invoke protected methods or access protected data within the instance. By the way, I reviewed the signature of some callbacks
from returning `gint` to returning `gboolean`.

Thus I wrote _dual code_ for all **expose-event** / **draw** `g_signal_connect` statements, and for the callback themselves, because of their different
signature. The code was rather small anyway, as I just had to replace the statements that built and destroyed the `cairo_t` Graphic Context, now directly passed
to the callback as a paramter, by a rebuild of the missing Event, more precisely the missing clip rectangle extracted from the `GdkEventExpose`.
The solution was the use of `gdk_cairo_get_clip_rectangle` to extract the event area rectangle from the `cairo_t` Graphic Context.
Indeed the GTK+ 3 code that builds - and destroys - the Graphic Context around the call to the **draw** callback, builds
it with the clip rectangle set to the Expose Event area, exactly the data needed by the C++ functions called from the **expose-event**.

__Comment__ : It looks like a small and feasible task, and indeed it is. And it can be made progressively. Of course I had first to fix the incompatibilities
listed above to get ExultStudio to compile. And of course the old **expose-event** event could no longer be bound using `g_signal_connect` to the **expose-event**
event, otherwise GTK+ 3 killed the application with a Critical Error.

But I could handle each callback one at a time, binding it using `g_signal_connect` to the **draw** event, and rewrite it for the signature switch.
All along ExultStudio still started, displayed its windows, reacted properly to all the buttons, menus, notebook pages, combo boxes, spin buttons.
And progressively each `GdkDrawingArea` worked again once its callback was bound again to the **draw** event using `g_signal_connect`, and its code was 
adjusted for the signature change.

### GTK+ 3 Strict, the Deprecated Functions and Objects in the C++ code

#### The few deprecated functions, first
With the GTK+ strictness macros activated again, a few GTK+ 3 deprecated functions became forbidden, and required some _dual code_ :
* `gdk_menu_popup` with its mostly `nullptr` argument is replaced by a simpler `gtk_menu_popup_at_pointer`.
* `gdk_window_get_pointer` is replaced by `gdk_window_get_device_position` where the GdkDevice is obtained by a rather complex sequence of `gdk_window_get_display`,
`gdk_display_get_default_seat` and `gdk_seat_get_pointer`. Truly this sequence is run when the mouse event is only a hint, and the hint mode of handling the mouse
is deprecated too...

#### The deprecated widgets, `GtkH...` and `GtkV...`, and the `GtkTable`, then

ExultStudio used explicitly the horizontal subclassed widgets `GtkHBox`, `GtkHButtonBox`, `GtkHScrollbar`, `GtkHSeparator` and the vertical subclassed
widgets `GtkVBox`, `GtkVButtonBox`, `GtkVScrollbar`, `GtkVSeparator` which are deprecated in GTK+ 3, with replacement the parent widget `GtkBox`, `GtkButtonBox`,
`GtkScrollbar`, `GtkSeparator` which provide widget creation functions.

 _Dual code_ again, since GTK+ 2 does not provide the parent widget creation functions.
The creation functions were mostly a replacement of the creation functions of the subclassed widgets, with an additional parameter for the orientation, expect
for the `GtkBox` which moved the `homogeneous` parameter of the subclassed widget into a subsequent `gtk_box_set_homogeneous` function call.

The `GtkTable` is deprecated and its replacement is the `GtkGrid`. The `GtkTable` was defined within the Glade file, but filled in the C++ code.
I reintegrated its creation into the C++ code as _dual code_ `GtkTable` vs `GtkGrid`, and gave an appropriate name to its parent `GtkViewport` in the Glade file to retrieve it in the C++ code.
I also changed the signature of a few C++ methods in ExultStudio from `GtkTable *` to `GtkContainer *`, to minimize the need of _dual code_.

 The _dual code_ is limited to the creation of the widget `gtk_table_new` becoming `gtk_grid_new`, and the attachment of the children widgets with `gtk_table_attach` becoming `gtk_grid_attach`
with the parameters subtly different. The Fill and Expand attributes, present in the `gtk_table_attach`, are transferred to properties of the children widgets.

#### The two deprecated classes, `GtkMisc` and the `GtkStock` Stock objects, finally

The GTK+ 2 `GtkMisc` was a superclass widget of `GtkLabel`, `GtkArrow`, `GtkImage` and `GtkPixmap`.
With both `GtkArrow` and `GtkPixmap` deprecated in GTK+ 3, `GtkMisc` is deprecated too, with its alignment and padding properties transferred to `GtkLabel` and `GtkImage`.
Thus the removal of `GtkMisc` in ExultStudio replaced the `gtk_misc_set_alignment` on a `GtkMisc` by the `gtk_label_set_x/y_align` on a `GtkLabel`.

More tricky was the deprecation of the Stock objects. All buttons in ExultStudio carry a standard - Stock - icon, and GTK+ 3 deprecates them. Likewise, the `GtkArrow`,
a Stock `GtkImage` that can be inserted in a `GtkButton`, is deprecated. The replacement process for both involves creating a `GtkImage` from a GTK Theme provided icon
with a `gtk_image_new_from_icon`. The `GtkImage` is then attached to the button with a `gtk_button_set_image`, which complements the `gtk_button_set_label` of the button text.

### GTK+ 3 Strict, the Deprecated Objects in the Glade file

At that point, I had to fork the Glade file, one for the GTK+ 2 ExultStudio, one for the GTK+ 3 ExultStudio.
Ensuring that the Glade file was not more dependent on any deprecated widget meant that a significant part of the Glade file became dependent on the GTK+ version, with 
widgets of GTK+ 2 being deprecated in GTK+ 3, with their GTK+ 3 replacement not provided by GTK+ 2 : for one, and by far the most frequent, the `GtkHBox` and `GtkVBox`
being replaced by the `GtkBox`. Reintegrating all these widgets into the C++ code with `#if GTK_CHECK_VERSION` would have made the Glade file near empty and useless.

All the transformations have been done with a script written in `Python` using the XML library `BeautifulSoup`. To perform the changes by hand, on an XML file of about 
22000 lines, would have been unbearably slow and prone to error, whereas the `Python` script, in its most recent form, requires 30 seconds to process the GTK+ 2 Glade
file to provide a compliant GTK+ 3 Glade file. Of course, each elementary change has been explored and validated on a simple case with the Glade tool itself.

#### All underscores replaced by dashes in the `name` of the `property`

Compared to the GTK+ standard, where the properties are named with dashes joining the words, the GTK+ 2 Glade file used only underscores instead. The file was accepted
by Glade and by the `GtkBuilder` of GTK+ 2 and GTK+ 3, but Glade would rewrite all properties with dashes on a simple change of a single property of a single widget,
making any file to file comparison near impossible, because the few pertinent XML changes would be drowned by the property renames.

#### Some simple removals : `primary/secondary-icon-sensitive`, `placeholder`, `margin-left/right`

Then the `Python` script removed the `primary-icon-sensitive` and `secondary-icon-sensitive` properties, the `placeholder` pseudo child widget of a `GtkTable`, and merged
the deprecated `margin-left` with the `margin-start` properties and the deprecated `margin-right` with the `margin-end` properties.

#### Some simple widget transforms : the horizontal and vertical subclassed widgets into their generic parent

Primarily the `GtkHBox` and the `GtkVBox` object nodes transformed into `GtkBox` object nodes with the additional `orientation` property taking the `horizontal` or `vertical`
value. Similarly for `GtkHButtonBox` and `GtkVButtonBox` to `GtkButtonBox`, `GtkHSeparator` and `GtkVSeparator` to `GtkSeparator`, `GtkHPaned` and `GtkVPaned` to `GtkPaned` and
`GtkHScale` and `GtkVScale` to `GtkScale`. There may be other such triples of widgets, but those were the ones that ExultStudio uses.

#### The Removal of the `GtkAlignment` and `GtkMisc` intermediates

The properties carried by the deprecated `GtkAlignment` and `GtkMisc` intermediate widgets are transferred to the child widget of the `GtkAlignment` and to the `GtkLabel` child
widget of the `GtkMisc`. With transfer rules well documented :

* `GtkAlignment` : Integer Padding properties are transferred to child Integer Margin properties on both sides,
* `GtkAlignment` : Float Scale properties are transferred to child Boolean Expand properties,
* `GtkAlignment` : Float Align properties are transferred to child Align properties, either Enumerate `h/valign` for a non `GtkLabel` child widget or Float `x/yalign` for a GtkLabel child widget,
* `GtkMisc` : Padding properties are transferred to child Margin properties on both sides.

#### The transformation of the `GtkTable` into the `GtkGrid`

On the paper, the `GtkGrid` is simpler than the `GtkTable`. This is because most properties of the `GtkTable` `packing` property set are added to the children `GtkWidget`.
Strangely the `GtkGrid` does not carry any more the explicit numbers of rows and columns in properties but in an XML comment instead.
The `homogeneous` property if present is replicated into `column-homogeneous` and `row-homogeneous` properties.

The four attachment numbers to the start and stop rows and columns change semantics and order.
The `packing` properties follow similar rules as the `GtkAlignment` properties :

* Enumerate Options properties are transferred to child Boolean Expand and Enumerate Align properties,
* Integer Padding properties are transferred to child Integer Margin properties on both sides.

#### The replacement of the Stock icons

The various icons of the buttons that ExultStudio uses are retrieved as independent `GtkImage` objects in the XML Glade file, one `GtkImage` per button icon.
Then an `image` property to the `GtkButton` has a `name` equal to the `id` of the `GtkImage`, and the `use-stock` property is removed.
ExultStudio uses the standard buttons, for which the standard GTK+ icon theme provides the `GtkImage` :

* OK + Apply, and Cancel : `gtk-ok` and `gtk-cancel`,
* Close  and Edit : `gtk-close` and `gtk-edit`,
* Delete : `edit-delete`,
* Play : `media-playback-start`,
* Up and Down : `go-up` and `go-down`,
* New and Open : `document-new` and `document-open`,
* Add and Remove : `list-add` and `list-remove`.

### The chase of the wrongly sized windows, and the way to retain the end size of the main window

ExultStudio stores the final size of the main window in the configuration file, to start its next session with the recorded size.
But with GTK+ 3, the size can be captured no later that in the callbacks for the `delete-event` of the main window and the `activate` callback
of `Quit` menu item. In GTK+ 2 ExultStudio took the sizes with the main window already removed, but its size was still available. No longer in GTK+ 3.

I chased for days the reason why the main `GtkDrawArea` of the Shapes panel displayed itself much too small, and seemed insensitive to 
`gtk_widget_set_size_request`, whereas the main window handled properly the `gtk_widget_set_size_request`.

I expected a regular increase of the requested size going from child widget to parent widget all the way up to the top level window.
Indeed the parent widget size be larger than its children sizes to encompass the total sizes of its children widgets plus some margins for itself.
It just happened that one widget, from the Shapes `GtkDrawingArea` to the main window, violated this rule of increase, and I discovered
that its `shrink` property was set to `True` in the Glade file. Turning it to `False` fixed the problem.

__Comment__ Voila. ExultStudio compiles and runs with GTK+ 3.

Dragon Baroque, December 17, 2020.
