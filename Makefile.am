warningflags = -Wall -Wextra -Wshadow
includeflags = -I$(srcdir)/src/shared
commoncflags = -fPIC -O2 $(warningflags) $(includeflags)
AM_CXXFLAGS = -std=c++11 $(commoncflags)
AM_CFLAGS = -std=c99 $(commoncflags)



moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose_$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC     " $@;


moc_%.cpp : %.h
	$(moc_verbose)$(MOC) -o "$@" "$<"

bin_PROGRAMS = wobbly

moc_files = src/wobbly/moc_PresetTextEdit.cpp \
			src/wobbly/moc_WobblyWindow.cpp


MOSTLYCLEANFILES = $(moc_files)

wobbly_SOURCES = src/wobbly/PresetTextEdit.cpp \
				 src/wobbly/PresetTextEdit.h \
				 src/wobbly/Wobbly.cpp \
				 src/wobbly/WobblyWindow.cpp \
				 src/wobbly/WobblyWindow.h \
				 src/shared/WobblyProject.cpp \
				 src/shared/WobblyProject.h \
				 src/shared/WobblyException.h \
				 $(moc_files)

wobbly_LDFLAGS = $(QT5WIDGETS_LIBS) $(VSScript_LIBS)

wobbly_CPPFLAGS = $(QT5WIDGETS_CFLAGS) $(VSScript_CFLAGS)


