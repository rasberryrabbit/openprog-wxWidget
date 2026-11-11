/**
 * \file test_devicedb.c
 * \brief deviceRW.c 확장 API 단위 테스트.
 *
 * 빌드: gcc -D_WXGUI -DVERSION=\"test\" -DHAVE_UNISTD_H -I. \
 *       -o test_devicedb test_devicedb.c deviceRW.c progP12.c progP16.c \
 *       progP18.c progP24.c progEEPROM.c progAVR.c fileIO.c I2CSPI.c \
 *       coff.c strings.c hw_io.c device_db.c \
 *       -lm $(pkg-config --libs glib-2.0 2>/dev/null) 2>/dev/null
 *
 * 실행: ./test_devicedb
 */

#include <stdio.h>
#include <string.h>
#include "deviceRW.h"
#include "common.h"

/* hw_io.c에서 정의된 전역 변수들 - extern 으로 참조 (stub 정의 불필요) */
extern char *groupNames[NUM_GROUPS];
extern char **strings;
extern char str[4096];
extern int statusID;
extern void *status_bar;  /* GtkWidget* 이지만 void*로 캐스팅 */
extern int fd;
extern int saveLog, programID, MinDly;
extern int load_osccal, load_BKosccal;
extern int use_osccal, use_BKosccal;
extern int load_calibword, max_err;
extern int AVRlock, AVRfuse, AVRfuse_h, AVRfuse_x;
extern int ICDenable, ICDaddr;
extern int FWVersion, HwID;
extern FILE *logfile;
extern char LogFileName[512];
extern char loadfile[512], savefile[512];
extern unsigned short *memCODE_W;
extern int size, sizeW, sizeEE, sizeCONFIG, sizeUSERID;
extern unsigned char *memCODE, *memEE;
extern unsigned char memID[512], memCONFIG[48], memUSERID[8];
extern double hvreg;
extern int RWstop;
extern int useSAFLOCK_flag;
extern int forceConfig;
extern int skipV33check, waitS1, waitingS1, ee, readRes, progress;
extern int devType;
extern int vid, pid;
extern int new_vid, new_pid;
extern int old_vid, old_pid;
extern int DeviceDetected, cmdline, IOTimer;
extern char dev[64];

/* stubs - 실제 구현은 코어 .c 파일들에 있음. 테스트에서는 단순 no-op. */
void PrintMessage(const char *msg) { (void)msg; }
void PrintMessageI2C(const char *msg) { (void)msg; }
int StartHVReg(double V) { (void)V; return 1; }
void DisplayEE(void) {}

/* gui_shim.h의 inline 함수들이 호출하는 콜백 stubs */
void gui_statusbar_push(const char *msg) { (void)msg; }
int gui_events_pending(void) { return 0; }
void gui_main_iteration(void) {}
void gui_print_message(const char *msg) { (void)msg; }
void gui_msg_box(const char *msg) { (void)msg; }
void gui_refresh_data_view(void) {}

/* str, status_bar, statusID, fd는 hw_io.c에서 정의를 제공하므로
 * 테스트에서 정의하지 않음. */

/* strings_en 더미 - strings.c에서 정의됨 */
extern char *strings_en[];

/* gui 콜백 stubs (device_db.c) */
void gui_dev_list_clear(void) {}
void gui_dev_list_add(const char *n, const char *g, int i) {
    printf("  DEV: %-20s group=%-10s idx=%d\n", n, g, i);
}
void gui_dev_list_done(void) {}
void gui_show_dev_info(const char *n, int f, int e, int v, double h, const char *ft) {
    printf("  INFO: %s family=%d EE=%d V33=%d HV=%.1f feat=%s\n",
           n, f, e, v, h, ft);
}
void gui_show_avr_panel(int s) { printf("  AVR panel: %d\n", s); }
void gui_show_pic_panel(int s) { printf("  PIC panel: %d\n", s); }
void gui_show_pic18_cw(int s) { (void)s; }
void gui_show_pic16_cw(int s) { (void)s; }
void gui_show_pic12_cw(int s) { (void)s; }
void gui_show_icd_panel(int s) { (void)s; }
void gui_show_osc_panel(int s) { (void)s; }
void gui_show_configw_panel(int s) { (void)s; }
void gui_show_id_bkcal_button(int s) { (void)s; }
void gui_show_calib12_button(int s) { (void)s; }
void gui_set_dev_status(const char *m) { printf("  STATUS: %s\n", m); }

/* deviceRW.c의 nameToGroup이 PrintMessage1 호출 - stub */
/* PrintMessage1은 common.h 매크로 */

/* 테스트 콜백 */
static void enumCallback(const char *name, const char *groupName,
                         int idx, void *userdata) {
    int *count = (int *)userdata;
    (*count)++;
    printf("  [%d] %-20s (%s, idx=%d)\n", *count, name,
           groupName ? groupName : "?", idx);
}

static void aliasCallback(const char *name, void *userdata) {
    int *count = (int *)userdata;
    (*count)++;
    printf("  alias: %s\n", name);
}

static void validateCallback(const char *msg, int idx, void *userdata) {
    (void)userdata;
    printf("  VALIDATE ISSUE [idx=%d]: %s\n", idx, msg);
}

