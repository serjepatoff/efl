MAINTAINERCLEANFILES = Makefile.in
CLEANFILES =
BUILT_SOURCES =

EOLIAN_FLAGS = -I$(srcdir)

DIST_SUBDIRS =
SUBDIRS =

AM_TESTS_ENVIRONMENT =

lib_LTLIBRARIES =
bin_PROGRAMS =
bin_SCRIPTS =
noinst_PROGRAMS =
check_PROGRAMS =
TESTS =
EXTRA_DIST2 =

noinst_LTLIBRARIES =
noinst_DATA =
noinst_HEADERS =

GENERATED_JS_BINDINGS =
GENERATED_LUA_BINDINGS =

EFL_INSTALL_EXEC_HOOK=

include Makefile_Evil.am
include Makefile_Escape.am

include Makefile_Eina.am
include Makefile_Eo.am
include Makefile_Efl.am
include Makefile_Emile.am
include Makefile_Eet.am
include Makefile_Eolian.am
include Makefile_Ector.am
include Makefile_Ecore.am
include Makefile_Evas.am
include Makefile_Eldbus.am
include Makefile_Ecore_Con.am
include Makefile_Ecore_Ipc.am
include Makefile_Ecore_File.am
include Makefile_Eeze.am
include Makefile_Ecore_Input.am
include Makefile_Ecore_Input_Evas.am
include Makefile_Ecore_Cocoa.am
include Makefile_Ecore_FB.am
include Makefile_Ecore_Psl1ght.am
include Makefile_Ecore_SDL.am
include Makefile_Ecore_Wayland.am
include Makefile_Ecore_Wl2.am
include Makefile_Ecore_Win32.am
include Makefile_Ecore_X.am
include Makefile_Ecore_IMF.am
include Makefile_Ecore_IMF_Evas.am
include Makefile_Elput.am
include Makefile_Ecore_Drm.am
include Makefile_Ecore_Drm2.am
include Makefile_Ecore_Evas.am
include Makefile_Ecore_Audio.am
include Makefile_Ecore_Avahi.am
include Makefile_Embryo.am
include Makefile_Efreet.am
include Makefile_Eio.am
include Makefile_Ecore_Buffer.am
include Makefile_EPhysics.am
include Makefile_Edje.am
include Makefile_Emotion.am
include Makefile_Ethumb.am
include Makefile_Ethumb_Client.am
include Makefile_Elocation.am
include Makefile_Elementary.am

include Makefile_Cxx.am
include Makefile_Eolian_Cxx.am

include Makefile_Elua.am

if HAVE_JS
AM_V_CP = $(am__v_CP_@AM_V@)
am__v_CP_ = $(am__v_CP_@AM_DEFAULT_V@)
am__v_CP_0 = @echo "  CP  " $@;
CP = cp
if EFL_ENABLE_TESTS
if HAVE_NODEJS
AM_TESTS_ENVIRONMENT += NODE_PATH='$(abs_builddir)/bin/efl_js:$(abs_builddir)/tests/eolian_js:$(abs_builddir)/tests/efl_js'; export NODE_PATH;
endif
endif
endif

include Makefile_Eolian_Js.am
include Makefile_Efl_Js.am

# Make sure all generated files getting cleaned up
CLEANFILES += $(BUILT_SOURCES)

.PHONY: benchmark examples

BENCHMARK_SUBDIRS = \
benchmarks/eina \
benchmarks/eo \
benchmarks/evas
DIST_SUBDIRS += $(BENCHMARK_SUBDIRS)

benchmark: all
	@for d in $(BENCHMARK_SUBDIRS); do \
	   echo "Making benchmark in $$d"; \
	   $(MAKE) $(AM_MAKEFLAGS) -C $$d benchmark; \
	done

EXAMPLES_SUBDIRS = \
examples/eina \
examples/eet \
examples/evas \
examples/ecore \
examples/ecore_avahi \
examples/eio \
examples/eldbus \
examples/ephysics \
examples/edje \
examples/emotion \
examples/emile \
examples/elocation \
examples/ethumb_client \
examples/elementary

if HAVE_ELUA
EXAMPLES_SUBDIRS += examples/elua
endif

EXAMPLES_CXX = \
examples/eina_cxx \
examples/eolian_cxx

if HAVE_CXX11
EXAMPLES_SUBDIRS += $(EXAMPLES_CXX)
endif

if ALWAYS_BUILD_EXAMPLES
SUBDIRS += . $(EXAMPLES_SUBDIRS)
endif

DIST_SUBDIRS += $(EXAMPLES_SUBDIRS)

if EFL_ENABLE_TESTS
check-build: all
	@$(MAKE) $(AM_MAKEFLAGS) $(check_PROGRAMS)
endif

examples: all
	@for d in $(EXAMPLES_SUBDIRS); do \
	   echo "Making examples in $$d"; \
	   $(MAKE) $(AM_MAKEFLAGS) -C $$d examples || exit 1; \
	done

install-examples:
	@for d in $(EXAMPLES_SUBDIRS); do \
	   echo "Making install-examples in $$d"; \
	   $(MAKE) $(AM_MAKEFLAGS) -C $$d install-examples || exit 1; \
	done

dist-hook:
	@mkdir -p $(distdir)/tests/elua/data/apps $(distdir)/tests/eolian/data
	@mkdir -p $(distdir)/tests/evas/images $(distdir)/tests/evas/meshes
	@mkdir -p $(distdir)/tests/efreet/data $(distdir)/tests/edje/data
	@mkdir -p $(distdir)/tests/emotion/data $(distdir)/tests/evas/meshes/md2
	@mkdir -p $(distdir)/tests/evas/meshes/obj $(distdir)/tests/efreet/data/sub
	@mkdir -p $(distdir)/scripts/eina $(distdir)/scripts/elua/modules
	@mkdir -p $(distdir)/scripts/elua/core $(distdir)/scripts/elua/apps/docgen
	@mkdir -p $(distdir)/lib/evas/common/evas_op_add $(distdir)/lib/evas/common/evas_op_copy
	@mkdir -p $(distdir)/lib/evas/common/evas_op_mask $(distdir)/lib/evas/common/evas_op_mul
	@mkdir -p $(distdir)/lib/evas/common/evas_op_sub $(distdir)/lib/evas/filters/blur
	@mkdir -p $(distdir)/static_libs/http-parser/contrib $(distdir)/bindings/luajit/eina
	for F in $(EXTRA_DIST2); do cp -f $(srcdir)/$$F $(distdir)/$$F; done

clean-local:
	find . -type f -name '*.gcno' -delete
	find . -type f -name '*.gcda' -delete
	find . -type f -name check-results.xml -delete
	rm -f $(GENERATED_JS_BINDINGS)
	rm -f $(GENERATED_LUA_BINDINGS)

install-exec-hook:
	$(MKDIR_P) $(DESTDIR)$(libdir)
	for i in $(lib_LTLIBRARIES); do \
	   FNAME="$(DESTDIR)$(libdir)/`echo $$i | sed "s#lib/.*/##"`"; \
	   FNAMEEXT="$$FNAME.$$$$"; \
	   sed "s/-luuid//" $$FNAME > $$FNAMEEXT; \
	   mv $$FNAMEEXT $$FNAME; \
	done
	$(EFL_INSTALL_EXEC_HOOK)
