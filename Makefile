SHELL      := bash
LUMEN      := tools/lumen/target/release/lumen
CMAKE_DIR  := build/cmake
BENCH_DIR  := build/bench
JOBS       ?= $(shell nproc)
MODE       ?= auto
ITER       ?= 300
STYLES     ?= Lumen Breeze Fusion

.DEFAULT_GOAL := build
.PHONY: help tool gen configure native build bench bench-gen bench-style check-settings check-tray kcm \
        install install-system uninstall uninstall-system apply apply-layout doctor snapshot clean \
        upstream-check upstream-sync

help:
	@echo "build            tool + gen + native plugins (style, decoration, effects, tray, KCM)"
	@echo "bench            generator and widget-style benchmarks (ITER=$(ITER), STYLES='$(STYLES)')"
	@echo "kcm              open the System Settings module from the build tree"
	@echo "check-settings   verify settings with a temporary config and isolated desktop services"
	@echo "check-tray       verify Quick Settings icon dragging, clicks and ordering"
	@echo "install          user data (color schemes, Plasma theme, look-and-feel, sounds, ...)"
	@echo "install-system   user data + native plugins + lock/logout + pacman hooks + boot + login (sudo)"
	@echo "uninstall        remove user data;  uninstall-system: everything"
	@echo "apply            activate Lumen (MODE=dark|light|auto);  apply-layout: also the taskbar layout"
	@echo "doctor snapshot  health check;  preview what apply-layout keeps from the current panel"
	@echo "upstream-check   newest upstream releases of the forked KDE sources (upstream/forks.toml)"
	@echo "upstream-sync    three-way merge the forks with them (FORK=name TO=version optional)"
	@echo "clean            remove build/ and the Rust target"

tool:
	cargo build --release --quiet --manifest-path tools/lumen/Cargo.toml

gen: tool
	$(LUMEN) gen

$(CMAKE_DIR)/build.ninja: | gen
	cmake -S . -B $(CMAKE_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
		-DBUILD_TESTING=OFF -DLUMEN_BENCH=ON -DLUMEN_GEN_DIR=$(CURDIR)/build/gen/cpp

configure: $(CMAKE_DIR)/build.ninja

native: gen configure
	cmake --build $(CMAKE_DIR) --parallel $(JOBS)

build: native

bench: bench-gen bench-style

bench-gen: tool
	@$(LUMEN) gen >/dev/null
	@for i in 1 2 3; do $(LUMEN) gen | tail -1; done

bench-style: native
	@mkdir -p $(BENCH_DIR)/styles
	@ln -sf $(CURDIR)/$(CMAKE_DIR)/bin/lumen6.so $(BENCH_DIR)/styles/lumen6.so
	@for s in $(STYLES); do \
		QT_PLUGIN_PATH=$(CURDIR)/$(BENCH_DIR) QT_QPA_PLATFORM=offscreen $(CMAKE_DIR)/bin/stylebench $$s $(ITER) | tail -1; \
	done

kcm: native
	QT_PLUGIN_PATH=$(CURDIR)/$(CMAKE_DIR)/bin kcmshell6 kcm_lumen

check-settings: native
	dbus-run-session -- $(CMAKE_DIR)/bin/settingscheck $(CURDIR)/$(CMAKE_DIR)/bin/plasma/kcms/systemsettings/kcm_lumen.so

check-tray: configure
	cmake --build $(CMAKE_DIR) --target traycheck --parallel $(JOBS)
	QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FORCE_STDERR_LOGGING=1 $(CMAKE_DIR)/bin/traycheck

install: gen
	$(LUMEN) install --no-build

install-system: native
	$(LUMEN) install --system --no-build

uninstall: tool
	$(LUMEN) uninstall

uninstall-system: tool
	$(LUMEN) uninstall --system

apply: tool
	$(LUMEN) apply $(MODE)

apply-layout: tool
	$(LUMEN) apply $(MODE) --layout

doctor: tool
	$(LUMEN) doctor

snapshot: tool
	$(LUMEN) snapshot

upstream-check: tool
	$(LUMEN) upstream check

upstream-sync: tool
	$(LUMEN) upstream sync $(FORK) $(if $(TO),--to $(TO))

clean:
	rm -rf build
	cargo clean --quiet --manifest-path tools/lumen/Cargo.toml
