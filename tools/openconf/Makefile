#
# Copyright (C) 2010-2020 Anup Patel <anup@brainfault.org>
#

#---------------------------------------------------------
# Nothing beyond this point should ever be touched by mere
# mortals.  Unless you hang out with the gods, you should
# probably leave all this stuff alone.

# ===========================================================================
# These targets are used from top-level makefile

PHONY += oldconfig xconfig gconfig menuconfig config silentoldconfig
PHONY += randconfig allyesconfig allnoconfig allmodconfig defconfig

all: mconf

xconfig: qconf
#	$< $(OPENCONF_CONFIG)

gconfig: gconf
#	$< $(OPENCONF_CONFIG)

menuconfig: mconf
#	$< $(OPENCONF_CONFIG)

config: conf
#	$< $(OPENCONF_CONFIG)

oldconfig: conf
#	$< -o $(OPENCONF_CONFIG)

silentoldconfig: conf
#	$< -s $(OPENCONF_CONFIG)

randconfig: conf
#	$< -r $(OPENCONF_CONFIG)

allyesconfig: conf
#	$< -y $(OPENCONF_CONFIG)

allnoconfig: conf
#	$< -n $(OPENCONF_CONFIG)

allmodconfig: conf
#	$< -m $(OPENCONF_CONFIG)

savedefconfig: conf
#	$< -S defconfig $(OPENCONF_CONFIG)

defconfig: conf
#	$< -d $(OPENCONF_CONFIG)

%_defconfig: conf
#	$(Q)$< -D arch/$(SRCARCH)/configs/$@ $(OPENCONF_CONFIG)

# Help text used by make help
help:
	@echo  '  config	  - Update current config utilising a line-oriented program'
	@echo  '  menuconfig	  - Update current config utilising a menu based program'
	@echo  '  xconfig	  - Update current config utilising a QT based front-end'
	@echo  '  gconfig	  - Update current config utilising a GTK based front-end'
	@echo  '  oldconfig	  - Update current config utilising a provided .config as base'
	@echo  '  silentoldconfig - Same as oldconfig, but quietly'
	@echo  '  randconfig	  - New config with random answer to all options'
	@echo  '  defconfig	  - New config with default answer to all options'
	@echo  '  allmodconfig	  - New config selecting modules when possible'
	@echo  '  allyesconfig	  - New config where all options are accepted with yes'
	@echo  '  allnoconfig	  - New config where all options are answered with no'

# check for proper make version
ifneq ($(findstring x3.7,x$(MAKE_VERSION)),)
$(error Your make is too old $(MAKE_VERSION). Go get at least 3.80)
endif

# lxdialog stuff
check-lxdialog  := ./lxdialog/check-lxdialog.sh

# Select the compiler needed to build binaries for your development system
HOST_CC     = gcc
HOST_CFLAGS = -O2 -Wall

# Use recursively expanded variables so we do not call gcc unless
# we really need to do so. (Do not call gcc as part of make mrproper)
HOST_EXTRACFLAGS = $(shell $(CONFIG_SHELL) $(check-lxdialog) -ccflags)
HOST_LOADLIBES   = $(shell $(CONFIG_SHELL) $(check-lxdialog) -ldflags $(HOST_CC))

HOST_EXTRACFLAGS += -DLOCALE -I. -I.. -g

lxdialog := lxdialog/checklist.o lxdialog/util.o lxdialog/inputbox.o
lxdialog += lxdialog/textbox.o lxdialog/yesno.o lxdialog/menubox.o

conf-objs	:= conf.o  zconf.tab.o
mconf-objs	:= mconf.o zconf.tab.o $(lxdialog)
gettext-objs	:= gettext.o zconf.tab.o

ifeq ($(MAKECMDGOALS),xconfig)
	qconf-target := 1
endif

ifeq ($(MAKECMDGOALS),gconfig)
	gconf-target := 1
endif

ifeq ($(qconf-target),1)
qconf-cxxobjs	:= qconf.o
qconf-objs	:= openconf_load.o zconf.tab.o
endif

