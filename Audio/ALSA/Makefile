# MMAP Audio Example Makefile

CC = gcc
CFLAGS = -Wall -g -O2 -std=c99
LDFLAGS = -lasound -lm -lpthread

# 源文件
SOURCES = mmap_audio_example.c
TARGET = mmap_audio_test

# 检测ALSA库
ALSA_CFLAGS = $(shell pkg-config --cflags alsa 2>/dev/null)
ALSA_LIBS = $(shell pkg-config --libs alsa 2>/dev/null)

# 如果找到pkg-config中的ALSA配置，使用它
ifneq ($(ALSA_CFLAGS),)
    CFLAGS += $(ALSA_CFLAGS)
endif

ifneq ($(ALSA_LIBS),)
    LDFLAGS = $(ALSA_LIBS) -lm -lpthread
endif

.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(SOURCES)
	@echo "Compiling MMAP audio example..."
	@if ! pkg-config --exists alsa; then \
		echo "Error: ALSA development library not found!"; \
		echo "Please install: sudo apt-get install libasound2-dev"; \
		exit 1; \
	fi
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build completed: $@"

clean:
	rm -f $(TARGET) *.o

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)

# 测试不同的音频设备
test: $(TARGET)
	@echo "Testing MMAP audio playback..."
	@echo "Available audio devices:"
	@aplay -l 2>/dev/null || echo "No audio devices found"
	@echo ""
	@echo "Testing with default device:"
	./$(TARGET) default
	
test-hw: $(TARGET)
	@echo "Testing with hardware device hw:0,0:"
	./$(TARGET) hw:0,0

test-plughw: $(TARGET)  
	@echo "Testing with plughw device:"
	./$(TARGET) plughw:0,0

# 检查系统MMAP支持
check-mmap:
	@echo "Checking MMAP support in ALSA devices:"
	@for card in /proc/asound/card*; do \
		if [ -d "$$card" ]; then \
			echo "Card: $$card"; \
			for pcm in $$card/pcm*/info; do \
				if [ -f "$$pcm" ]; then \
					echo "  PCM: $$pcm"; \
					grep -H "subclass\|capabilities" "$$pcm" 2>/dev/null; \
				fi; \
			done; \
		fi; \
	done

# 显示ALSA配置信息
alsa-info:
	@echo "=== ALSA System Information ==="
	@echo "ALSA version:"
	@cat /proc/asound/version 2>/dev/null || echo "ALSA not available"
	@echo ""
	@echo "Sound cards:"
	@cat /proc/asound/cards 2>/dev/null || echo "No sound cards found"
	@echo ""
	@echo "PCM devices:"
	@cat /proc/asound/pcm 2>/dev/null || echo "No PCM devices found"

# 性能测试
perf-test: $(TARGET)
	@echo "Running performance test..."
	@perf record -g ./$(TARGET) hw:0,0 2>/dev/null || \
		echo "Performance monitoring not available (install perf-tools)"

# 调试版本
debug: CFLAGS += -DDEBUG -g3
debug: $(TARGET)
	@echo "Debug version built with extended debugging info"

help:
	@echo "MMAP Audio Example Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build the MMAP audio example"
	@echo "  clean      - Remove build artifacts"
	@echo "  install    - Install to /usr/local/bin"
	@echo "  test       - Test with default audio device"
	@echo "  test-hw    - Test with hw:0,0 device"
	@echo "  test-plughw- Test with plughw:0,0 device"
	@echo "  check-mmap - Check system MMAP support"
	@echo "  alsa-info  - Display ALSA system information"
	@echo "  debug      - Build debug version"
	@echo "  help       - Show this help message"
	@echo ""
	@echo "Prerequisites:"
	@echo "  sudo apt-get install libasound2-dev build-essential" 