int main(void) {
    strings = strings_en;
    printf("=== deviceRW.c Extended API Test ===\n\n");

    /* 1. GetDeviceCount / GetDeviceNameCount */
    int devCount = GetDeviceCount();
    int nameCount = GetDeviceNameCount();
    printf("1. GetDeviceCount() = %d\n", devCount);
    printf("   GetDeviceNameCount() = %d\n\n", nameCount);

    /* 2. GetDeviceStats - 패밀리별 */
    int stats[NUM_FAMILIES];
    int total = GetDeviceStats(stats);
    printf("2. GetDeviceStats() total=%d:\n", total);
    const char *famNames[NUM_FAMILIES] = {
        "PIC12","PIC16","PIC18","PIC24","PIC32","AVR",
        "I2CEE","SPIEE","UWEE","OWEE","UNIOEE"
    };
    int f;
    for (f = 0; f < NUM_FAMILIES; f++) {
        if (stats[f] > 0) printf("   %-8s: %d\n", famNames[f], stats[f]);
    }
    printf("\n");

    /* 3. GetGroupStats - 그룹별 */
    int gstats[NUM_GROUPS];
    int gtotal = GetGroupStats(gstats);
    printf("3. GetGroupStats() total=%d:\n", gtotal);
    int g;
    for (g = 0; g < NUM_GROUPS; g++) {
        printf("   %-10s: %d\n", groupNames[g], gstats[g]);
    }
    printf("\n");

    /* 4. FindDeviceIndex - 특정 디바이스 검색 */
    /* DEVLIST에서는 "16F628A" 형태로 저장 (PIC 접두사 없음) */
    printf("4. FindDeviceIndex():\n");
    int idx628 = FindDeviceIndex("16F628A");
    int idx328 = FindDeviceIndex("ATmega328");
    int idxBad = FindDeviceIndex("NOTEXIST");
    printf("   16F628A -> idx=%d\n", idx628);
    printf("   ATmega328 -> idx=%d\n", idx328);
    printf("   NOTEXIST -> idx=%d\n\n", idxBad);

    /* 5. IsDeviceSupported */
    printf("5. IsDeviceSupported():\n");
    printf("   16F628A read=%d write=%d\n",
           IsDeviceSupported("16F628A", 1, 0),
           IsDeviceSupported("16F628A", 0, 1));
    printf("   ATmega328 read=%d write=%d\n",
           IsDeviceSupported("ATmega328", 1, 0),
           IsDeviceSupported("ATmega328", 0, 1));
    printf("\n");

    /* 6. GetDeviceHV / GetDeviceV33Required */
    if (idx628 >= 0) {
        printf("6. 16F628A details:\n");
        printf("   HV = %.1f\n", GetDeviceHV("16F628A"));
        printf("   V33 required = %d\n", GetDeviceV33Required("16F628A"));
        printf("   ReservedArea = %d\n\n", GetDeviceReservedArea("16F628A"));
    }

    /* 7. EnumerateDevices - PIC16 그룹, "16F628" 필터 */
    printf("7. EnumerateDevices(G_PIC_16, \"16F628\"):\n");
    int count = 0;
    EnumerateDevices(G_PIC_16, "16F628", enumCallback, &count);
    printf("   total matched: %d\n\n", count);

    /* 8. EnumerateDevices - AVR 그룹, 필터 없음 */
    printf("8. EnumerateDevices(G_ATMEL, NULL) - first 5:\n");
    count = 0;
    int printed = 0;
    /* 별도 콜백으로 처음 5개만 출력 */
    /* (단순화를 위해 전부 출력하지만 여기서는 처음 5개만 보이도록 제한) */
    extern void *_enum_first_5_only_marker;
    (void)_enum_first_5_only_marker;
    count = 0;
    EnumerateDevices(G_ATMEL, NULL, enumCallback, &count);
    printf("   total AVR devices: %d\n\n", count);

    /* 9. EnumerateDeviceAliases */
    if (idx628 >= 0) {
        printf("9. EnumerateDeviceAliases(%d):\n", idx628);
        int acount = 0;
        EnumerateDeviceAliases(idx628, aliasCallback, &acount);
        printf("   total aliases: %d\n\n", acount);
    }

    /* 10. GetDevInfo + FreeDevInfo */
    printf("10. GetDevInfo(\"16F628A\"):\n");
    struct DevInfo info = GetDevInfo("16F628A");
    if (info.device) {
        printf("   device=%s family=%d size=%d sizeEE=%d HV=%.1f V33=%d\n",
               info.device, info.family, info.size, info.sizeEE, info.HV, info.V33);
        printf("   features=%s\n", info.features ? info.features : "(null)");
        FreeDevInfo(&info);
        printf("   (after FreeDevInfo) features=%s device=%s\n",
               info.features ? "(freed)" : "(null)",
               info.device ? "(freed)" : "(null)");
    }
    printf("\n");

    /* 11. GetDeviceReadParams / GetDeviceWriteParams */
    printf("11. GetDeviceReadParams/WriteParams(\"16F628A\"):\n");
    int rp[4], wp[6];
    if (GetDeviceReadParams("16F628A", rp) == 0) {
        printf("   ReadParam: %d %d %d %d\n", rp[0], rp[1], rp[2], rp[3]);
    }
    if (GetDeviceWriteParams("16F628A", wp) == 0) {
        printf("   WriteParam: %d %d %d %d %d %d\n",
               wp[0], wp[1], wp[2], wp[3], wp[4], wp[5]);
    }
    printf("\n");

    /* 12. ValidateDeviceList */
    printf("12. ValidateDeviceList():\n");
    int issues = ValidateDeviceList(validateCallback, NULL);
    printf("   issues: %d\n\n", issues);

    /* 13. AddDevices (device_db.c) 테스트 */
    printf("13. AddDevices(G_PIC_16, \"628\"):\n");
    extern void AddDevices(enum group_t, const char *);
    AddDevices(G_PIC_16, "628");
    printf("\n");

    /* 14. ShowDeviceInfo 테스트 */
    printf("14. ShowDeviceInfo(\"16F628A\"):\n");
    extern void ShowDeviceInfo(const char *);
    ShowDeviceInfo("16F628A");
    printf("\n");

    printf("=== All tests completed ===\n");
    return 0;
}
