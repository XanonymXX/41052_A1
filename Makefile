CXX ?= c++
CPPFLAGS := -Iinclude
CXXFLAGS := -std=c++23 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow

BUILD_DIR := build
LIB_SOURCES := \
	src/red_black_tree_event_set.cpp \
	src/binary_search_tree_event_set.cpp \
	src/sorted_vector_event_set.cpp

.PHONY: all setup demo test sanitize benchmark-bin benchmark benchmark-quick plots plots-quick test-analysis deliverables clean

PYTHON ?= .venv/bin/python
VENV_STAMP := .venv/.requirements-installed

all: demo

setup: $(VENV_STAMP)

$(VENV_STAMP): requirements.txt
	python3 -m venv .venv
	.venv/bin/python -m pip install -r requirements.txt
	touch $(VENV_STAMP)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

demo: $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LIB_SOURCES) src/main.cpp -o $(BUILD_DIR)/scheduler_demo

test: $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LIB_SOURCES) tests/test_event_sets.cpp -o $(BUILD_DIR)/test_event_sets
	./$(BUILD_DIR)/test_event_sets

sanitize: $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++23 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		-fsanitize=undefined -fno-omit-frame-pointer -DRANDOM_OPERATION_COUNT=2500 \
		$(LIB_SOURCES) tests/test_event_sets.cpp -o $(BUILD_DIR)/test_event_sets_sanitize
	./$(BUILD_DIR)/test_event_sets_sanitize

benchmark-bin: $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++23 -O3 -DNDEBUG -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
		$(LIB_SOURCES) benchmark/benchmark.cpp -o $(BUILD_DIR)/benchmark

benchmark: benchmark-bin
	./$(BUILD_DIR)/benchmark --output results/raw.csv

benchmark-quick: benchmark-bin
	./$(BUILD_DIR)/benchmark --quick --output build/quick-results.csv

plots: setup
	$(PYTHON) scripts/analyze_results.py --input results/raw.csv --expected-repetitions 7 \
		--expected-sizes 250,500,1000,2000,4000,8000

plots-quick: setup benchmark-quick
	$(PYTHON) scripts/analyze_results.py --input build/quick-results.csv \
		--summary build/quick-summary.csv --figures-dir build/quick-figures \
		--metadata build/quick-metadata.json --expected-repetitions 2 --expected-sizes 100,250

test-analysis: plots-quick
	$(PYTHON) tests/test_analysis.py
	test -s build/quick-summary.csv
	test -s build/quick-figures/build_time.png
	test -s build/quick-figures/lookup_time.png
	test -s build/quick-figures/mixed_workloads.png
	test -s build/quick-figures/tree_height.png
	test -s build/quick-figures/memory_per_event.png

deliverables: test sanitize test-analysis plots

clean:
	rm -rf $(BUILD_DIR)
