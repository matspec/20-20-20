# 20-20-20 for Linux. Mirrors build.cmd / mac/build.sh: one command, no project generator.
#
#   make                 build  -> build/twentytwentytwenty
#   make run             build and launch
#   make install         install for the current user (PREFIX=$HOME/.local by default)
#   make uninstall
#   make appimage        build a portable AppImage (needs appimagetool on PATH, or set APPIMAGETOOL)
#
# Build deps (Debian/Ubuntu):  sudo apt install build-essential pkg-config libgtk-3-dev libayatana-appindicator3-dev libxss-dev
#            (Fedora):         sudo dnf install gcc make pkgconf gtk3-devel libayatana-appindicator-gtk3-devel libXScrnSaver-devel
#            (Arch):           sudo pacman -S base-devel gtk3 libayatana-appindicator libxss
# appindicator and libXss are optional: without them you get a legacy GtkStatusIcon tray and no idle detection on X11.

CC      ?= cc
PREFIX  ?= $(HOME)/.local
EXE     := twentytwentytwenty
APP_ID  := com.sadiq.twentytwentytwenty
BUILD   := build
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:src/%.c=$(BUILD)/%.o)

PKGS    := gtk+-3.0 gio-2.0
CFLAGS  += -std=gnu11 -O2 -Wall -Wextra -Wno-deprecated-declarations -Wno-unused-parameter
LDLIBS  += -lm

# Optional: AppIndicator (Ayatana first, then the Ubuntu fork). Falls back to GtkStatusIcon.
ifeq ($(shell pkg-config --exists ayatana-appindicator3-0.1 && echo y),y)
  PKGS   += ayatana-appindicator3-0.1
  CFLAGS += -DHAVE_APPINDICATOR
else ifeq ($(shell pkg-config --exists appindicator3-0.1 && echo y),y)
  PKGS   += appindicator3-0.1
  CFLAGS += -DHAVE_APPINDICATOR -DHAVE_LEGACY_APPINDICATOR
endif

# Optional: XScreenSaver extension for idle detection on X11.
ifeq ($(shell pkg-config --exists xscrnsaver x11 && echo y),y)
  PKGS   += xscrnsaver x11
  CFLAGS += -DHAVE_XSS
endif

CFLAGS  += $(shell pkg-config --cflags $(PKGS))
LDLIBS  += $(shell pkg-config --libs $(PKGS))

.PHONY: all run install uninstall appimage clean

all: $(BUILD)/$(EXE)

$(BUILD)/$(EXE): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	@echo "Built $@"

$(BUILD)/%.o: src/%.c $(wildcard src/*.h) | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

run: all
	-pkill -x $(EXE)
	$(BUILD)/$(EXE) &

$(BUILD)/icons/512.png: $(BUILD)/$(EXE)
	$(BUILD)/$(EXE) --make-icons $(BUILD)/icons

install: all $(BUILD)/icons/512.png
	install -Dm755 $(BUILD)/$(EXE) $(DESTDIR)$(PREFIX)/bin/$(EXE)
	install -Dm644 $(APP_ID).desktop $(DESTDIR)$(PREFIX)/share/applications/$(APP_ID).desktop
	for s in 16 22 24 32 48 64 128 256 512; do \
	  install -Dm644 $(BUILD)/icons/$$s.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${s}x$${s}/apps/$(APP_ID).png; \
	done
	-gtk-update-icon-cache -q $(DESTDIR)$(PREFIX)/share/icons/hicolor 2>/dev/null
	-update-desktop-database -q $(DESTDIR)$(PREFIX)/share/applications 2>/dev/null
	@echo "Installed. Launch '$(APP_ID)' from your app menu or run $(PREFIX)/bin/$(EXE)"

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE) $(DESTDIR)$(PREFIX)/share/applications/$(APP_ID).desktop
	for s in 16 22 24 32 48 64 128 256 512; do rm -f $(DESTDIR)$(PREFIX)/share/icons/hicolor/$${s}x$${s}/apps/$(APP_ID).png; done

# Portable AppImage: bundles the binary only (GTK comes from the host, as every desktop has it).
APPIMAGETOOL ?= appimagetool
appimage: all $(BUILD)/icons/512.png
	rm -rf $(BUILD)/AppDir
	$(MAKE) install DESTDIR=$(BUILD)/AppDir PREFIX=/usr
	cp $(APP_ID).desktop $(BUILD)/AppDir/
	cp $(BUILD)/icons/256.png $(BUILD)/AppDir/$(APP_ID).png
	ln -sf usr/bin/$(EXE) $(BUILD)/AppDir/AppRun
	ln -sf $(APP_ID).png $(BUILD)/AppDir/.DirIcon
	mkdir -p dist
	ARCH=$$(uname -m) $(APPIMAGETOOL) $(BUILD)/AppDir dist/TwentyTwentyTwenty-$$(uname -m).AppImage

clean:
	rm -rf $(BUILD) dist
