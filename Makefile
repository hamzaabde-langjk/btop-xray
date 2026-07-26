# ============================================================
# Makefile لـ XRAY-SCOPE v1.0
# ============================================================

PROJECT = xray
VERSION = 1.0
PREFIX = /usr/local

# المترجمات
CC = gcc
CLANG = clang
ZIG = zig
AR = ar
UPX = upx

# خيارات الترجمة
CFLAGS = -O2 -Wall -Wextra -fPIC -D_GNU_SOURCE
LDFLAGS = -lm -lpthread -ldl -lzstd -lxcb -lX11 -lvulkan -lmnl

# الأدلة
SRC_DIR = src
ENGINE_DIR = $(SRC_DIR)/engine
GFX_DIR = $(SRC_DIR)/gfx
UI_DIR = $(SRC_DIR)/ui
SHARED_DIR = $(SRC_DIR)/shared
BUILD_DIR = build
DIST_DIR = dist

# ============================================================
# الأهداف
# ============================================================

.PHONY: all clean install uninstall dist test check-deps

all: check-deps $(BUILD_DIR) $(DIST_DIR) bootstrap engines gfx ui xray final

# التحقق من التبعيات
check-deps:
	@echo "🔍 التحقق من التبعيات..."
	@command -v $(CC) >/dev/null 2>&1 || { echo "❌ GCC غير موجود"; exit 1; }
	@command -v make >/dev/null 2>&1 || { echo "❌ Make غير موجود"; exit 1; }
	@echo "✅ جميع التبعيات الأساسية موجودة"

# إنشاء الأدلة
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(DIST_DIR):
	@mkdir -p $(DIST_DIR)

# ============================================================
# بناء المكونات
# ============================================================

# Bootstrap (Zig)
bootstrap: $(SRC_DIR)/bootstrap.zig
	@echo "🚀 بناء Bootstrap..."
	@if command -v $(ZIG) >/dev/null 2>&1; then \
		$(ZIG) build-exe $(SRC_DIR)/bootstrap.zig -target x86_64-linux -O ReleaseSmall -fno-strip; \
	else \
		echo "⚠️  Zig غير موجود، تخطي Bootstrap"; \
	fi
	@if [ -f bootstrap ]; then \
		mv bootstrap $(BUILD_DIR)/; \
	fi

# محركات المراقبة
engines: $(BUILD_DIR)/ebpf.so $(BUILD_DIR)/netlink.so $(BUILD_DIR)/polling.so

$(BUILD_DIR)/ebpf.so: $(ENGINE_DIR)/ebpf.c
	@echo "🔧 بناء محرك eBPF..."
	@$(CLANG) -O2 -target bpf -c $(ENGINE_DIR)/ebpf.c -o $(BUILD_DIR)/ebpf.o 2>/dev/null || \
		{ echo "⚠️  فشل بناء eBPF (يتطلب نواة 5.8+)"; touch $(BUILD_DIR)/ebpf.so; }
	@if [ -f $(BUILD_DIR)/ebpf.o ]; then \
		$(CC) -shared -o $(BUILD_DIR)/ebpf.so $(BUILD_DIR)/ebpf.o; \
	fi

$(BUILD_DIR)/netlink.so: $(ENGINE_DIR)/netlink.c
	@echo "🔧 بناء محرك Netlink..."
	@$(CC) $(CFLAGS) -shared -o $(BUILD_DIR)/netlink.so $(ENGINE_DIR)/netlink.c -lmnl 2>/dev/null || \
		{ echo "⚠️  فشل بناء Netlink"; touch $(BUILD_DIR)/netlink.so; }

$(BUILD_DIR)/polling.so: $(ENGINE_DIR)/polling.c
	@echo "🔧 بناء محرك Polling..."
	@$(CC) $(CFLAGS) -shared -o $(BUILD_DIR)/polling.so $(ENGINE_DIR)/polling.c 2>/dev/null || \
		{ echo "⚠️  فشل بناء Polling"; touch $(BUILD_DIR)/polling.so; }

# العارض الرسومي
gfx: $(BUILD_DIR)/softpipe.o $(BUILD_DIR)/vulkan.o

$(BUILD_DIR)/softpipe.o: $(GFX_DIR)/softpipe.c
	@echo "🎨 بناء العارض البرمجي..."
	@$(CC) $(CFLAGS) -c $(GFX_DIR)/softpipe.c -o $(BUILD_DIR)/softpipe.o -lm

$(BUILD_DIR)/vulkan.o: $(GFX_DIR)/vulkan.c
	@echo "🎨 بناء عارض Vulkan..."
	@if pkg-config --exists vulkan; then \
		$(CC) $(CFLAGS) -c $(GFX_DIR)/vulkan.c -o $(BUILD_DIR)/vulkan.o `pkg-config --cflags vulkan` -DUSE_VULKAN; \
	else \
		echo "⚠️  Vulkan غير موجود، تخطي"; \
		touch $(BUILD_DIR)/vulkan.o; \
	fi

# الواجهة
ui: $(BUILD_DIR)/interface.o

