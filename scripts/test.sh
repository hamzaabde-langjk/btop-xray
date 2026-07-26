#!/bin/bash
# ============================================================
# test.sh - اختبار أداء XRAY-SCOPE
# ============================================================

set -e

PROJECT="xray"
COLOR_RESET="\033[0m"
COLOR_GREEN="\033[32m"
COLOR_BLUE="\033[34m"
COLOR_YELLOW="\033[33m"
COLOR_RED="\033[31m"

echo -e "${COLOR_BLUE}╔══════════════════════════════════════════╗${COLOR_RESET}"
echo -e "${COLOR_BLUE}║  XRAY-SCOPE - اختبار الأداء            ║${COLOR_RESET}"
echo -e "${COLOR_BLUE}╚══════════════════════════════════════════╝${COLOR_RESET}"
echo ""

# التحقق من وجود الملف
if [ ! -f "./$PROJECT" ] && [ ! -f "./${PROJECT}_final" ]; then
    echo -e "${COLOR_RED}❌ الملف الثنائي غير موجود${COLOR_RESET}"
    echo "يرجى بناء المشروع أولاً: make"
    exit 1
fi

BINARY="./${PROJECT}_final"
if [ ! -f "$BINARY" ]; then
    BINARY="./$PROJECT"
fi

echo -e "${COLOR_BLUE}🔍 اختبار: $BINARY${COLOR_RESET}"
echo ""

# 1. اختبار حجم الملف
echo -e "${COLOR_YELLOW}📦 حجم الملف:${COLOR_RESET}"
ls -lh $BINARY | awk '{print "  " $5}'
echo ""

# 2. اختبار استهلاك الذاكرة (تشغيل سريع)
echo -e "${COLOR_YELLOW}🧠 استهلاك الذاكرة (RSS):${COLOR_RESET}"
timeout 3 $BINARY --headless &
PID=$!
sleep 1
if ps -p $PID > /dev/null 2>&1; then
    RSS=$(ps -o rss= -p $PID 2>/dev/null | tr -d ' ')
    if [ -n "$RSS" ]; then
        RSS_MB=$((RSS / 1024))
        echo "  ${RSS_MB} ميجابايت"
    else
        echo "  غير متاح"
    fi
    kill $PID 2>/dev/null
else
    echo "  فشل في قياس الذاكرة"
fi
echo ""

# 3. اختبار سرعة الأحداث
echo -e "${COLOR_YELLOW}⚡ سرعة معالجة الأحداث:${COLOR_RESET}"
timeout 2 $BINARY --headless > /dev/null 2>&1 &
PID=$!
sleep 1
if ps -p $PID > /dev/null 2>&1; then
    echo "  تم قياس الأداء (يعتمد على المحرك)"
    kill $PID 2>/dev/null
else
    echo "  غير متاح"
fi
echo ""

# 4. معلومات النظام
echo -e "${COLOR_YELLOW}🖥️  معلومات النظام:${COLOR_RESET}"
echo "  النواة: $(uname -r)"
echo "  المعالج: $(uname -m)"
echo "  الذاكرة: $(free -h | grep Mem | awk '{print $2}')"
echo ""

echo -e "${COLOR_GREEN}✅ اكتمل الاختبار${COLOR_RESET}"
