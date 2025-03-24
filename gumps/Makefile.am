AM_CPPFLAGS = -I$(top_srcdir)/headers -I$(top_srcdir) -I$(top_srcdir)/files \
		-I$(top_srcdir)/imagewin -I$(top_srcdir)/shapes -I$(top_srcdir)/objs \
		-I$(top_srcdir)/audio -I$(top_srcdir)/audio/midi_drivers \
		-I$(top_srcdir)/conf -I$(top_srcdir)/usecode -I$(top_srcdir)/data \
		-I$(top_srcdir)/shapes/shapeinf \
		$(SDL_CFLAGS) $(OGG_CFLAGS) $(INCDIRS) \
		$(WINDOWING_SYSTEM) $(DEBUG_LEVEL) $(OPT_LEVEL) $(WARNINGS) $(CPPFLAGS)

if BUILD_EXULT
noinst_LTLIBRARIES = libgumps.la

libgumps_la_SOURCES = \
	Actor_gump.cc	\
	Actor_gump.h	\
	Book_gump.cc	\
	Book_gump.h	\
	File_gump.cc	\
	File_gump.h	\
	Gamemenu_gump.cc	\
	Gamemenu_gump.h	\
	Gump.cc	\
	Gump.h	\
	Gump_button.cc	\
	Gump_button.h	\
	gump_utils.h	\
	Gump_widget.cc	\
	Gump_widget.h	\
	misc_buttons.cc	\
	misc_buttons.h	\
	Modal_gump.h	\
	Notebook_gump.cc	\
	Notebook_gump.h		\
	Paperdoll_gump.cc	\
	Paperdoll_gump.h	\
	Scroll_gump.cc	\
	Scroll_gump.h	\
	Sign_gump.cc	\
	Sign_gump.h	\
	Slider_gump.cc	\
	Slider_gump.h	\
	Slider_widget.cc	\
	Slider_widget.h	\
	StringList_widget.cc \
	StringList_widget.h  \
	IterableGump_widget.h \
	Spellbook_gump.cc	\
	Spellbook_gump.h	\
	Stats_gump.cc	\
	Stats_gump.h	\
	Text_gump.cc	\
	Text_gump.h	\
	Yesno_gump.cc	\
	Yesno_gump.h	\
	Newfile_gump.cc	\
	Newfile_gump.h	\
	AudioOptions_gump.cc	\
	AudioOptions_gump.h	\
	VideoOptions_gump.cc	\
	VideoOptions_gump.h	\
	GameDisplayOptions_gump.cc	\
	GameDisplayOptions_gump.h	\
	GameEngineOptions_gump.cc \
	GameEngineOptions_gump.h \
	Modal_gump.cc \
	Modal_gump.h \
	Mixer_gump.cc \
	Mixer_gump.h \
	Gump_ToggleButton.cc	\
	Gump_ToggleButton.h	\
	CombatStats_gump.cc	\
	CombatStats_gump.h	\
	gump_types.h	\
	Face_button.cc	\
	Face_button.h	\
	Jawbone_gump.cc	\
	Jawbone_gump.h	\
	Gump_manager.cc	\
	Gump_manager.h	\
	Face_stats.cc	\
	Face_stats.h	\
	Text_button.cc \
	Text_button.h	\
	Enabled_button.cc \
	Enabled_button.h \
	ShortcutBar_gump.cc \
	ShortcutBar_gump.h \
	ItemMenu_gump.cc \
	ItemMenu_gump.h	\
	InputOptions_gump.cc	\
	InputOptions_gump.h \
	AdvancedOptions_gump.cc \
	AdvancedOptions_gump.h \
	ConfigSetting_widget.cc \
	ConfigSetting_widget.h \
	DropDown_widget.cc \
	DropDown_widget.h \
	Scrollable_widget.cc \
	Scrollable_widget.h
endif

CLEANFILES = *~
