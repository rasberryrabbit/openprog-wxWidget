# Open Programmer GUI - wxWidgets 포팅 빌드
# Linux (테스트) 및 Windows (MinGW 크로스 컴파일) 지원

VERSION = 0.12.4-wx

# wx-config 경로 (Linux 네이티브)
WX_CONFIG_NATIVE = /home/z/my-project/tools/wx-linux/bin/wx-config

# 기본 컴파일러 플래그 (Linux 네이티브)
CFLAGS_COMMON = -DVERSION=\"$(VERSION)\" -DHAVE_UNISTD_H -D_WXGUI -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-function
CXXFLAGS_COMMON = $(CFLAGS_COMMON)

# hidapi 경로 (Windows 크로스 컴파일용)
# hidapi.h는 $(HIDAPI_DIR)/hidapi/hidapi.h 에 위치.
# #include <hidapi/hidapi.h> 형태로 include하므로 -I$(HIDAPI_DIR) 사용.
HIDAPI_DIR = /home/z/my-project/tools/hidapi-hidapi-0.15.0
HIDAPI_INCLUDE = -I$(HIDAPI_DIR)
HIDAPI_LIB = /home/z/my-project/tools/hidapi-hidapi-0.15.0/build-mingw/libhidapi.a

# 코어 C 파일 (GTK 의존성 제거됨 - icd.c는 별도 포팅)
CORE_C_FILES = \
    deviceRW.c \
    progP12.c \
    progP16.c \
    progP18.c \
    progP24.c \
    progEEPROM.c \
    progAVR.c \
    fileIO.c \
    I2CSPI.c \
    coff.c \
    strings.c \
    hw_io.c \
    device_db.c \
    icd_core.c

# C++ 파일 (wxWidgets GUI)
CXX_FILES = opgui_wx.cpp icd_wx.cpp

# 리소스는 현재 빌드에서 제외 (icons.c, resources.c - GTK 리소스 매커니즘)
# opgui.c도 제외 (대신 opgui_wx.cpp 사용)

.PHONY: all clean linux windows

all: linux

# ============================================================
# Linux 네이티브 빌드 (테스트용)
# ============================================================
linux: opgui_wx_linux

opgui_wx_linux: $(CORE_C_FILES:.c=_linux.o) opgui_wx_linux.o icd_wx_linux.o
	@echo "[LINK] $@"
	@g++ -o $@ $^ \
	    $$($(WX_CONFIG_NATIVE) --libs std,aui,adv,html,richtext,stc) \
	    $$($(WX_CONFIG_NATIVE) --cxxflags | sed 's/-I/-L/g' | head -1) \
	    -lpthread -ldl -lm -lrt 2>&1 | grep -v "warning:" || true
	@test -f $@ && echo "  -> $@ built: $$(stat -c%s $@) bytes" || echo "  FAILED"

%_linux.o: %.c
	@echo "[CC] $<"
	@gcc -c $(CFLAGS_COMMON) $< -o $@ 2>&1 | grep -E "error:" | head -3 || true
	@test -f $@ || (echo "  FAILED: $<" && false)

opgui_wx_linux.o: opgui_wx.cpp
	@echo "[CXX] $<"
	@g++ -c $(CXXFLAGS_COMMON) \
	    $$($(WX_CONFIG_NATIVE) --cxxflags) \
	    $< -o $@ 2>&1 | grep -E "error:" | head -10 || true
	@test -f $@ || (echo "  FAILED: $<" && false)

icd_wx_linux.o: icd_wx.cpp
	@echo "[CXX] $<"
	@g++ -c $(CXXFLAGS_COMMON) \
	    $$($(WX_CONFIG_NATIVE) --cxxflags) \
	    $< -o $@ 2>&1 | grep -E "error:" | head -10 || true
	@test -f $@ || (echo "  FAILED: $<" && false)

# ============================================================
# Windows 크로스 컴파일 (MinGW-w64 + wxWidgets-mingw)
# ============================================================
MINGW_PREFIX = x86_64-w64-mingw32
MINGW_CC  = $(MINGW_PREFIX)-gcc
MINGW_CXX = $(MINGW_PREFIX)-g++
MINGW_WX_CONFIG = /home/z/my-project/tools/wx-mingw/bin/wx-config

windows: opgui_wx.exe

opgui_wx.exe: $(CORE_C_FILES:.c=_mingw.o) opgui_wx_mingw.o icd_wx_mingw.o
	@echo "[LINK-WIN] $@"
	@$(MINGW_CXX) -o $@ $^ \
	    -static -static-libgcc -static-libstdc++ \
	    $(HIDAPI_LIB) \
	    $$($(MINGW_WX_CONFIG) --libs std,aui,adv,html 2>/dev/null) \
	    -lws2_32 -lwinspool -lwinmm -lshell32 -lcomctl32 -lcomdlg32 \
	    -ladvapi32 -lgdi32 -lole32 -loleaut32 -luuid -lwsock32 -lsetupapi
	@test -f $@ && echo "  -> $@ built: $$(stat -c%s $@) bytes" || echo "  FAILED"

%_mingw.o: %.c
	@echo "[CC-MINGW] $<"
	@$(MINGW_CC) -c $(CFLAGS_COMMON) -D_WIN32 -DUNICODE -D_UNICODE \
	    $(HIDAPI_INCLUDE) \
	    $$($(MINGW_WX_CONFIG) --cxxflags 2>/dev/null) \
	    $< -o $@ 2>&1 | grep -E "error:" | head -3 || true
	@test -f $@ || (echo "  FAILED: $<" && false)

opgui_wx_mingw.o: opgui_wx.cpp
	@echo "[CXX-MINGW] $<"
	@$(MINGW_CXX) -c $(CXXFLAGS_COMMON) -D_WIN32 -DUNICODE -D_UNICODE \
	    $(HIDAPI_INCLUDE) \
	    $$($(MINGW_WX_CONFIG) --cxxflags 2>/dev/null) \
	    $< -o $@ 2>&1 | grep -E "error:" | head -10 || true
	@test -f $@ || (echo "  FAILED: $<" && false)

icd_wx_mingw.o: icd_wx.cpp
	@echo "[CXX-MINGW] $<"
	@$(MINGW_CXX) -c $(CXXFLAGS_COMMON) -D_WIN32 -DUNICODE -D_UNICODE \
	    $(HIDAPI_INCLUDE) \
	    $$($(MINGW_WX_CONFIG) --cxxflags 2>/dev/null) \
	    $< -o $@ 2>&1 | grep -E "error:" | head -10 || true
	@test -f $@ || (echo "  FAILED: $<" && false)

# ============================================================
# 정리
# ============================================================
clean:
	rm -f *.o opgui_wx_linux opgui_wx.exe