ifeq ($(gconf-target),1)
gconf-objs	:= gconf.o openconf_load.o zconf.tab.o
endif

clean-files	:= openconf_defs.h qconf.moc .tmp_qtcheck \
		   .tmp_gtkcheck zconf.tab.c lex.zconf.c zconf.hash.c gconf.glade.h
clean-files     += mconf qconf gconf
clean-files     += config.pot linux.pot

# Check that we have the required ncurses stuff installed for lxdialog (menuconfig)
PHONY += $(obj)/dochecklxdialog
$(addprefix $(obj)/,$(lxdialog)): $(obj)/dochecklxdialog
$(obj)/dochecklxdialog:
	$(Q)$(CONFIG_SHELL) $(check-lxdialog) -check $(HOSTCC) $(HOST_EXTRACFLAGS) $(HOST_LOADLIBES)

always := dochecklxdialog

# Add environment specific flags
HOST_EXTRACFLAGS += $(shell $(CONFIG_SHELL) ./check.sh $(HOSTCC) $(HOSTCFLAGS))

# generated files seem to need this to find local include files
HOSTCFLAGS_lex.zconf.o	:= -I.
HOSTCFLAGS_zconf.tab.o	:= -I.

HOSTLOADLIBES_qconf	= $(KC_QT_LIBS) -ldl
HOSTCXXFLAGS_qconf.o	= $(KC_QT_CFLAGS) -D OPENCONF_DIRECT_LINK

HOSTLOADLIBES_gconf	= `pkg-config --libs gtk+-2.0 gmodule-2.0 libglade-2.0`
HOSTCFLAGS_gconf.o	= `pkg-config --cflags gtk+-2.0 gmodule-2.0 libglade-2.0` \
                          -D OPENCONF_DIRECT_LINK

qconf.o: .tmp_qtcheck

ifeq ($(qconf-target),1)
.tmp_qtcheck: $(src)/Makefile
-include .tmp_qtcheck

# QT needs some extra effort...
.tmp_qtcheck:
	@set -e; echo "  CHECK   qt"; dir=""; pkg=""; \
	pkg-config --exists qt 2> /dev/null && pkg=qt; \
	pkg-config --exists qt-mt 2> /dev/null && pkg=qt-mt; \
	if [ -n "$$pkg" ]; then \
	  cflags="\$$(shell pkg-config $$pkg --cflags)"; \
	  libs="\$$(shell pkg-config $$pkg --libs)"; \
	  moc="\$$(shell pkg-config $$pkg --variable=prefix)/bin/moc"; \
	  dir="$$(pkg-config $$pkg --variable=prefix)"; \
	else \
	  for d in $$QTDIR /usr/share/qt* /usr/lib/qt*; do \
	    if [ -f $$d/include/qconfig.h ]; then dir=$$d; break; fi; \
	  done; \
	  if [ -z "$$dir" ]; then \
	    echo "*"; \
	    echo "* Unable to find the QT3 installation. Please make sure that"; \
	    echo "* the QT3 development package is correctly installed and"; \
	    echo "* either install pkg-config or set the QTDIR environment"; \
	    echo "* variable to the correct location."; \
	    echo "*"; \
	    false; \
	  fi; \
	  libpath=$$dir/lib; lib=qt; osdir=""; \
	  $(HOSTCXX) -print-multi-os-directory > /dev/null 2>&1 && \
	    osdir=x$$($(HOSTCXX) -print-multi-os-directory); \
	  test -d $$libpath/$$osdir && libpath=$$libpath/$$osdir; \
	  test -f $$libpath/libqt-mt.so && lib=qt-mt; \
	  cflags="-I$$dir/include"; \
	  libs="-L$$libpath -Wl,-rpath,$$libpath -l$$lib"; \
	  moc="$$dir/bin/moc"; \
	fi; \
	if [ ! -x $$dir/bin/moc -a -x /usr/bin/moc ]; then \
	  echo "*"; \
	  echo "* Unable to find $$dir/bin/moc, using /usr/bin/moc instead."; \
	  echo "*"; \
	  moc="/usr/bin/moc"; \
	fi; \
	echo "KC_QT_CFLAGS=$$cflags" > $@; \
	echo "KC_QT_LIBS=$$libs" >> $@; \
	echo "KC_QT_MOC=$$moc" >> $@
