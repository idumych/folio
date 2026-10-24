# Folio — builds on Haiku and on VitruvianOS (V\OS) from the same sources.
#
#   make          → ./Folio
#   make test     → link-engine unit tests (any OS, no Haiku API needed)
#   make install  → copies Folio to your apps folder

NAME     := Folio
CXX      ?= g++
CXXFLAGS ?= -O2 -g
override CXXFLAGS += -std=c++17 -Wall -Wno-multichar
UNAME    := $(shell uname -s)
VOS_HEADERS ?= /system/develop/headers
COSMOE_ROOT ?= /usr
VOS_ARCH := $(if $(filter aarch64,$(shell uname -m)),arm64,$(shell uname -m))

ifeq ($(UNAME),Haiku)
  PLATFORM     := Haiku
  LIBS         := -lbe -ltracker -ltranslation
  INSTALL_DIR  := $(shell finddir B_USER_NONPACKAGED_DIRECTORY)/apps
  # stamps the app signature and file types onto the binary
  RESOURCES     = rc -o obj/Folio.rsrc Folio.rdef && xres -o $@ obj/Folio.rsrc
else
  # VitruvianOS: the Haiku API on a Linux kernel. Headers and flags come
  # from the vos-dev package through pkg-config.
  ifeq ($(shell pkg-config --exists vos && echo yes),yes)
    PLATFORM    := VitruvianOS
    override CXXFLAGS += $(shell pkg-config --cflags vos) -fPIC
    LIBS        := $(shell pkg-config --libs vos) -ltracker -ltranslation
    INSTALL_DIR := $(or $(shell finddir B_USER_NONPACKAGED_DIRECTORY 2>/dev/null),$(HOME)/config/non-packaged)/apps
    # V\OS keeps the app signature both as resources and as attributes
    # (absolute paths, as V\OS's own build passes them to xres/resattr)
    RESOURCES    = rc -o $(CURDIR)/obj/Folio.rsrc $(CURDIR)/Folio.rdef \
                   && xres -o $(CURDIR)/$@ $(CURDIR)/obj/Folio.rsrc \
                   && resattr -O -o $(CURDIR)/$@ $(CURDIR)/obj/Folio.rsrc
  else ifneq ($(wildcard $(VOS_HEADERS)/os/BeBuild.h),)
    # VitruvianOS 0.6.x has no vos-dev/pkg-config yet: the headers ship in
    # the system package under /system/develop/headers.
    PLATFORM    := VitruvianOS
    VOS_DIRS    := . os os/app os/drivers os/kernel os/support os/storage \
                   os/translation os/config os/add-ons/file_system \
                   os/add-ons/graphics os/add-ons/registrar \
                   os/add-ons/network_settings os/arch/$(VOS_ARCH) os/device \
                   os/drivers/bus os/game os/interface os/locale \
                   os/locale/tools os/mail os/net os/posix/compat/sys \
                   os/add-ons/input_server os/add-ons/tracker os/opengl \
                   os/media2 posix build build/config_headers config
    override CXXFLAGS += $(addprefix -I$(VOS_HEADERS)/,$(VOS_DIRS)) \
                   -include LinuxBuildCompatibility.h -fPIC
    LIBS        := -lbe -lroot -ltracker -ltranslation
    INSTALL_DIR := $(or $(shell finddir B_USER_NONPACKAGED_DIRECTORY 2>/dev/null),$(HOME)/config/non-packaged)/apps
    RESOURCES    = rc -o $(CURDIR)/obj/Folio.rsrc $(CURDIR)/Folio.rdef \
                   && xres -o $(CURDIR)/$@ $(CURDIR)/obj/Folio.rsrc \
                   && resattr -O -o $(CURDIR)/$@ $(CURDIR)/obj/Folio.rsrc
  else ifneq ($(wildcard $(COSMOE_ROOT)/include/os/app/Application.h),)
    # Plain Linux with Cosmoe (the Haiku API as a library on X11/Wayland).
    # COSMOE_ROOT is its install prefix; build-appimage.sh points it at a
    # staged copy that ships inside the AppImage.
    PLATFORM    := Cosmoe
    COSMOE_DIRS := config os os/app os/game os/interface os/kernel os/locale \
                   os/media os/net os/storage os/support os/translation \
                   os/be_apps/Tracker private/interface private/shared \
                   private/support posix
    override CXXFLAGS += -I$(COSMOE_ROOT)/include \
                   $(addprefix -I$(COSMOE_ROOT)/include/,$(COSMOE_DIRS)) \
                   -DFOLIO_COSMOE -fPIC
    # libbe finds Cosmoe's add-ons and data next to itself, so the AppImage
    # (bin/Folio + lib/) works wherever it is mounted.
    COSMOE_RPATH ?= $$ORIGIN/../lib
    LIBS        := -L$(COSMOE_ROOT)/lib -lbe -ltracker \
                   -Wl,-rpath,'$(COSMOE_RPATH)'
    INSTALL_DIR := $(HOME)/.local/bin
    # Cosmoe reads resources from a .cosmoe_rsrc section linked into the app.
    EXTRA_OBJS  := obj/Folio.rsrc.o
    RESOURCES    = true
  else
    PLATFORM    := none
  endif
