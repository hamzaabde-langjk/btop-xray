#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
XRAY-SCOPE v1.0 - نظام البناء الذكي
"رؤية كل بايت، في كل مكان، في لحظة واحدة"

هذا النظام يقوم ب:
1. إنشاء هيكل المشروع بالكامل
2. إدارة التبعيات
3. بناء الملفات الثنائية
4. إنشاء حزمة التوزيع
5. اختبار الأداء
"""

import os
import sys
import json
import shutil
import subprocess
import platform
import argparse
import time
import hashlib
import tarfile
import zipfile
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass, field
from datetime import datetime
import urllib.request
import tempfile

# ============================================================
# بيانات المشروع
# ============================================================

@dataclass
class ProjectConfig:
    """تكوين المشروع"""
    name: str = "xray-scope"
    version: str = "1.0"
    author: str = "XRAY-SCOPE Team"
    description: str = "نظام مراقبة متكامل لأنظمة Linux"
    license: str = "MIT"
    
    # المسارات
    src_dir: str = "src"
    build_dir: str = "build"
    dist_dir: str = "dist"
    assets_dir: str = "assets"
    scripts_dir: str = "scripts"
    
    # الملفات
    main_file: str = "main.c"
    makefile: str = "Makefile"
    readme: str = "README.md"
    
    # التبعيات
    dependencies: List[str] = field(default_factory=lambda: [
        "gcc", "make", "libx11-dev", "libxcb-dev", 
        "libvulkan-dev", "libzstd-dev", "libmnl-dev",
        "linux-headers-$(uname -r)"
    ])
    
    # المحركات
    engines: List[str] = field(default_factory=lambda: [
        "ebpf", "netlink", "polling"
    ])
    
    # المكونات
    components: Dict[str, List[str]] = field(default_factory=lambda: {
        "engine": ["adapter.h", "ebpf.c", "netlink.c", "polling.c"],
        "gfx": ["renderer.h", "vulkan.c", "softpipe.c"],
        "ui": ["interface.h", "interface.c", "nuklear.h"],
        "shared": ["shm.h", "shm.c", "ring_buffer.h", "ring_buffer.c"]
    })

# ============================================================
# نظام الألوان للمخرجات
# ============================================================

class Colors:
    """ألوان الطرفية"""
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    RESET = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

def print_colored(text: str, color: str = Colors.RESET, bold: bool = False):
    """طباعة نص ملون"""
    prefix = Colors.BOLD if bold else ""
    print(f"{prefix}{color}{text}{Colors.RESET}")

# ============================================================
# نظام البناء الأساسي
# ============================================================

class XrayBuilder:
    """فئة البناء الرئيسية"""
    
    def __init__(self, config: ProjectConfig):
        self.config = config
        self.root_dir = Path.cwd()
        self.start_time = time.time()
        self.errors = []
        self.warnings = []
        
        # إنشاء الأدلة
        self.dirs = {
            'src': self.root_dir / config.src_dir,
            'build': self.root_dir / config.build_dir,
            'dist': self.root_dir / config.dist_dir,
            'assets': self.root_dir / config.assets_dir,
            'scripts': self.root_dir / config.scripts_dir,
        }
        
        # مكونات المشروع
        self.components = {
            'engine': self.dirs['src'] / 'engine',
            'gfx': self.dirs['src'] / 'gfx',
            'ui': self.dirs['src'] / 'ui',
            'shared': self.dirs['src'] / 'shared',
        }
        
    def create_directory_structure(self):
        """إنشاء هيكل المجلدات"""
        print_colored("\n📁 إنشاء هيكل المشروع...", Colors.CYAN)
        
        for dir_path in self.dirs.values():
            dir_path.mkdir(parents=True, exist_ok=True)
            print_colored(f"  ✅ {dir_path}", Colors.GREEN)
        
        for comp_name, comp_path in self.components.items():
            comp_path.mkdir(parents=True, exist_ok=True)
            print_colored(f"  ✅ {comp_path}", Colors.GREEN)
            
        # إنشاء مجلدات فرعية إضافية
        (self.dirs['assets'] / 'fonts').mkdir(exist_ok=True)
        (self.dirs['scripts']).mkdir(exist_ok=True)
        
    def create_main_files(self):
        """إنشاء الملفات الرئيسية"""
        print_colored("\n📝 إنشاء الملفات الرئيسية...", Colors.CYAN)
        
        files = {
            self.dirs['src'] / self.config.main_file: self.generate_main_c(),
            self.dirs['src'] / 'bootstrap.zig': self.generate_bootstrap_zig(),
            self.root_dir / self.config.makefile: self.generate_makefile(),
            self.root_dir / self.config.readme: self.generate_readme(),
            self.root_dir / '.gitignore': self.generate_gitignore(),
        }
        
        # إضافة ملفات التوزيع
        for comp_name, comp_files in self.config.components.items():
            comp_dir = self.components[comp_name]
            for file_name in comp_files:
                if file_name.endswith('.h'):
                    file_path = comp_dir / file_name
                    if file_name == 'adapter.h':
                        content = self.generate_adapter_h()
                    elif file_name == 'renderer.h':
                        content = self.generate_renderer_h()
                    elif file_name == 'interface.h':
                        content = self.generate_interface_h()
                    elif file_name == 'shm.h':
                        content = self.generate_shm_h()
                    elif file_name == 'ring_buffer.h':
                        content = self.generate_ring_buffer_h()
                    else:
                        content = f"// {file_name}\n"
                    files[file_path] = content
                    
                elif file_name.endswith('.c'):
                    file_path = comp_dir / file_name
                    if file_name == 'main.c':
                        content = self.generate_main_c()
                    elif file_name == 'ebpf.c':
                        content = self.generate_ebpf_c()
                    elif file_name == 'netlink.c':
                        content = self.generate_netlink_c()
                    elif file_name == 'polling.c':
                        content = self.generate_polling_c()
                    elif file_name == 'vulkan.c':
                        content = self.generate_vulkan_c()
                    elif file_name == 'softpipe.c':
                        content = self.generate_softpipe_c()
                    elif file_name == 'interface.c':
                        content = self.generate_interface_c()
                    elif file_name == 'shm.c':
                        content = self.generate_shm_c()
                    elif file_name == 'ring_buffer.c':
                        content = self.generate_ring_buffer_c()
                    else:
                        content = f"// {file_name}\n"
                    files[file_path] = content
        
        # كتابة الملفات
        for file_path, content in files.items():
            if content:  # فقط إذا كان المحتوى غير فارغ
                file_path.parent.mkdir(parents=True, exist_ok=True)
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                print_colored(f"  ✅ {file_path}", Colors.GREEN)
    
    def generate_main_c(self) -> str:
        """توليد ملف main.c"""
        return '''/**
 * XRAY-SCOPE v1.0 - Omniscient Lens
 * نظام مراقبة متكامل لأنظمة Linux
 * 
 * حقوق النشر 2026 - المشروع مفتوح المصدر
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdatomic.h>

#include "engine/adapter.h"
#include "gfx/renderer.h"
#include "ui/interface.h"
#include "shared/shm.h"
#include "shared/ring_buffer.h"

#define VERSION "1.0"
#define PROJECT_NAME "XRAY-SCOPE"
#define MAX_ENGINES 3
#define DEFAULT_SHM_KEY 0x58415259

/* بنيات البيانات */
typedef struct {
    char name[64];
    int pid;
    int ppid;
    char cmdline[256];
    char cwd[256];
    char state;
    long cpu_usage;
    long memory_usage;
    unsigned long long io_read;
    unsigned long long io_write;
    int connections[32];
    int conn_count;
    uint64_t inodes[64];
    int inode_count;
    double latitude;
    double longitude;
    uint64_t last_update;
} ProcessInfo;

typedef struct {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t event_type;
    uint64_t inode;
    char path[256];
    char remote_ip[16];
    uint16_t remote_port;
    uint64_t bytes_transferred;
} SystemEvent;

typedef struct {
    uint64_t magic;
    uint32_t version;
    uint32_t num_processes;
    uint32_t num_events;
    uint64_t timestamp;
    ProcessInfo processes[4096];
    SystemEvent events[16384];
    uint32_t process_count;
    uint32_t event_count;
    atomic_flag lock;
    char reserved[1024];
} SharedMemoryHeader;

static volatile bool running = true;
static SharedMemoryHeader *shm = NULL;
static RingBuffer *ring = NULL;
static EngineAdapter *engine = NULL;
static Renderer *renderer = NULL;
static UIInterface *ui = NULL;

static void signal_handler(int sig) {
    (void)sig;
    running = false;
    fprintf(stderr, "\\n⏹️  إيقاف التشغيل...\\n");
}

static void print_banner(void) {
    printf("\\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\\n");
    printf("║  ██╗  ██╗██████╗  █████╗ ██╗   ██╗      ███████╗ ██████╗ ██████╗ ██████╗ ███████╗ ║\\n");
    printf("║  ╚██╗██╔╝██╔══██╗██╔══██╗╚██╗ ██╔╝      ██╔════╝██╔═══██╗██╔══██╗██╔══██╗██╔════╝ ║\\n");
    printf("║   ╚███╔╝ ██████╔╝███████║ ╚████╔╝       ███████╗██║   ██║██████╔╝██████╔╝█████╗   ║\\n");
    printf("║   ██╔██╗ ██╔══██╗██╔══██║  ╚██╔╝        ╚════██║██║   ██║██╔═══╝ ██╔══██╗██╔══╝   ║\\n");
    printf("║  ██╔╝ ██╗██║  ██║██║  ██║   ██║         ███████║╚██████╔╝██║     ██║  ██║███████╗ ║\\n");
    printf("║  ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝         ╚══════╝ ╚═════╝ ╚═╝     ╚═╝  ╚═╝╚══════╝ ║\\n");
    printf("║                                                                                      ║\\n");
    printf("║  🌍 Omniscient Lens v%s - رؤية كل بايت، في كل مكان، في لحظة واحدة  ║\\n", VERSION);
    printf("╚══════════════════════════════════════════════════════════════════╝\\n");
    printf("\\n");
}

static void print_help(void) {
    printf("الاستخدام: xray [خيارات]\\n\\n");
    printf("الخيارات:\\n");
    printf("  --help, -h           عرض هذه المساعدة\\n");
    printf("  --version, -v        عرض رقم الإصدار\\n");
    printf("  --engine <name>      فرض محرك معين (ebpf, netlink, polling)\\n");
    printf("  --nogpu              تعطيل GPU واستخدام العارض البرمجي\\n");
    printf("  --obsidian           فتح واجهة Obsidian-only\\n");
    printf("  --headless           وضع الخادم (بدون عرض)\\n");
    printf("  --filter <expr>      تصفية العمليات\\n");
    printf("  --shm-key <hex>      مفتاح الذاكرة المشتركة\\n");
    printf("\\n");
    printf("الأمثلة:\\n");
    printf("  ./xray                    # تشغيل عادي\\n");
    printf("  ./xray --engine ebpf      # استخدام eBPF\\n");
    printf("  ./xray --nogpu --headless # وضع خادم بدون عرض\\n");
    printf("\\n");
}

static EngineAdapter* detect_engine(const char *forced_engine) {
    const char *engine_names[] = {"ebpf", "netlink", "polling"};
    const char *engine_files[] = {"ebpf.so", "netlink.so", "polling.so"};
    
    if (forced_engine) {
        for (int i = 0; i < MAX_ENGINES; i++) {
            if (strcmp(forced_engine, engine_names[i]) == 0) {
                void *handle = dlopen(engine_files[i], RTLD_NOW);
                if (handle) {
                    EngineAdapter *adapter = (EngineAdapter*)dlsym(handle, "adapter");
                    if (adapter && adapter->init()) {
                        printf("✅ استخدام المحرك المطلوب: %s\\n", engine_names[i]);
                        return adapter;
                    }
                }
                fprintf(stderr, "❌ فشل تحميل المحرك المطلوب: %s\\n", forced_engine);
                return NULL;
            }
        }
        fprintf(stderr, "❌ محرك غير معروف: %s\\n", forced_engine);
        return NULL;
    }
    
    printf("🔍 اكتشاف المحرك المناسب...\\n");
    
    /* محاولة eBPF أولاً */
    void *handle = dlopen("ebpf.so", RTLD_NOW);
    if (handle) {
        EngineAdapter *adapter = (EngineAdapter*)dlsym(handle, "adapter");
        if (adapter && adapter->init()) {
            printf("✅ استخدام محرك eBPF (أسرع محرك)\\n");
            return adapter;
        }
        dlclose(handle);
    }
    
    /* ثم Netlink */
    handle = dlopen("netlink.so", RTLD_NOW);
    if (handle) {
        EngineAdapter *adapter = (EngineAdapter*)dlsym(handle, "adapter");
        if (adapter && adapter->init()) {
            printf("✅ استخدام محرك Netlink (متوسط السرعة)\\n");
            return adapter;
        }
        dlclose(handle);
    }
    
    /* أخيراً Polling */
    handle = dlopen("polling.so", RTLD_NOW);
    if (handle) {
        EngineAdapter *adapter = (EngineAdapter*)dlsym(handle, "adapter");
        if (adapter && adapter->init()) {
            printf("✅ استخدام محرك Polling (بطيء لكن يعمل دائماً)\\n");
            return adapter;
        }
        dlclose(handle);
    }
    
    fprintf(stderr, "❌ فشل تحميل أي محرك!\\n");
    return NULL;
}

static void* collect_data_thread(void *arg) {
    (void)arg;
    
    while (running && engine) {
        SystemEvent events[256];
        int count = engine->poll_events(events, 256);
        
        if (count > 0) {
            while (atomic_flag_test_and_set(&shm->lock));
            
            for (int i = 0; i < count && shm->event_count < 16384; i++) {
                shm->events[shm->event_count++] = events[i];
            }
            
            ProcessInfo procs[256];
            int proc_count = engine->get_processes(procs, 256);
            if (proc_count > 0) {
                shm->process_count = proc_count;
                memcpy(shm->processes, procs, proc_count * sizeof(ProcessInfo));
            }
            
            shm->num_events = shm->event_count;
            shm->num_processes = shm->process_count;
            shm->timestamp = time(NULL);
            
            atomic_flag_clear(&shm->lock);
        }
        
        usleep(10000);
    }
    
    return NULL;
}

int main(int argc, char **argv) {
    bool use_gpu = true;
    bool obsidian_only = false;
    bool headless = false;
    const char *forced_engine = NULL;
    const char *filter_expr = NULL;
    unsigned int shm_key = DEFAULT_SHM_KEY;
    
    struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"engine", required_argument, 0, 'e'},
        {"nogpu", no_argument, 0, 'g'},
        {"obsidian", no_argument, 0, 'o'},
        {"headless", no_argument, 0, 'H'},
        {"filter", required_argument, 0, 'f'},
        {"shm-key", required_argument, 0, 's'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "hve:goHf:s:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'h': print_help(); return 0;
            case 'v': printf("XRAY-SCOPE v%s\\n", VERSION); return 0;
            case 'e': forced_engine = optarg; break;
            case 'g': use_gpu = false; break;
            case 'o': obsidian_only = true; break;
            case 'H': headless = true; break;
            case 'f': filter_expr = optarg; break;
            case 's': shm_key = strtoul(optarg, NULL, 16); break;
            default: print_help(); return 1;
        }
    }
    
    print_banner();
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    shm = shm_init(shm_key, sizeof(SharedMemoryHeader));
    if (!shm) {
        fprintf(stderr, "❌ فشل تهيئة الذاكرة المشتركة\\n");
        return 1;
    }
    
    memset(shm, 0, sizeof(SharedMemoryHeader));
    shm->magic = 0x58415259;
    shm->version = 1;
    atomic_flag_clear(&shm->lock);
    
    ring = ring_buffer_create(16384, sizeof(SystemEvent));
    if (!ring) {
        fprintf(stderr, "❌ فشل إنشاء الحلقة الدائرية\\n");
        return 1;
    }
    
    engine = detect_engine(forced_engine);
    if (!engine) {
        fprintf(stderr, "❌ فشل تحميل المحرك\\n");
        return 1;
    }
    
    if (!headless) {
        renderer = renderer_create(use_gpu, obsidian_only);
        if (!renderer) {
            fprintf(stderr, "⚠️  فشل تهيئة العارض، التشغيل في وضع headless\\n");
            headless = true;
        }
    }
    
    if (!headless) {
        ui = ui_create(renderer, shm, filter_expr);
        if (!ui) {
            fprintf(stderr, "⚠️  فشل تهيئة الواجهة، التشغيل في وضع headless\\n");
            headless = true;
        }
    }
    
    pthread_t collector_thread;
    if (pthread_create(&collector_thread, NULL, collect_data_thread, NULL) != 0) {
        fprintf(stderr, "❌ فشل إنشاء خيط جمع البيانات\\n");
        return 1;
    }
    
    printf("🚀 XRAY-SCOPE يعمل... (اضغط Ctrl+C للإيقاف)\\n\\n");
    
    if (headless) {
        while (running) {
            sleep(1);
            static uint64_t last_log = 0;
            if (time(NULL) - last_log > 5) {
                while (atomic_flag_test_and_set(&shm->lock));
                printf("📊 العمليات: %d, الأحداث: %d\\n", 
                       shm->process_count, shm->event_count);
                atomic_flag_clear(&shm->lock);
                last_log = time(NULL);
            }
        }
    } else {
        while (running) {
            ui->update(ui);
            ui->render(ui);
            usleep(16666);
        }
    }
    
    running = false;
    pthread_join(collector_thread, NULL);
    
    if (ui) ui_destroy(ui);
    if (renderer) renderer_destroy(renderer);
    if (engine) engine->cleanup();
    if (ring) ring_buffer_destroy(ring);
    if (shm) shm_destroy(shm);
    
    printf("\\n✅ XRAY-SCOPE توقف بنجاح\\n");
    return 0;
}
'''
    
    def generate_adapter_h(self) -> str:
        """توليد ملف adapter.h"""
        return '''/**
 * engine/adapter.h
 * واجهة موحدة لمحركات المراقبة
 */

#ifndef ENGINE_ADAPTER_H
#define ENGINE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char name[64];
    int pid;
    int ppid;
    char cmdline[256];
    char cwd[256];
    char state;
    long cpu_usage;
    long memory_usage;
    unsigned long long io_read;
    unsigned long long io_write;
    int connections[32];
    int conn_count;
    uint64_t inodes[64];
    int inode_count;
    double latitude;
    double longitude;
    uint64_t last_update;
} ProcessInfo;

typedef struct {
    uint64_t timestamp;
    uint32_t pid;
    uint32_t event_type;
    uint64_t inode;
    char path[256];
    char remote_ip[16];
    uint16_t remote_port;
    uint64_t bytes_transferred;
} SystemEvent;

typedef struct {
    bool (*init)(void);
    int (*get_processes)(ProcessInfo *buffer, int max_count);
    int (*poll_events)(SystemEvent *buffer, int max_count);
    void (*cleanup)(void);
    const char *name;
    uint32_t events_per_second;
    uint32_t memory_usage;
} EngineAdapter;

extern EngineAdapter adapter;

#endif /* ENGINE_ADAPTER_H */
'''
    
    def generate_bootstrap_zig(self) -> str:
        """توليد ملف bootstrap.zig"""
        return '''//! bootstrap.zig
//! محمل ذاتي لـ XRAY-SCOPE

const std = @import("std");
const posix = std.posix;

pub fn main() !void {
    const blue = "\\x1b[34m";
    const green = "\\x1b[32m";
    const reset = "\\x1b[0m";
    
    try std.io.getStdOut().writer().print(
        \\\\{s}╔══════════════════════════════════════════╗{s}
        \\\\{s}║  XRAY-SCOPE Bootstrap v1.0             ║{s}
        \\\\{s}║  تحميل النظام...                       ║{s}
        \\\\{s}╚══════════════════════════════════════════╝{s}
        \\\\
    , .{blue, reset, blue, reset, blue, reset, blue, reset});
    
    const self_path = try std.fs.selfExePathAlloc(std.heap.page_allocator);
    defer std.heap.page_allocator.free(self_path);
    
    const main_binary = try std.fs.cwd().openFile("xray_final", .{});
    defer main_binary.close();
    
    const stat = try main_binary.stat();
    const binary_data = try std.heap.page_allocator.alloc(u8, @as(usize, @intCast(stat.size)));
    defer std.heap.page_allocator.free(binary_data);
    
    _ = try main_binary.readAll(binary_data);
    
    const stdout = std.io.getStdOut().writer();
    try stdout.print("{s}📦 حجم الملف الثنائي: {d} بايت{s}\\n", .{green, binary_data.len, reset});
    
    var checksum: u64 = 0;
    for (binary_data) |byte| {
        checksum = checksum +% byte;
    }
    try stdout.print("{s}✅ المجموع الاختباري: 0x{x}{s}\\n", .{green, checksum, reset});
    
    try stdout.print("{s}🚀 تشغيل XRAY-SCOPE...{s}\\n", .{green, reset});
    
    const args = [_][]const u8{ "./xray_final" };
    _ = posix.execvpe("./xray_final", &args, std.os.environ);
    
    @panic("فشل تشغيل الملف الثنائي الرئيسي");
}
'''
    
    def generate_makefile(self) -> str:
        """توليد ملف Makefile"""
        return '''# ============================================================
# Makefile لـ XRAY-SCOPE v1.0
# ============================================================

PROJECT = xray
VERSION = 1.0
PREFIX = /usr/local

CC = gcc
CLANG = clang
ZIG = zig
AR = ar
UPX = upx

CFLAGS = -O2 -Wall -Wextra -fPIC -D_GNU_SOURCE
LDFLAGS = -lm -lpthread -ldl -lzstd -lxcb -lX11 -lvulkan -lmnl

SRC_DIR = src
ENGINE_DIR = $(SRC_DIR)/engine
GFX_DIR = $(SRC_DIR)/gfx
UI_DIR = $(SRC_DIR)/ui
SHARED_DIR = $(SRC_DIR)/shared
BUILD_DIR = build
DIST_DIR = dist

.PHONY: all clean install uninstall dist test check-deps

all: check-deps $(BUILD_DIR) $(DIST_DIR) bootstrap engines gfx ui xray final

check-deps:
\t@echo "🔍 التحقق من التبعيات..."
\t@command -v $(CC) >/dev/null 2>&1 || { echo "❌ GCC غير موجود"; exit 1; }
\t@command -v make >/dev/null 2>&1 || { echo "❌ Make غير موجود"; exit 1; }
\t@echo "✅ جميع التبعيات الأساسية موجودة"

$(BUILD_DIR):
\t@mkdir -p $(BUILD_DIR)

$(DIST_DIR):
\t@mkdir -p $(DIST_DIR)

bootstrap: $(SRC_DIR)/bootstrap.zig
\t@echo "🚀 بناء Bootstrap..."
\t@if command -v $(ZIG) >/dev/null 2>&1; then \\
\t\t$(ZIG) build-exe $(SRC_DIR)/bootstrap.zig -target x86_64-linux -O ReleaseSmall -fno-strip; \\
\telse \\
\t\techo "⚠️  Zig غير موجود، تخطي Bootstrap"; \\
\tfi
\t@if [ -f bootstrap ]; then \\
\t\tmv bootstrap $(BUILD_DIR)/; \\
\tfi

engines: $(BUILD_DIR)/ebpf.so $(BUILD_DIR)/netlink.so $(BUILD_DIR)/polling.so

$(BUILD_DIR)/ebpf.so: $(ENGINE_DIR)/ebpf.c
\t@echo "🔧 بناء محرك eBPF..."
\t@$(CLANG) -O2 -target bpf -c $(ENGINE_DIR)/ebpf.c -o $(BUILD_DIR)/ebpf.o 2>/dev/null || \\
\t\t{ echo "⚠️  فشل بناء eBPF (يتطلب نواة 5.8+)"; touch $(BUILD_DIR)/ebpf.so; }
\t@if [ -f $(BUILD_DIR)/ebpf.o ]; then \\
\t\t$(CC) -shared -o $(BUILD_DIR)/ebpf.so $(BUILD_DIR)/ebpf.o; \\
\tfi

$(BUILD_DIR)/netlink.so: $(ENGINE_DIR)/netlink.c
\t@echo "🔧 بناء محرك Netlink..."
\t@$(CC) $(CFLAGS) -shared -o $(BUILD_DIR)/netlink.so $(ENGINE_DIR)/netlink.c -lmnl 2>/dev/null || \\
\t\t{ echo "⚠️  فشل بناء Netlink"; touch $(BUILD_DIR)/netlink.so; }

$(BUILD_DIR)/polling.so: $(ENGINE_DIR)/polling.c
\t@echo "🔧 بناء محرك Polling..."
\t@$(CC) $(CFLAGS) -shared -o $(BUILD_DIR)/polling.so $(ENGINE_DIR)/polling.c 2>/dev/null || \\
\t\t{ echo "⚠️  فشل بناء Polling"; touch $(BUILD_DIR)/polling.so; }

gfx: $(BUILD_DIR)/softpipe.o $(BUILD_DIR)/vulkan.o

$(BUILD_DIR)/softpipe.o: $(GFX_DIR)/softpipe.c
\t@echo "🎨 بناء العارض البرمجي..."
\t@$(CC) $(CFLAGS) -c $(GFX_DIR)/softpipe.c -o $(BUILD_DIR)/softpipe.o -lm

$(BUILD_DIR)/vulkan.o: $(GFX_DIR)/vulkan.c
\t@echo "🎨 بناء عارض Vulkan..."
\t@if pkg-config --exists vulkan; then \\
\t\t$(CC) $(CFLAGS) -c $(GFX_DIR)/vulkan.c -o $(BUILD_DIR)/vulkan.o `pkg-config --cflags vulkan` -DUSE_VULKAN; \\
\telse \\
\t\techo "⚠️  Vulkan غير موجود، تخطي"; \\
\t\ttouch $(BUILD_DIR)/vulkan.o; \\
\tfi

ui: $(BUILD_DIR)/interface.o

$(BUILD_DIR)/interface.o: $(UI_DIR)/interface.c
\t@echo "🖥️  بناء الواجهة..."
\t@$(CC) $(CFLAGS) -c $(UI_DIR)/interface.c -o $(BUILD_DIR)/interface.o -I$(UI_DIR)

xray: $(BUILD_DIR)/main.o $(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o
\t@echo "🔗 ربط الملف الرئيسي..."
\t@$(CC) -o $(BUILD_DIR)/$(PROJECT) $(BUILD_DIR)/main.o \\
\t\t$(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o \\
\t\t$(BUILD_DIR)/softpipe.o $(BUILD_DIR)/vulkan.o \\
\t\t$(BUILD_DIR)/interface.o \\
\t\t$(LDFLAGS) 2>/dev/null || \\
\t{ echo "⚠️  فشل الربط، محاولة بدون Vulkan"; \\
\t  $(CC) -o $(BUILD_DIR)/$(PROJECT) $(BUILD_DIR)/main.o \\
\t\t$(BUILD_DIR)/shm.o $(BUILD_DIR)/ring.o \\
\t\t$(BUILD_DIR)/softpipe.o $(BUILD_DIR)/interface.o \\
\t\t-lm -lpthread -ldl -lzstd -lxcb -lX11; }

$(BUILD_DIR)/main.o: $(SRC_DIR)/main.c
\t@echo "📝 ترجمة main.c..."
\t@$(CC) $(CFLAGS) -c $(SRC_DIR)/main.c -o $(BUILD_DIR)/main.o -I$(SRC_DIR) -I$(ENGINE_DIR) -I$(GFX_DIR) -I$(UI_DIR) -I$(SHARED_DIR)

$(BUILD_DIR)/shm.o: $(SHARED_DIR)/shm.c
\t@$(CC) $(CFLAGS) -c $(SHARED_DIR)/shm.c -o $(BUILD_DIR)/shm.o

$(BUILD_DIR)/ring.o: $(SHARED_DIR)/ring_buffer.c
\t@$(CC) $(CFLAGS) -c $(SHARED_DIR)/ring_buffer.c -o $(BUILD_DIR)/ring.o

final:
\t@echo "📦 إنشاء الملف النهائي..."
\t@cp $(BUILD_DIR)/$(PROJECT) $(DIST_DIR)/$(PROJECT)_unpacked
\t@if command -v $(UPX) >/dev/null 2>&1; then \\
\t\techo "🗜️  ضغط الملف بـ UPX..."; \\
\t\t$(UPX) --best --lzma $(DIST_DIR)/$(PROJECT)_unpacked -o $(DIST_DIR)/$(PROJECT)_final 2>/dev/null || \\
\t\tcp $(DIST_DIR)/$(PROJECT)_unpacked $(DIST_DIR)/$(PROJECT)_final; \\
\telse \\
\t\techo "⚠️  UPX غير موجود، الملف غير مضغوط"; \\
\t\tcp $(DIST_DIR)/$(PROJECT)_unpacked $(DIST_DIR)/$(PROJECT)_final; \\
\tfi
\t@echo "✅ الملف النهائي: $(DIST_DIR)/$(PROJECT)_final"
\t@ls -lh $(DIST_DIR)/$(PROJECT)_final

install:
\t@echo "📦 تثبيت XRAY-SCOPE..."
\t@sudo cp $(DIST_DIR)/$(PROJECT)_final $(PREFIX)/bin/$(PROJECT)
\t@sudo chmod +x $(PREFIX)/bin/$(PROJECT)
\t@echo "✅ تم التثبيت! شغّل '$(PROJECT)' من أي مكان"

uninstall:
\t@echo "🗑️  إزالة XRAY-SCOPE..."
\t@sudo rm -f $(PREFIX)/bin/$(PROJECT)
\t@echo "✅ تم الإزالة"

clean:
\t@echo "🧹 تنظيف الملفات..."
\t@rm -rf $(BUILD_DIR) $(DIST_DIR)
\t@rm -f bootstrap *.o *.so $(PROJECT) $(PROJECT)_*
\t@echo "✅ تم التنظيف"

dist: all
\t@echo "📦 إنشاء حزمة التوزيع..."
\t@mkdir -p $(DIST_DIR)/package
\t@cp $(DIST_DIR)/$(PROJECT)_final $(DIST_DIR)/package/$(PROJECT)
\t@cp scripts/install.sh $(DIST_DIR)/package/
\t@cp README.md $(DIST_DIR)/package/ 2>/dev/null || echo "⚠️  README.md غير موجود"
\t@cd $(DIST_DIR)/package && tar -czf ../$(PROJECT)-$(VERSION)-linux.tar.gz *
\t@echo "✅ حزمة التوزيع: $(DIST_DIR)/$(PROJECT)-$(VERSION)-linux.tar.gz"
\t@ls -lh $(DIST_DIR)/*.tar.gz

test: all
\t@echo "🧪 اختبار XRAY-SCOPE..."
\t@$(DIST_DIR)/$(PROJECT)_final --help || echo "⚠️  فشل الاختبار"

run: all
\t@$(DIST_DIR)/$(PROJECT)_final
'''
    
    def generate_readme(self) -> str:
        """توليد ملف README.md"""
        return f'''# XRAY-SCOPE v{self.config.version} - "Omniscient Lens" '''

## 🌍 رؤية كل بايت، في كل مكان، في لحظة واحدة


