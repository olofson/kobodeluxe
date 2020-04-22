KOBOGFX_OBJECTS = *.png

if BUILD_MACOSX_BUNDLE
kobogfxdir = $(srcdir)/../@PACKAGE@.app/Contents/Resources/gfx
else

if BUILD_SIMPLE_BUNDLE
kobogfxdir = $(srcdir)/../@PACKAGE@/gfx
else
kobogfxdir = $(datadir)/kobo-deluxe/gfx
endif

endif

kobogfx_DATA = $(KOBOGFX_OBJECTS)

EXTRA_DIST = $(KOBOGFX_OBJECTS)
