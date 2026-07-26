# ============================================================
# Makefile لـ XRAY-SCOPE v1.0
# ============================================================

PROJECT = xray
VERSION = 1.0
PREFIX = /usr/local

CC = gcc
CFLAGS = -O2 -Wall -Wextra -fPIC -D_GNU_SOURCE
LDFLAGS = -lm -lpthread -ldl -lzstd -lxcb -lX11 -lmnl

SRC_DIR = src
BUILD_DIR = build
DIST_DIR = dist

.PHONY: all clean install uninstall dist test

all: $(DIST_DIR) $(BUILD_DIR) xray final

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(DIST_DIR):
	@mkdir -p $(DIST_DIR)

xray: $(BUILD_DIR)/main.o $(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o \
      $(BUILD_DIR)/softpipe.o $(BUILD_DIR)/interface.o
	@echo "🔗 ربط الملف الرئيسي..."
	@$(CC) -o $(BUILD_DIR)/$(PROJECT) $(BUILD_DIR)/main.o \
		$(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o \
		$(BUILD_DIR)/softpipe.o $(BUILD_DIR)/interface.o \
		$(LDFLAGS)

$(BUILD_DIR)/main.o: main.c
	@mkdir -p $(BUILD_DIR)
	@$(CC) $(CFLAGS) -c main.c -o $(BUILD_DIR)/main.o -I. -Iengine -Ishared -Iui -Igfx

$(BUILD_DIR)/shm.o: shared/shm.c
	@$(CC) $(CFLAGS) -c shared/shm.c -o $(BUILD_DIR)/shm.o -I.

$(BUILD_DIR)/ring.o: shared/ring_buffer.c
	@$(CC) $(CFLAGS) -c shared/ring_buffer.c -o $(BUILD_DIR)/ring.o -I.

$(BUILD_DIR)/softpipe.o: gfx/softpipe.c
	@$(CC) $(CFLAGS) -c gfx/softpipe.c -o $(BUILD_DIR)/softpipe.o -lm

$(BUILD_DIR)/interface.o: ui/interface.c
	@$(CC) $(CFLAGS) -c ui/interface.c -o $(BUILD_DIR)/interface.o -I. -Iui -Ishared -I/usr/include/X11 -lm

polling.so: engine/polling.c
	@$(CC) $(CFLAGS) -shared -o polling.so engine/polling.c -lm -lpthread -I.

final:
	@echo "📦 إنشاء الملف النهائي..."
	@cp $(BUILD_DIR)/$(PROJECT) $(DIST_DIR)/$(PROJECT)_final
	@chmod +x $(DIST_DIR)/$(PROJECT)_final
	@cp polling.so $(DIST_DIR)/ 2>/dev/null || true
	@echo "✅ الملف النهائي: $(DIST_DIR)/$(PROJECT)_final"

install:
	@echo "📦 تثبيت XRAY-SCOPE..."
	@sudo cp $(DIST_DIR)/$(PROJECT)_final $(PREFIX)/bin/$(PROJECT)
	@sudo chmod +x $(PREFIX)/bin/$(PROJECT)
	@echo "✅ تم التثبيت!"

uninstall:
	@echo "🗑️  إزالة XRAY-SCOPE..."
	@sudo rm -f $(PREFIX)/bin/$(PROJECT)
	@echo "✅ تم الإزالة"

clean:
	@echo "🧹 تنظيف..."
	@rm -rf $(BUILD_DIR) $(DIST_DIR)
	@rm -f *.o *.so xray
	@echo "✅ تم التنظيف"

dist: all
	@echo "📦 إنشاء حزمة التوزيع..."
	@mkdir -p $(DIST_DIR)/package
	@cp $(DIST_DIR)/$(PROJECT)_final $(DIST_DIR)/package/$(PROJECT)
	@cp README.md $(DIST_DIR)/package/ 2>/dev/null || true
	@cd $(DIST_DIR)/package && tar -czf ../$(PROJECT)-$(VERSION)-linux.tar.gz *
	@echo "✅ حزمة التوزيع: $(DIST_DIR)/$(PROJECT)-$(VERSION)-linux.tar.gz"

test: all
	@echo "🧪 اختبار XRAY-SCOPE..."
	@$(DIST_DIR)/$(PROJECT)_final --help || echo "⚠️  فشل الاختبار"

run: all
	@$(DIST_DIR)/$(PROJECT)_final
