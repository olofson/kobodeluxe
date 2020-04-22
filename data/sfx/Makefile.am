KOBOSFX_OBJECTS = *.agw *.mid *.h

if BUILD_MACOSX_BUNDLE
kobosfxdir = $(srcdir)/../@PACKAGE@.app/Contents/Resources/sfx
else

if BUILD_SIMPLE_BUNDLE
kobosfxdir = $(srcdir)/../@PACKAGE@/sfx
else
kobosfxdir = $(datadir)/kobo-deluxe/sfx
endif

endif

kobosfx_DATA = $(KOBOSFX_OBJECTS)

EXTRA_DIST = $(KOBOSFX_OBJECTS)

CLEANFILES = *_c.raw
