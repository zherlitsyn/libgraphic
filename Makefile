CMAKE		?= cmake
GENERATOR	?= Unix Makefiles
JOBS		?= $(shell nproc 2>/dev/null || echo 4)

BUILD_DIR	:= build
RELEASE_DIR	:= build-release

.PHONY: all release run clean configure configure-release

all: configure
	$(CMAKE) --build $(BUILD_DIR) -j $(JOBS)

release: configure-release
	$(CMAKE) --build $(RELEASE_DIR) -j $(JOBS)

run: all
	./$(BUILD_DIR)/bin/window

configure: $(BUILD_DIR)/Makefile

configure-release: $(RELEASE_DIR)/Makefile

$(BUILD_DIR)/Makefile:
	$(CMAKE) -S . -B $(BUILD_DIR) -G "$(GENERATOR)" \
		-DCMAKE_BUILD_TYPE=Debug

$(RELEASE_DIR)/Makefile:
	$(CMAKE) -S . -B $(RELEASE_DIR) -G "$(GENERATOR)" \
		-DCMAKE_BUILD_TYPE=Release

clean:
	rm -rf $(BUILD_DIR) $(RELEASE_DIR)