endif

gconf.o: .tmp_gtkcheck

ifeq ($(gconf-target),1)
-include .tmp_gtkcheck

# GTK needs some extra effort, too...
.tmp_gtkcheck:
	@if `pkg-config --exists gtk+-2.0 gmodule-2.0 libglade-2.0`; then		\
		if `pkg-config --atleast-version=2.0.0 gtk+-2.0`; then			\
			touch $@;								\
		else									\
			echo "*"; 							\
			echo "* GTK+ is present but version >= 2.0.0 is required.";	\
			echo "*";							\
			false;								\
		fi									\
	else										\
		echo "*"; 								\
		echo "* Unable to find the GTK+ installation. Please make sure that"; 	\
		echo "* the GTK+ 2.0 development package is correctly installed..."; 	\
		echo "* You need gtk+-2.0, glib-2.0 and libglade-2.0."; 		\
		echo "*"; 								\
		false;									\
	fi
endif

zconf.tab.o: lex.zconf.c zconf.hash.c

openconf_load.o: openconf_defs.h

qconf.o: qconf.moc openconf_defs.h

gconf.o: openconf_defs.h

%.moc: %.h
	$(KC_QT_MOC) -i $< -o $@

openconf_defs.h: openconf_proto.h
	sed < $< > $@ 's/P(\([^,]*\),.*/#define \1 (\*\1_p)/'

# Extract gconf menu items for I18N support
gconf.glade.h: gconf.glade
	intltool-extract --type=gettext/glade $(obj)/gconf.glade

###
# The following requires flex/bison/gperf
# By default we use the _shipped versions, uncomment the following line if
# you are modifying the flex/bison src.
# OPENCONF_GENPARSER := 1

ifdef OPENCONF_GENPARSER

zconf.tab.c: zconf.y
lex.zconf.c: zconf.l
zconf.hash.c: zconf.gperf

%.tab.c: %.y
	bison -l -b $* -p $(notdir $*) $<
	cp $@ $@_shipped

lex.%.c: %.l
	flex -L -P$(notdir $*) -o$@ $<
	cp $@ $@_shipped

%.hash.c: %.gperf
	gperf < $< > $@
	cp $@ $@_shipped

endif

ifeq ($(findstring mconf,$(MAKECMDGOALS)),mconf)
hostprogs-y += mconf
endif

-include .depend
.depend: $(wildcard *.h *.c)
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) -MM *.c > .depend 2>/dev/null || :

conf: $(conf-objs)
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) $(HOSTCFLAGS_$@) $(conf-objs) $(HOST_LOADLIBES) -o $@

mconf: $(mconf-objs)
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) $(HOSTCFLAGS_$@) $(mconf-objs) $(HOST_LOADLIBES) -o $@

qconf: $(qconf-objs)
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) $(HOSTCFLAGS_$@) $(qonf-objs) $(HOST_LOADLIBES) -o $@

gconf: $(gconf-objs)
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) $(HOSTCFLAGS_$@) $(gconf-objs) $(HOST_LOADLIBES) -o $@

gettext: $(gettext-objs)
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) $(HOSTCFLAGS_$@) $(gettext-objs) $(HOST_LOADLIBES) -o $@

%.o: %.c
	$(Q)$(HOST_CC) $(HOST_EXTRACFLAGS) $(HOST_CFLAGS) $(HOSTCFLAGS_$@) -c $< -o $@

%.c : %.c_shipped
	$(Q)cat $< > $@

clean:
	$(Q)rm -f $(clean-files) conf
	$(Q)rm -f $(lxdialog) $(conf-objs) $(mconf-objs) $(kxgettext-objs) $(gettext-objs) \
		$(hostprogs-y) $(qconf-cxxobjs) $(qconf-objs) $(gconf-objs) \
		.depend

FORCE:
.PHONY: FORCE clean distclean
