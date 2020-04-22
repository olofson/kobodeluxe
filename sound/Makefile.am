noinst_LIBRARIES = libsound.a

INCLUDES =	-I../eel -I../graphics @LOCALCFLAGS@

libsound_a_SOURCES = audio.c a_globals.c a_struct.c		\
	audiodev.c a_midi.c sfifo.c a_pitch.c a_midisock.c	\
	a_wave.c a_patch.c a_channel.c a_voice.c a_bus.c	\
	a_commands.c a_control.c a_midicon.c a_midifile.c	\
	a_plugin.c a_delay.c a_limiter.c a_filters.c		\
	a_wca.c a_agw.c a_sequencer.c a_events.c

EXTRA_DIST = *.h
