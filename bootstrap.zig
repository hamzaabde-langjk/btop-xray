//! bootstrap.zig
//! محمل ذاتي لـ XRAY-SCOPE
//! يقوم بتحميل الملف الثنائي الرئيسي إلى الذاكرة وتشغيله

const std = @import("std");
const posix = std.posix;

pub fn main() !void {
    // إعدادات الألوان للطرفية
    const blue = "\x1b[34m";
    const green = "\x1b[32m";
    const yellow = "\x1b[33m";
    const reset = "\x1b[0m";
    
    // عرض الشعار
    try std.io.getStdOut().writer().print(
        \\{s}╔══════════════════════════════════════════╗{s}
        \\{s}║  XRAY-SCOPE Bootstrap v1.0             ║{s}
        \\{s}║  تحميل النظام...                       ║{s}
        \\{s}╚══════════════════════════════════════════╝{s}
        \\
    , .{blue, reset, blue, reset, blue, reset, blue, reset});
    
    // الحصول على مسار الملف الثنائي الرئيسي
    const self_path = try std.fs.selfExePathAlloc(std.heap.page_allocator);
    defer std.heap.page_allocator.free(self_path);
    
    // البحث عن الملف الرئيسي
    const main_binary = try std.fs.cwd().openFile("xray_final", .{});
    defer main_binary.close();
    
    // قراءة الملف الثنائي إلى الذاكرة
    const stat = try main_binary.stat();
    const binary_data = try std.heap.page_allocator.alloc(u8, @as(usize, @intCast(stat.size)));
    defer std.heap.page_allocator.free(binary_data);
    
    _ = try main_binary.readAll(binary_data);
    
    // التحقق من التوقيع
    if (binary_data.len < 4) {
        @panic("ملف غير صالح");
    }
    
    // عرض معلومات التحميل
    const stdout = std.io.getStdOut().writer();
    try stdout.print("{s}📦 حجم الملف الثنائي: {d} بايت{s}\n", .{green, binary_data.len, reset});
    
    // محاكاة التحقق من سلامة الملف (حساب المجموع الاختباري)
    var checksum: u64 = 0;
    for (binary_data) |byte| {
        checksum = checksum +% byte;
    }
    try stdout.print("{s}✅ المجموع الاختباري: 0x{x}{s}\n", .{green, checksum, reset});
    
    // في التطبيق الحقيقي، هنا نقوم بتحميل الملف الثنائي وتشغيله
    // لكن Zig لا يدعم التحميل الديناميكي المباشر، لذلك نستخدم execv
    
    try stdout.print("{s}🚀 تشغيل XRAY-SCOPE...{s}\n", .{green, reset});
    
    // تنفيذ الملف الثنائي الرئيسي
    const args = [_][]const u8{ "./xray_final" };
    const env = std.process.getEnvMap(std.heap.page_allocator) catch std.process.getEnvMap(std.heap.page_allocator) catch null;
    
    // استخدام execv لتشغيل الملف
    if (env) |env_map| {
        // تحويل env_map إلى قائمة
        var env_list = std.ArrayList([]const u8).init(std.heap.page_allocator);
        defer env_list.deinit();
        
        var it = env_map.iterator();
        while (it.next()) |entry| {
            const pair = try std.fmt.allocPrint(std.heap.page_allocator, "{s}={s}", .{entry.key_ptr.*, entry.value_ptr.*});
            try env_list.append(pair);
        }
        
        const env_ptrs = try env_list.toOwnedSlice();
        defer std.heap.page_allocator.free(env_ptrs);
        
        _ = posix.execvpe("./xray_final", &args, env_ptrs);
    } else {
        _ = posix.execvpe("./xray_final", &args, std.os.environ);
    }
    
    @panic("فشل تشغيل الملف الثنائي الرئيسي");
}
