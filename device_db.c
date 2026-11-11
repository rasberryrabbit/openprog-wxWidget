/**
 * \file device_db.c
 * \brief 디바이스 DB 접근 레이어 - wxWidgets GUI와 deviceRW.c 사이의 다리.
 *
 * 원본 opgui.c의 AddDevices / FilterDevType / GetSelectedDevice / onDevSel_Changed
 * 는 GTK TreeView/ListStore에 강하게 결합되어 있어, 이들을 wxWidgets에서
 * 재사용 가능한 C 함수로 재작성. deviceRW.c의 DEVLIST[]와 populateDevInfo,
 * nameToGroup, GetDevInfo를 호출하여 실제 DB에서 데이터를 가져옴.
 */

#include "common.h"
#include "deviceRW.h"
#include <string.h>
#include <stdlib.h>
#include <strings.h>

/* strcasestr - Linux glibc extension, MinGW에는 없음 */
#ifndef __GLIBC__
char *strcasestr(const char *haystack, const char *needle);
#endif

/* ---- deviceRW.c에서 제공되는 extern ---- */
extern char *devices[];
extern int Ndevices;
extern struct DEVICES DEVLIST[];
extern int NDEVLIST;
extern char *groupNames[NUM_GROUPS];

/* dev 전역 변수 - hw_io.c에 정의됨 */
extern char dev[64];

/* ---- GUI 콜백: wxWidgets 측에서 제공 ---- */
/* 디바이스 하나를 UI 트리에 추가하도록 요청 */
extern void gui_dev_list_clear(void);
extern void gui_dev_list_add(const char *name, const char *group, int idx);
extern void gui_dev_list_done(void);
/* 선택된 디바이스에 대한 정보를 UI에 표시하도록 요청 */
extern void gui_show_dev_info(const char *name, int family, int hasEE,
                              int v33, double hv, const char *features);
/* 디바이스 패밀리에 따라 AVR/PIC 프레임 표시 토글 */
extern void gui_show_avr_panel(int show);
extern void gui_show_pic_panel(int show);
extern void gui_show_pic18_cw(int show);  /* PIC18은 CW1~CW8 모두 표시 */
extern void gui_show_pic16_cw(int show);  /* PIC16은 CW2 추가 표시 */
extern void gui_show_pic12_cw(int show);  /* PIC12는 CW1만 */
extern void gui_show_icd_panel(int show);
extern void gui_show_osc_panel(int show);
extern void gui_show_configw_panel(int show);
extern void gui_show_id_bkcal_button(int show);
extern void gui_show_calib12_button(int show);
extern void gui_set_dev_status(const char *msg);

/* ---- AddDevices 콜백 컨텍스트 ---- */
struct AddDevCtx {
    int added;
};

/* EnumerateDevices 콜백 - 각 디바이스마다 UI에 추가 */
static void addDevicesCallback(const char *name, const char *groupName,
                               int idx, void *userdata)
{
    (void)groupName;  /* deviceRW.c는 groupNames를 모르므로 NULL 전달.
                       * 본 파일에서 nameToGroup으로 그룹명 결정. */
    struct AddDevCtx *ctx = (struct AddDevCtx *)userdata;
    enum group_t g = nameToGroup(name);
    const char *gn = (g >= 0 && g < NUM_GROUPS) ? groupNames[g] : "Unknown";
    gui_dev_list_add(name, gn, idx);
    ctx->added++;
}

/* ---- 원본 AddDevices 재구현 (GTK 독립) ---- */
/* deviceRW.c의 EnumerateDevices를 사용하여 단순화. */
void AddDevices(enum group_t groupFilter, const char *textFilter)
{
    gui_dev_list_clear();
    struct AddDevCtx ctx = { 0 };

    /* deviceRW.c 확장 API 활용 */
    EnumerateDevices(groupFilter, textFilter, addDevicesCallback, &ctx);

    gui_dev_list_done();

    sprintf(str, "%d devices", ctx.added);
    gui_set_dev_status(str);
}