endif

SRCS := src/App.cpp src/MainWindow.cpp src/NoteView.cpp src/Widgets.cpp \
        src/Vault.cpp src/LinkGraph.cpp src/MarkdownScan.cpp
OBJS := $(patsubst src/%.cpp,obj/%.o,$(SRCS))

all: check-platform $(NAME)

check-platform:
ifeq ($(PLATFORM),none)
	@echo "Folio needs Haiku, VitruvianOS, or Linux with Cosmoe. On"
	@echo "VitruvianOS it uses 'pkg-config vos' (vos-dev) or the headers in"
	@echo "$(VOS_HEADERS); on Linux, Cosmoe installed under COSMOE_ROOT=$(COSMOE_ROOT)."
	@echo "(The link-engine tests run anywhere: make test)"
	@false
endif

obj/%.o: src/%.cpp src/*.h
	@mkdir -p obj
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/Folio.rsrc.o: Folio.rdef
	@mkdir -p obj
	LD_LIBRARY_PATH=$(COSMOE_ROOT)/lib $(COSMOE_ROOT)/bin/rc \
		-o obj/Folio.rsrc Folio.rdef
	cd obj && ld -r -b binary Folio.rsrc -o Folio.rsrc.o
	objcopy --rename-section .data=.cosmoe_rsrc \
		--add-section .note.GNU-stack=/dev/null \
		--set-section-flags .note.GNU-stack=noload,readonly obj/Folio.rsrc.o

$(NAME): $(OBJS) $(EXTRA_OBJS) Folio.rdef
	$(CXX) $(LDFLAGS) $(OBJS) $(EXTRA_OBJS) -o $@ $(LIBS)
	$(RESOURCES)
	@if command -v mimeset >/dev/null 2>&1; then mimeset -f $@; fi

# Debian package for VitruvianOS (see packaging/build-deb.sh)
deb: all
	sh packaging/build-deb.sh

# Haiku package, built on Haiku R1/beta6+ (see packaging/build-hpkg.sh)
hpkg:
	sh packaging/build-hpkg.sh

test: tests/test_core.cpp src/LinkGraph.cpp src/MarkdownScan.cpp src/*.h
	@mkdir -p obj
	$(CXX) -std=c++17 -Wall -O1 -g tests/test_core.cpp src/LinkGraph.cpp \
		src/MarkdownScan.cpp -o obj/test_core
	./obj/test_core

install: all
	mkdir -p "$(INSTALL_DIR)"
	cp $(NAME) "$(INSTALL_DIR)/"
	@if command -v mimeset >/dev/null 2>&1; then mimeset -f "$(INSTALL_DIR)/$(NAME)"; fi

clean:
	rm -rf obj $(NAME)

.PHONY: all check-platform test install clean deb hpkg
