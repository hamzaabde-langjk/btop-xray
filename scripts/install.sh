#!/bin/bash
# ============================================================
# install.sh - نص التثبيت الذاتي لـ XRAY-SCOPE
# ============================================================

set -e

PROJECT="xray"
VERSION="1.0"
PREFIX="/usr/local"
COLOR_RESET="\033[0m"
COLOR_GREEN="\033[32m"
COLOR_BLUE="\033[34m"
COLOR_YELLOW="\033[33m"
COLOR_RED="\033[31m"

echo -e "${COLOR_BLUE}╔══════════════════════════════════════════╗${COLOR_RESET}"
echo -e "${COLOR_BLUE}║  XRAY-SCOPE v${VERSION} - التثبيت الذاتي     ║${COLOR_RESET}"
echo -e "${COLOR_BLUE}╚══════════════════════════════════════════╝${COLOR_RESET}"
echo ""

# التحقق من صلاحيات الجذر
if [ "$EUID" -eq 0 ]; then 
    echo -e "${COLOR_YELLOW}⚠️  جارٍ التشغيل كجذر${COLOR_RESET}"
fi

# التحقق من وجود الملف
if [ ! -f "./$PROJECT" ]; then
    echo -e "${COLOR_RED}❌ الملف $PROJECT غير موجود في الدليل الحالي${COLOR_RESET}"
    echo "يرجى التأكد من وجود الملف الثنائي"
    exit 1
fi

# التحقق من صلاحيات التنفيذ
chmod +x ./$PROJECT

# نسخ الملف
echo -e "${COLOR_GREEN}📦 نسخ $PROJECT إلى $PREFIX/bin/$PROJECT${COLOR_RESET}"
sudo cp ./$PROJECT $PREFIX/bin/$PROJECT
sudo chmod +x $PREFIX/bin/$PROJECT

# التحقق من التثبيت
if [ -f "$PREFIX/bin/$PROJECT" ]; then
    echo -e "${COLOR_GREEN}✅ تم التثبيت بنجاح!${COLOR_RESET}"
    echo ""
    echo -e "${COLOR_BLUE}🔍 استخدم الأمر التالي للتشغيل:${COLOR_RESET}"
    echo "  $PROJECT"
    echo ""
    echo -e "${COLOR_BLUE}📖 للحصول على المساعدة:${COLOR_RESET}"
    echo "  $PROJECT --help"
else
    echo -e "${COLOR_RED}❌ فشل التثبيت${COLOR_RESET}"
    exit 1
fi