/* ---- 원본 GetDevInfo + onDevSel_Changed 통합 ---- */
/* 디바이스 이름을 받아서 DB에서 정보를 찾고 UI를 갱신. */
void ShowDeviceInfo(const char *devName)
{
    if (!devName || !devName[0]) {
        gui_set_dev_status("No device selected");
        return;
    }

    /* deviceRW.c 확장 API를 사용하여 디바이스 지원 여부 먼저 확인 */
    if (!IsDeviceSupported(devName, 0, 0)) {
        gui_set_dev_status("Device not found");
        return;
    }

    struct DevInfo info = GetDevInfo(devName);
    if (!info.device) {
        gui_set_dev_status("Device not found");
        return;
    }

    char feat[512];
    if (info.features) {
        strncpy(feat, info.features, sizeof(feat) - 1);
        feat[sizeof(feat) - 1] = 0;
    } else {
        feat[0] = 0;
    }

    /* 읽기/쓰기 지원 여부도 표시 */
    int canRead  = IsDeviceSupported(devName, 1, 0);
    int canWrite = IsDeviceSupported(devName, 0, 1);
    if (!canRead || !canWrite) {
        char buf[64];
        snprintf(buf, sizeof(buf), " [%s%s%s]",
                 canRead ? "" : "no read",
                 (!canRead && !canWrite) ? ", " : "",
                 canWrite ? "" : "no write");
        strncat(feat, buf, sizeof(feat) - strlen(feat) - 1);
    }

    gui_show_dev_info(devName, info.family, info.sizeEE,
                      info.V33, info.HV, feat);

    /* 패밀리별 UI 패널 토글 */
    int isAVR  = (info.family == AVR);
    int isPIC  = !isAVR && info.family != I2CEE && info.family != SPIEE &&
                 info.family != UWEE && info.family != OWEE &&
                 info.family != UNIOEE;
    gui_show_avr_panel(isAVR);
    gui_show_pic_panel(isPIC);
    gui_show_icd_panel(info.family == PIC16);
    gui_show_osc_panel(isPIC && info.family != PIC24 && info.family != PIC32);
    gui_show_configw_panel(isPIC && info.family != PIC24 && info.family != PIC32);
    gui_show_id_bkcal_button(isPIC && (info.family == PIC16 || info.family == PIC18));
    gui_show_calib12_button(isPIC && info.family == PIC12);

    if (info.family == PIC18) {
        gui_show_pic18_cw(1);
        gui_show_pic16_cw(0);
        gui_show_pic12_cw(0);
    } else if (info.family == PIC16) {
        gui_show_pic18_cw(0);
        gui_show_pic16_cw(1);
        gui_show_pic12_cw(0);
    } else if (info.family == PIC12) {
        gui_show_pic18_cw(0);
        gui_show_pic16_cw(0);
        gui_show_pic12_cw(1);
    } else {
        gui_show_pic18_cw(0);
        gui_show_pic16_cw(0);
        gui_show_pic12_cw(0);
    }

    char status[256];
    snprintf(status, sizeof(status), "%s - %s", devName, feat);
    gui_set_dev_status(status);

    /* GetDevInfo가 malloc한 device/features 메모리 해제 */
    FreeDevInfo(&info);
}

/* ---- 디바이스 읽기/쓰기 활성화 ---- */
/* 원본 DevRead/DevWrite에서 호출. deviceRW.c의 Read/Write를 호출. */
extern int checkDevForRW(void);  /* UI 측에서 제공: 디바이스 선택 여부 확인 */

int DoReadDevice(int ee, int readRes)
{
    if (!dev || !dev[0]) {
        PrintMessage("No device selected\n");
        return -1;
    }
    PrintMessage1("Reading %s...\n", dev);
    Read(dev, ee, readRes);
    PrintMessage1("Read %s complete\n", dev);
    return 0;
}

int DoWriteDevice(int ee)
{
    if (!dev || !dev[0]) {
        PrintMessage("No device selected\n");
        return -1;
    }
    PrintMessage1("Writing %s...\n", dev);
    Write(dev, ee);
    PrintMessage1("Write %s complete\n", dev);
    return 0;
}

/* ---- strcasestr (Linux에서는 gnu extension이지만 mingw에는 없음) ---- */
#ifndef __GLIBC__
char *strcasestr(const char *haystack, const char *needle) {
    if (!needle[0]) return (char *)haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
        haystack++;
    }
    return NULL;
}
#endif