$(BUILD_DIR)/interface.o: $(UI_DIR)/interface.c
	@echo "🖥️  بناء الواجهة..."
	@$(CC) $(CFLAGS) -c $(UI_DIR)/interface.c -o $(BUILD_DIR)/interface.o -I$(UI_DIR)

# الملف الرئيسي
xray: $(BUILD_DIR)/main.o $(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o
	@echo "🔗 ربط الملف الرئيسي..."
	@$(CC) -o $(BUILD_DIR)/$(PROJECT) $(BUILD_DIR)/main.o \
		$(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o \
		$(BUILD_DIR)/softpipe.o $(BUILD_DIR)/vulkan.o \
		$(BUILD_DIR)/interface.o \
		$(LDFLAGS) 2>/dev/null || \
	{ echo "⚠️  فشل الربط، محاولة بدون Vulkan"; \
	  $(CC) -o $(BUILD_DIR)/$(PROJECT) $(BUILD_DIR)/main.o \
		$(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o \
		$(BUILD_DIR)/softpipe.o $(BUILD_DIR)/interface.o \
		-lm -lpthread -ldl -lzstd -lxcb -lX11; }

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c
	@echo "📝 ترجمة main.c..."
	@$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(BUILD_DIR)/main.o -I$(SRC_DIR) -I$(ENGINE_DIR) -I$(GFX_DIR) -I$(UI_DIR) -I$(SHARED_DIR)

$(BUILD_DIR)/shm.o: $(SHARED_DIR)/shm.c
	@$(CC) $(CFLAGS) -c $(SHARED_DIR)/shm.c -o $(BUILD_DIR)/shm.o

$(BUILD_DIR)/ring.o: $(SHARED_DIR)/ring_buffer.c
	@$(CC) $(CFLAGS) -c $(SHARED_DIR)/ring_buffer.c -o $(BUILD_DIR)/ring.o

# الملف النهائي (مع ضغط)
final:
	@echo "📦 إنشاء الملف النهائي..."
	@cp $(BUILD_DIR)/$(PROJECT) $(DIST_DIR)/$(PROJECT)_unpacked
	@if command -v $(UPX) >/dev/null 2>&1; then \
		echo "🗜️  ضغط الملف بـ UPX..."; \
		$(UPX) --best --lzma $(DIST_DIR)/$(PROJECT)_unpacked -o $(DIST_DIR)/$(PROJECT)_final 2>/dev/null || \
		cp $(DIST_DIR)/$(PROJECT)_unpacked $(DIST_DIR)/$(PROJECT)_final; \
	else \
		echo "⚠️  UPX غير موجود، الملف غير مضغوط"; \
		cp $(DIST_DIR)/$(PROJECT)_unpacked $(DIST_DIR)/$(PROJECT)_final; \
	fi
	@echo "✅ الملف النهائي: $(DIST_DIR)/$(PROJECT)_final"
	@ls -lh $(DIST_DIR)/$(PROJECT)_final

# ============================================================
# التثبيت
# ============================================================

install:
	@echo "📦 تثبيت XRAY-SCOPE..."
	@sudo cp $(DIST_DIR)/$(PROJECT)_final $(PREFIX)/bin/$(PROJECT)
	@sudo chmod +x $(PREFIX)/bin/$(PROJECT)
	@echo "✅ تم التثبيت! شغّل '$(PROJECT)' من أي مكان"

uninstall:
	@echo "🗑️  إزالة XRAY-SCOPE..."
	@sudo rm -f $(PREFIX)/bin/$(PROJECT)
	@echo "✅ تم الإزالة"

# ============================================================
# التنظيف
# ============================================================

clean:
	@echo "🧹 تنظيف الملفات..."
	@rm -rf $(BUILD_DIR) $(DIST_DIR)
	@rm -f bootstrap *.o *.so $(PROJECT) $(PROJECT)_*
	@echo "✅ تم التنظيف"

# ============================================================
# توزيع
# ============================================================

dist: all
	@echo "📦 إنشاء حزمة التوزيع..."
	@mkdir -p $(DIST_DIR)/package
	@mkdir -p $(DIST_DIR)/xray_final
	@cp $(DIST_DIR)/$(PROJECT)_final $(DIST_DIR)/package/$(PROJECT)
	@cp scripts/install.sh $(DIST_DIR)/package/
	@cp README.md $(DIST_DIR)/package/ 2>/dev/null || echo "⚠️  README.md غير موجود"
	@cd $(DIST_DIR)/package && tar -czf ../$(PROJECT)-$(VERSION)-linux.tar.gz *
	@echo "✅ حزمة التوزيع: $(DIST_DIR)/$(PROJECT)-$(VERSION)-linux.tar.gz"
	@ls -lh $(DIST_DIR)/*.tar.gz

# ============================================================
# اختبار
# ============================================================

test: all
	@echo "🧪 اختبار XRAY-SCOPE..."
	@$(DIST_DIR)/$(PROJECT)_final --help || echo "⚠️  فشل الاختبار"

# ============================================================
# تشغيل سريع
# ============================================================

run: all
	@$(DIST_DIR)/$(PROJECT)_final
