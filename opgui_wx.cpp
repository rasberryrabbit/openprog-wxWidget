/**
 * \file opgui_wx.cpp
 * \brief Open Programmer GUI - wxWidgets 포팅 메인 모듈.
 *
 * 원본 opgui.c (GTK3, 2530라인 + 231KB Glade)을 wxWidgets 3.2로 재작성.
 *
 * 포팅 범위 (현재 버전):
 *   - 메인 윈도우 (타이틀, 아이콘, 종료 핸들러)
 *   - 툴바: Open / Save / Read / Write / Stop / Connect / Info
 *   - 노트북 5개 탭: Data / Device / Options / I2C-SPI / IO
 *   - 상태바 (PrintStatus 매크로와 연동)
 *   - 디바이스 트리뷰 (필터 콤보 + 검색 엔트리)
 *   - Data 탭 텍스트 뷰 (PrintMessage와 연동)
 *
 * 미구현 (TODO):
 *   - icd.c (ICD 디버거) - GTK 의존도가 높아 별도 포팅 필요
 *   - Hex editor, IO pin manual control - 이벤트 핸들러만 stub
 *   - resources.c / icons.c (이미지 리소스) - 현재 빌드에서 제외
 *   - 명령행 모드 (-c 옵션) - 단순화된 형태로만 지원
 *
 * Copyright (C) 2009-2025 Alberto Maccioni (원작)
 * wxWidgets 포팅: 2026
 * GPL v2
 */

#include <wx/wx.h>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/toolbar.h>
#include <wx/statusbr.h>
#include <wx/msgdlg.h>
#include <wx/filedlg.h>
#include <wx/image.h>
#include <wx/artprov.h>
#include <wx/treectrl.h>
#include <wx/scrolwin.h>
#include <wx/statbox.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>
#include <wx/config.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <wx/slider.h>
#include <wx/splitter.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include "common.h"
#include "strings.h"
#include "instructions.h"
#include "I2CSPI.h"
#include "coff.h"
#include "deviceRW.h"
#include "fileIO.h"
#include "progAVR.h"
}

/* FindDevice, PacketIO, msDelay 등은 hw_io.c에서 제공됨 (C 링키지) */
extern "C" int FindDevice(int vid, int pid);
/* deviceRW.c에서 제공 */
extern "C" int GetDevType(const char *device);
/* device_db.c에서 제공 */
extern "C" void AddDevices(enum group_t groupFilter, const char *textFilter);
extern "C" void ShowDeviceInfo(const char *devName);
extern "C" void getOptions(void);
extern "C" int DoReadDevice(int ee, int readRes);
extern "C" int DoWriteDevice(int ee);
/* icd_wx.cpp에서 제공 */
extern "C" void on_icd_command_enter(wxCommandEvent &e);
extern "C" void on_icd_key(wxKeyEvent &e);
/* progAVR.c에서 제공 */
extern "C" void WriteATfuseSlow(int fuse);
/* icd_wx.cpp에서 제공 */
class wxNotebook; class wxMenuBar; class wxTimer;
extern "C" void create_icd_tab(wxWindow *parent, wxNotebook *notebook,
                                wxMenuBar *menuBar, wxTimer *timer);
/* hw_io.c에서 제공하는 코어 함수들 */
struct io_btn_state;
extern "C" double IOchangedCore(struct io_btn_state *states);
extern "C" void VPPVDDactiveCore(int vpp_on, int vdd_on);
extern "C" void DCDCactiveCore(int dcdc_on, double voltage);
extern "C" int ApplyConfigWordsCore(int devType, const char *devName,
    unsigned int cw1, unsigned int cw2, unsigned int cw3, unsigned int cw4,
    unsigned int cw5, unsigned int cw6, unsigned int cw7, int forceConfig);
extern "C" void LoadConfigFile(char *dev_out, int dev_sz,
    int *vid_out, int *pid_out, int *max_err_out,
    char *dev_ini, int dev_ini_sz,
    int *vid_ini, int *pid_ini, int *max_err_ini);
extern "C" void SaveConfigFile(const char *dev, int vid, int pid, int max_err,
    const char *dev_ini, int vid_ini, int pid_ini, int max_err_ini);
extern "C" int RunCommandLineMode(unsigned int *tmpbuf, int tmpbuf_sz);
/* I2CSPI.c에서 제공 */
extern "C" void I2CSend(int mode, int speed, int nbyte, unsigned char *buffer);
extern "C" void I2CReceive(int mode, int speed, int nbyte, unsigned char *buffer);
/* fileIO.c에서 제공 */
extern "C" void OpenLogFile(void);
extern "C" void CloseLogFile(void);
/* icd_core.c에서 제공 */
extern "C" void initVar(void);
/* forward 선언 */
class OpguiFrame;
static OpguiFrame *g_opguiFrame = nullptr;

/* ============================================================
 *   원본 opgui.c 에서 가져온 extern 선언 + 글로벌 데이터
 * ============================================================ */

extern "C" {

/* 디바이스 그룹 정의 - deviceRW.c에 정의됨 */
#define NUM_GROUPS 7
extern char *groupNames[NUM_GROUPS];
extern int devType;
extern int vid, pid;
extern int new_vid, new_pid;
extern int old_vid, old_pid;
extern int DeviceDetected;
extern int FWVersion, HwID;
extern int saveLog;
extern int use_osccal, use_BKosccal;
extern int load_osccal, load_BKosccal;
extern int load_calibword, max_err;
extern int useSAFLOCK_flag;
extern int AVRlock, AVRfuse, AVRfuse_h, AVRfuse_x;
extern int ICDenable, ICDaddr;
extern int forceConfig;
extern int RWstop;
extern char dev[64];
extern char loadfile[512], savefile[512];
extern char LogFileName[512];
extern WORD *memCODE_W;
extern int size, sizeW, sizeEE, sizeCONFIG, sizeUSERID;
extern unsigned char *memCODE, *memEE, memID[512], memCONFIG[48], memUSERID[8];
extern double hvreg;

/* status_bar, statusID, str, fd는 common.h에 extern 으로 선언되어 있고
 * hw_io.c에서 정의를 제공하므로 여기서는 정의하지 않음. */

/* 코어 파일들에서 extern 으로 참조하는 전역 변수들은 hw_io.c에서 정의됨. */

/* 원본 opgui.c에서 포팅한 함수들 - GTK 의존성 제거 */

/* MsgBox - 메시지 다이얼로그 (GUI 측에서 처리) */
void MsgBox(const char *msg) {
    gui_msg_box(msg);
}

/* DisplayEE - EEPROM 메모리 내용을 hex dump로 출력.
 * 원본 opgui.c line 1263에서 포팅. GTK 의존(g_locale_to_utf8) 부분은
 * 단순 복사로 대체. */
void DisplayEE() {
    if (!memEE || sizeEE <= 0) return;
    int i, j, max;
    int valid = 0, empty = 1, lines = 0;
    /* 동적 버퍼 할당 */
    int bufsz = (sizeEE / COL + 1) * (16 + COL * 5);
    char *aux = (char *)malloc(bufsz);
    if (!aux) return;
    aux[0] = 0;
    PrintMessage("EEPROM memory:\n");
    max = sizeEE > 7000 ? 7000 : sizeEE;
    for (i = 0; i < max; i += COL) {
        char s[256], t[64];
        s[0] = 0;
        char v[COL + 1];
        v[0] = 0;
        valid = 0;
        for (j = i; j < i + COL && j < sizeEE; j++) {
            sprintf(t, "%02X ", memEE[j]);
            strcat(s, t);
            /* ASCII 문자 (isprint이고 0xFF가 아닌 경우) */
            char c = (isprint(memEE[j]) && memEE[j] < 0xFF) ? memEE[j] : '.';
            int vlen = strlen(v);
            if (vlen < COL) { v[vlen] = c; v[vlen + 1] = 0; }
            if (memEE[j] < 0xFF) valid = 1;
        }
        if (valid) {
            sprintf(t, "%04X: %s %s\n", i, s, v);
            strcat(aux, t);
            empty = 0;
            lines++;
            if (lines > 500) {
                strcat(aux, "(...)\n");
                i = max - COL * 2;
                lines = 490;
            }
        }
    }
    if (empty) {
        PrintMessage("(empty)\n");
    } else {
        PrintMessage(aux);
        if (sizeEE > max) PrintMessage("(...)\n");
    }
    free(aux);
}

/* I2cspiR - I2C/SPI 수신.
 * 원본 opgui.c line 1169에서 포팅. GTK 위젯 읽기 → UI 멤버 변수 접근으로 대체.
 * 본 함수는 device_db.c의 DoReadDevice와 유사하게 hw_io.c의 I2CReceive 호출. */
void I2cspiR() {
    /* UI 값은 OnI2cReceive 핸들러에서 직접 처리하므로 여기서는 no-op.
     * (원본에서는 GTK 위젯에서 직접 읽었으나, wxWidgets에서는
     * OnI2cReceive 이벤트 핸들러가 동일 로직 수행) */
}

/* I2cspiS - I2C/SPI 송신.
 * 원본 opgui.c line 1199에서 포팅. 동일하게 OnI2cSend에서 처리. */
void I2cspiS() {
    /* OnI2cSend 핸들러에서 처리 */
}

/* CommandIO - 직접 USB 명령 전송.
 * 원본 opgui.c line 1229에서 포팅. OnCommandSend에서 처리. */
void CommandIO() {
    /* OnCommandSend 핸들러에서 처리 */
}

/* StartHVReg - HV 레귤레이터 시작.
 * 원본 opgui.c line 1307에서 포팅. */
int StartHVReg(double V) {
    int j = 0, z;
    int vreg = (int)(V * 10.0);
    if (saveLog && logfile) fprintf(logfile, "StartHVReg(%.2f)\n", V);
#if defined _WIN32
    DWORD t0, t;
    t = t0 = GetTickCount();
#else
    DWORD t0, t;
    t = t0 = GetTickCount();
#endif
    if (V == -1) {
        bufferU[j++] = VREG_DIS;
        bufferU[j++] = FLUSH;
        PacketIO(5);
        msDelay(40);
        return -1;
    }
    bufferU[j++] = VREG_EN;
    bufferU[j++] = SET_VPP;
    bufferU[j++] = vreg;
    bufferU[j++] = SET_PARAMETER;
    bufferU[j++] = SET_T3;
    bufferU[j++] = 2000 >> 8;
    bufferU[j++] = 2000 & 0xff;
    bufferU[j++] = WAIT_T3;
    bufferU[j++] = READ_ADC;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    msDelay(20);
    for (z = 0; z < DIMBUF - 2 && bufferI[z] != READ_ADC; z++);
    int v = (bufferI[z + 1] << 8) + bufferI[z + 2];
    if (v == 0) {
        PrintMessage("USB voltage too low (VUSB<4.5V)\n");
        return 0;
    }
    j = 0;
    bufferU[j++] = WAIT_T3;
    bufferU[j++] = READ_ADC;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    /* 안정화 대기 (최대 1.5초) */
    while ((v < (vreg / 10.0 - 1) * G || v > (vreg / 10.0 + 1) * G) && t < t0 + 1500) {
        PacketIO(5);
        msDelay(20);
        for (z = 0; z < DIMBUF - 2 && bufferI[z] != READ_ADC; z++);
        v = (bufferI[z + 1] << 8) + bufferI[z + 2];
        if (HwID == 3) v >>= 2;  /* 12-bit ADC */
        t = GetTickCount();
    }
    if (v > (vreg / 10.0 + 1) * G) {
        PrintMessage("Warning: regulator voltage too high\n\n");
        return 0;
    } else if (v < (vreg / 10.0 - 1) * G) {
        PrintMessage("Warning: regulator voltage too low\n\n");
        return 0;
    } else if (v == 0) {
        PrintMessage("USB voltage too low (VUSB<4.5V)\n");
        return 0;
    } else {
        {
            char msg[256];
            sprintf(msg, "Regulator started OK after T=%d ms VPP=%.1f\n\n",
                    (int)(t - t0), v / G);
            PrintMessage(msg);
        }
        if (saveLog && logfile) fprintf(logfile, "Regulator OK T=%d VPP=%.1f\n", (int)(t - t0), v / G);
        return vreg;
    }
}

/* CheckS1 - S1 버튼 상태 확인.
 * 원본 opgui.c line 1432에서 포팅. */
int CheckS1() {
    int i, j = 0;
    bufferU[j++] = READ_RAM;
    bufferU[j++] = 0x0F;
    if (HwID == 5) {  /* all-in-one board */
        bufferU[j++] = 0x80;  /* Read PORTA */
    } else {  /* std board */
        bufferU[j++] = 0x84;  /* READ PORTE */
    }
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    for (j = 0; j < DIMBUF - 3 && bufferI[j] != READ_RAM; j++);
    if (HwID == 5) {  /* all-in-one board */
        i = bufferI[j + 3] & 0x80;  /* A7 */
        return i ? 1 : 0;  /* A7=1 when switch pressed */
    } else {  /* std board */
        i = bufferI[j + 3] & 0x8;  /* E3 */
        return i ? 0 : 1;  /* S1 open -> E3=1 (active low) */
    }
}

/* TestHw - 하드웨어 테스트.
 * 원본 opgui.c line 1458에서 포팅. MsgBox → gui_msg_box 사용. */
void TestHw() {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    StartHVReg(13);
    int j = 0;
    MsgBox("Test hardware ...\nCheck voltages at the IC socket:");
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x0;
    bufferU[j++] = EN_VPP_VCC;  /* VDD+VPP */
    bufferU[j++] = 0x5;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    MsgBox("Test: VDDU=5V, VPPU=13V, PGD(RB5)=0V, PGC(RB6)=0V, PGM(RB7)=0V");
    j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x15;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x1;  /* VDD */
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    MsgBox("Test: VDDU=5V, VPPU=0V, PGD(RB5)=5V, PGC(RB6)=5V, PGM(RB7)=5V");
    j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x1;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x4;  /* VPP */
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    MsgBox("Test: VDDU=0V, VPPU=13V, PGD(RB5)=5V, PGC(RB6)=0V, PGM(RB7)=0V");
    j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x4;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x0;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    MsgBox("Test: VDDU=0V, VPPU=0V, PGD(RB5)=0V, PGC(RB6)=5V, PGM(RB7)=0V");
    j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x0;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x0;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);

    /* IO 테스트 (FW >= 0x900) */
    if (FWVersion >= 0x900) {
        int i, x, r;
        char str[64];
        PrintMessage("IO test\nRC|RA|--RB--|\n");
        for (i = 0; i < 13; i++) {
            x = 1 << i;
            j = 0;
            bufferU[j++] = EN_VPP_VCC;
            bufferU[j++] = 0x0;
            bufferU[j++] = SET_PORT_DIR;
            bufferU[j++] = 0x0;  /* TRISB */
            bufferU[j++] = 0x0;  /* TRISA-C */
            bufferU[j++] = EXT_PORT;
            bufferU[j++] = x & 0xFF;       /* PORTB */
            bufferU[j++] = (x >> 5) & 0xFF; /* PORTA-C */
            bufferU[j++] = READ_B;
            bufferU[j++] = READ_AC;
            bufferU[j++] = FLUSH;
            for (; j < DIMBUF; j++) bufferU[j] = 0x0;
            PacketIO(5);
            for (j = 0; j < DIMBUF - 1 && bufferI[j] != READ_B; j++);
            r = bufferI[j + 1];
            for (j += 2; j < DIMBUF - 1 && bufferI[j] != READ_AC; j++);
            r += (bufferI[j + 1] & 0xF8) << 5;
            int k;
            for (k = 0; k < 13; k++) str[12 - k] = x & (1 << k) ? '1' : '0';
            str[13] = 0;
            {
                char msg[64];
                sprintf(msg, "  %s (%s)\n", str, r == x ? "OK" : "ERR");
                PrintMessage(msg);
            }
        }
    }
}

/* ProgID - 원본 opgui.c에서 포팅.
 * 프로그래머의 펌웨어 버전과 하드웨어 ID를 읽어서 출력. */
void ProgID() {
    if (DeviceDetected != 1) return;
    int j = 0;
    bufferU[j++] = PROG_RST;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(2);
    for (j = 0; j < DIMBUF - 7 && bufferI[j] != PROG_RST; j++);
    /* 펌웨어 버전: bufferI[j+1..3] = major.minor.patch */
    FWVersion = (bufferI[j+1] << 16) + (bufferI[j+2] << 8) + bufferI[j+3];
    {
        char msg[256];
        sprintf(msg, "Firmware version %d.%d.%d\n",
                bufferI[j+1], bufferI[j+2], bufferI[j+3]);
        PrintMessage(msg);
    }
    /* 하드웨어 ID: bufferI[j+4..6] */
    HwID = bufferI[j+6];
    {
        char msg[256];
        const char *hwName = " (?)";
        if (HwID == 1) hwName = " (18F2550)";
        else if (HwID == 2) hwName = " (18F2450)";
        else if (HwID == 3) hwName = " (18F2458/2553)";
        else if (HwID == 4) hwName = " (18F25K50)";
        else if (HwID == 5) hwName = " (18F25K50 all-in-one)";
        sprintf(msg, "Hardware ID: %d.%d.%d%s\n",
                bufferI[j+4], bufferI[j+5], bufferI[j+6], hwName);
        PrintMessage(msg);
    }
}

/* FindDevice / PacketIO / msDelay / GetTickCount / htoi / OpenLogFile /
 * CloseLogFile / WriteLogIO 는 hw_io.c 와 fileIO.c 에 정의되어 있어 링크 단계에서 해결됨. */

/* glib 호환 함수 구현 */
void g_free(gpointer mem) {
    free(mem);
}

gchar *g_locale_to_utf8(const gchar *opsysstring, gssize len,
                         gsize *bytes_read, gsize *bytes_written,
                         GError **error) {
    (void)len; (void)bytes_read; (void)bytes_written; (void)error;
    /* Latin-1/UTF-8이 대부분 호환되므로 단순 복사 */
    if (!opsysstring) return NULL;
    gchar *out = (gchar *)malloc(strlen(opsysstring) + 1);
    if (out) strcpy(out, opsysstring);
    return out;
}

gchar *gui_g_build_path(const gchar *separator,
                         const gchar *first_element, ...) {
    if (!first_element) return NULL;
    va_list ap;
    va_start(ap, first_element);
    /* 최대 8개 요소까지 가정 (충분히 큰 버퍼) */
    const gchar *parts[16];
    int nparts = 0;
    parts[nparts++] = first_element;
    const gchar *p;
    while ((p = va_arg(ap, const gchar *)) != NULL && nparts < 16) {
        parts[nparts++] = p;
    }
    va_end(ap);
    size_t total = 1;
    for (int i = 0; i < nparts; i++) total += strlen(parts[i]) + 1;
    gchar *out = (gchar *)malloc(total);
    if (!out) return NULL;
    out[0] = 0;
    for (int i = 0; i < nparts; i++) {
        if (i > 0) strcat(out, separator);
        strcat(out, parts[i]);
    }
    return out;
}

const gchar *gui_g_get_home_dir(void) {
    return getenv("HOME");
}

gboolean g_file_test(const gchar *filename, int test) {
    (void)test;
    if (!filename) return 0;
    struct stat st;
    if (stat(filename, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

/* ============================================================
 *   wxWidgets 측에서 gui_shim.h에 제공하는 함수들
 * ============================================================ */

static wxTextCtrl *g_dataText = nullptr;   /* Data 탭의 로그 텍스트 */
static wxTextCtrl *g_i2cDataRecv = nullptr;  /* I2C/SPI 탭의 Receive 필드 */
static wxTextCtrl *g_cmdResp = nullptr;      /* IO 탭의 Direct USB Command 응답 */
static wxStatusBar *g_statusBar = nullptr;
static wxFrame *g_mainFrame = nullptr;

void gui_statusbar_push(const char *msg) {
    if (g_statusBar) g_statusBar->SetStatusText(wxString::FromUTF8(msg));
}
gboolean gui_events_pending(void) {
    if (wxTheApp) return wxTheApp->Pending() ? 1 : 0;
    return 0;
}
void gui_main_iteration(void) {
    if (wxTheApp) wxTheApp->Yield();
}
void gui_print_message(const char *msg) {
    if (g_dataText) {
        g_dataText->AppendText(wxString::FromUTF8(msg));
        g_dataText->AppendText("\n");
        g_dataText->ShowPosition(g_dataText->GetLastPosition());
    }
    if (wxTheApp) wxTheApp->Yield();
}
void gui_msg_box(const char *msg) {
    if (g_mainFrame) {
        wxMessageDialog dlg(g_mainFrame, wxString::FromUTF8(msg),
                            "Open Programmer", wxOK | wxICON_INFORMATION);
        dlg.ShowModal();
    }
}
void gui_refresh_data_view(void) {}

/* C 코드에서 호출되는 PrintMessage (원본 opgui.c에서 정의되지만,
 * wxWidgets 빌드에서는 gui_print_message를 사용하도록 여기서 제공) */
void PrintMessage(const char *msg) {
    gui_print_message(msg);
}
void PrintMessageI2C(const char *msg) {
    /* I2C/SPI 탭의 Receive 필드를 직접 갱신.
     * 원본 icd.c/GTK에서는 gtk_text_buffer_set_text(statusBuf, ...) 사용.
     * wxWidgets에서는 m_i2cDataRecv 텍스트 컨트롤에 설정. */
    if (g_i2cDataRecv) {
        g_i2cDataRecv->Clear();
        g_i2cDataRecv->AppendText(wxString::FromUTF8(msg));
        g_i2cDataRecv->ShowPosition(0);
    }
    if (wxTheApp) wxTheApp->Yield();
}

} /* extern "C" */

/* ============================================================
 *   디바이스 정보 (원본 instructions.h / deviceRW.c 활용)
 *   - 디바이스 목록을 채우기 위한 최소한의 코드
 * ============================================================ */

/* deviceRW.c에 정의된 함수들 */
extern "C" {
    /* 디바이스 데이터베이스 - deviceRW.c에 실제 정의 있음 */
    void loadDevices(const char *filter, int groupFilter);
    /* 위 함수는 원본 코드에 있는 것으로 추정되지만,
     * 현재 빌드에서는 stub으로 처리. */
}

/* ============================================================
 *   메인 애플리케이션 클래스
 * ============================================================ */

class OpguiApp : public wxApp {
public:
    virtual bool OnInit() override;
    virtual int OnExit() override;
};

wxIMPLEMENT_APP(OpguiApp);

/* ============================================================
 *   메인 프레임
 * ============================================================ */

class OpguiFrame : public wxFrame {
public:
    OpguiFrame();
    virtual ~OpguiFrame() {}

    /* public 접근 - getOptions/gui_* 콜백에서 사용 */
    wxTextCtrl    *m_dataText;
    wxListCtrl    *m_devTree;
    wxComboBox    *m_devTypeCombo;
    wxTextCtrl    *m_devFilterEntry;
    wxStatusBar   *m_statusBar;
    wxTextCtrl    *m_vidEntry, *m_pidEntry, *m_errEntry;
    wxTextCtrl *m_avrFuseLow, *m_avrFuseHigh, *m_avrFuseExt, *m_avrLock;
    wxCheckBox *m_avrFuseLowW, *m_avrFuseHighW, *m_avrFuseExtW, *m_avrLockW;
    wxTextCtrl *m_hexIn, *m_hexOut, *m_addrIn, *m_dataIn, *m_hexOut2;
    wxPanel *m_avrPanel;
    wxStaticText *m_devInfoLabel;
    wxTimer *m_icdTimer;
    wxMenuBar *m_menuBar;
    wxNotebook *m_notebook;  /* 노트북 (AVR 탭 추가/제거용) */
    int m_avrTabIndex;       /* AVR 탭의 인덱스 (-1 = 없음) */

    /* IO 패널 - 13개 핀 */
    wxRadioButton *m_ioR0[13];  /* 라디오: 출력 0 */
    wxRadioButton *m_ioR1[13];  /* 라디오: 출력 1 */
    wxRadioButton *m_ioRI[13];  /* 라디오: 입력 */
    wxStaticText  *m_ioLabel[13]; /* PORT 상태 표시 */
    wxCheckBox    *m_ioActive;
    wxCheckBox    *m_vppOn, *m_vddOn, *m_dcdcOn;
    wxSlider      *m_dcdcVoltage;
    wxTimer       *m_ioTimer;     /* 100ms IO 폴링 */
    /* Direct USB Command */
    wxTextCtrl    *m_cmdEntry;   /* hex 명령 입력 */
    wxTextCtrl    *m_cmdResp;    /* 응답 표시 */

    /* I2C/SPI 패널 */
    wxRadioButton *m_i2c8bit, *m_i2c16bit;
    wxRadioButton *m_spi00, *m_spi01, *m_spi10, *m_spi11;
    wxTextCtrl    *m_i2cDataSend, *m_i2cDataRecv;
    wxSpinCtrl    *m_i2cNbyte;
    wxComboBox    *m_i2cSpeed;

    /* Options 패널 - Config Words */
    wxTextCtrl    *m_cw1, *m_cw2, *m_cw3, *m_cw4, *m_cw5, *m_cw6, *m_cw7;
    wxCheckBox    *m_configForce;
    wxCheckBox    *m_icdEnable;
    wxTextCtrl    *m_icdAddrEntry;
    wxCheckBox    *m_logCheckbox;
    wxCheckBox    *m_v33Check, *m_waitS1;
    wxCheckBox    *m_eeRw, *m_resRead, *m_progId, *m_progCal12;
    wxCheckBox    *m_useSaflock, *m_useOsc, *m_useBkOsc, *m_useFileCal;

    /* 설정 파일용 초기값 백업 */
    char m_devIni[64];
    int  m_vidIni, m_pidIni, m_maxErrIni;

private:
    /* 툴바 버튼 IDs */
    enum ToolBtnId {
        ID_BTN_OPEN = 10001,
        ID_BTN_SAVE,
        ID_BTN_READ,
        ID_BTN_WRITE,
        ID_BTN_VERIFY,
        ID_BTN_STOP,
        ID_BTN_CONNECT,
        ID_BTN_INFO,
        ID_BTN_LOG,
        ID_BTN_TESTHW
    };
    /* 디바이스 패널 IDs */
    enum DevId {
        ID_DEV_TYPE_COMBO = 11001,
        ID_DEV_FILTER,
        ID_DEV_TREE,
        ID_EE_RW,
        ID_RES_READ,
        ID_PROG_ID,
        ID_PROG_CAL12,
        ID_USE_SAFLOCK,
        ID_USE_OSCCAL,
        ID_USE_BKOSCCAL,
        ID_USE_FILECAL,
        ID_VID_ENTRY,
        ID_PID_ENTRY,
        ID_ERR_ENTRY
    };
    /* IO 패널 IDs */
    enum IoId {
        ID_VPP_ON = 12001,
        ID_VDD_ON,
        ID_DCDC_ON,
        ID_DCDC_VOLT,
        ID_IO_ACTIVE,
        ID_COMMAND_SEND,
        ID_COMMAND_TRANSFER,
        ID_I2C_SEND,
        ID_I2C_RECV,
        ID_HEX_ENTRY,
        ID_ADDRESS_ENTRY,
        ID_DATA_ENTRY
    };
    /* AVR 퓨즈 IDs */
    enum AvrId {
        ID_AVR_FUSE_LOW = 13001,
        ID_AVR_FUSE_LOW_W,
        ID_AVR_FUSE_HIGH,
        ID_AVR_FUSE_HIGH_W,
        ID_AVR_FUSE_EXT,
        ID_AVR_FUSE_EXT_W,
        ID_AVR_LOCK,
        ID_AVR_LOCK_W,
        ID_AVR_WFUSE_LF,
        ID_AVR_READ_FUSES
    };
    /* Hex 도구 IDs */
    enum HexId {
        ID_HEX_CONVERT = 14001,
        ID_DATA_TO_HEX,
        ID_HEX_SAVE
    };
    /* ICD IDs (icd_wx.cpp 와 동일) */
    enum IcdId {
        ID_ICD_RUN = 13001,        /* icd_wx.cpp 와 공유 */
        ID_ICD_HALT,
        ID_ICD_STEP,
        ID_ICD_STEPOVER,
        ID_ICD_STOP,
        ID_ICD_REFRESH,
        ID_ICD_LOAD_COFF,
        ID_ICD_HELP,
        ID_ICD_CMD,
        ID_ICD_MENU_PC = 13001 + 100,
        ID_ICD_MENU_STAT,
        ID_ICD_MENU_BANK0,
        ID_ICD_MENU_BANK1,
        ID_ICD_MENU_BANK2,
        ID_ICD_MENU_BANK3,
        ID_ICD_MENU_EE
    };

    void CreateToolbar();
    void CreateDataTab(wxNotebook *notebook);
    void CreateDeviceTab(wxNotebook *notebook);
    void CreateOptionsTab(wxNotebook *notebook);
    void CreateI2cSpiTab(wxNotebook *notebook);
    void CreateIoTab(wxNotebook *notebook);
    void CreateAvrPanel(wxNotebook *notebook);
    void CreateHexToolsTab(wxNotebook *notebook);
    void CreateIcdTab(wxNotebook *notebook);

    /* AVR 패널 위젯 선언은 위쪽 public 영역으로 이동함 */
    wxPanel *m_picPanel;
    wxPanel *m_icdOptionsPanel;
    wxPanel *m_oscPanel;
    wxPanel *m_configwPanel;
    wxPanel *m_pic18CwPanel;
    wxPanel *m_pic16CwPanel;
    wxPanel *m_pic12CwPanel;
    wxWindow *m_idBkcalBtn;
    wxWindow *m_calib12Btn;
    /* AVR 패널 위젯은 위쪽 public 영역으로 이동함 */

    /* 이벤트 핸들러 */
    void OnOpen(wxCommandEvent &e);
    void OnSave(wxCommandEvent &e);
    void OnRead(wxCommandEvent &e);
    void OnWrite(wxCommandEvent &e);
    void OnVerify(wxCommandEvent &e);
    void OnStop(wxCommandEvent &e);
    void OnConnect(wxCommandEvent &e);
    void OnInfo(wxCommandEvent &e);
    void OnExit(wxCommandEvent &e);
    void OnAbout(wxCommandEvent &e);
    void OnDevTypeFilter(wxCommandEvent &e);
    void OnDevFilter(wxCommandEvent &e);
    void OnDevTreeSel(wxListEvent &e);
    void OnIoActive(wxCommandEvent &e);
    void OnAvrWfuseLF(wxCommandEvent &e);
    void OnAvrReadFuses(wxCommandEvent &e);
    void OnHexConvert(wxCommandEvent &e);
    void OnDataToHexConvert(wxCommandEvent &e);
    void OnHexSave(wxCommandEvent &e);
    void OnIcdCommand(wxCommandEvent &e);
    void OnCharHook(wxKeyEvent &e);
    void OnIoToggle(wxCommandEvent &e);       /* b_io_active 체크 */
    void OnIoTimer(wxTimerEvent &e);          /* 100ms IO 폴링 */
    void OnVppVddToggle(wxCommandEvent &e);   /* VPP/VDD 토글 */
    void OnDcdcToggle(wxCommandEvent &e);     /* DCDC 토글 */
    void OnDcdcVoltage(wxScrollEvent &e);     /* DCDC 전압 변경 */
    void OnI2cSend(wxCommandEvent &e);
    void OnI2cReceive(wxCommandEvent &e);
    void OnIoRadio(wxCommandEvent &e);        /* 13개 핀 라디오 버튼 */
    void OnCommandSend(wxCommandEvent &e);    /* Direct USB Command Send */

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(OpguiFrame, wxFrame)
    EVT_TOOL(OpguiFrame::ID_BTN_OPEN,    OpguiFrame::OnOpen)
    EVT_TOOL(OpguiFrame::ID_BTN_SAVE,    OpguiFrame::OnSave)
    EVT_TOOL(OpguiFrame::ID_BTN_READ,    OpguiFrame::OnRead)
    EVT_TOOL(OpguiFrame::ID_BTN_WRITE,   OpguiFrame::OnWrite)
    EVT_TOOL(OpguiFrame::ID_BTN_VERIFY,  OpguiFrame::OnVerify)
    EVT_TOOL(OpguiFrame::ID_BTN_STOP,    OpguiFrame::OnStop)
    EVT_TOOL(OpguiFrame::ID_BTN_CONNECT, OpguiFrame::OnConnect)
    EVT_TOOL(OpguiFrame::ID_BTN_INFO,    OpguiFrame::OnInfo)
    EVT_MENU(wxID_EXIT, OpguiFrame::OnExit)
    EVT_MENU(wxID_ABOUT, OpguiFrame::OnAbout)
    EVT_COMBOBOX(OpguiFrame::ID_DEV_TYPE_COMBO, OpguiFrame::OnDevTypeFilter)
    EVT_TEXT(OpguiFrame::ID_DEV_FILTER, OpguiFrame::OnDevFilter)
    EVT_LIST_ITEM_SELECTED(OpguiFrame::ID_DEV_TREE, OpguiFrame::OnDevTreeSel)
    EVT_BUTTON(OpguiFrame::ID_AVR_WFUSE_LF,    OpguiFrame::OnAvrWfuseLF)
    EVT_BUTTON(OpguiFrame::ID_AVR_READ_FUSES,  OpguiFrame::OnAvrReadFuses)
    EVT_BUTTON(OpguiFrame::ID_HEX_CONVERT,     OpguiFrame::OnHexConvert)
    EVT_BUTTON(OpguiFrame::ID_DATA_TO_HEX,     OpguiFrame::OnDataToHexConvert)
    EVT_BUTTON(OpguiFrame::ID_HEX_SAVE,        OpguiFrame::OnHexSave)
    EVT_TEXT_ENTER(OpguiFrame::ID_ICD_CMD,     OpguiFrame::OnIcdCommand)
    EVT_CHAR_HOOK(OpguiFrame::OnCharHook)
    EVT_CHECKBOX(OpguiFrame::ID_IO_ACTIVE,    OpguiFrame::OnIoToggle)
    EVT_CHECKBOX(OpguiFrame::ID_VPP_ON,       OpguiFrame::OnVppVddToggle)
    EVT_CHECKBOX(OpguiFrame::ID_VDD_ON,       OpguiFrame::OnVppVddToggle)
    EVT_CHECKBOX(OpguiFrame::ID_DCDC_ON,      OpguiFrame::OnDcdcToggle)
    EVT_COMMAND_SCROLL(OpguiFrame::ID_DCDC_VOLT, OpguiFrame::OnDcdcVoltage)
    EVT_BUTTON(OpguiFrame::ID_I2C_SEND,       OpguiFrame::OnI2cSend)
    EVT_BUTTON(OpguiFrame::ID_I2C_RECV,       OpguiFrame::OnI2cReceive)
    EVT_BUTTON(OpguiFrame::ID_COMMAND_SEND,   OpguiFrame::OnCommandSend)
    EVT_RADIOBUTTON(14000, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14001, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14002, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14003, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14004, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14005, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14006, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14007, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14008, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14009, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14010, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14011, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14012, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14013, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14014, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14015, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14016, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14017, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14018, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14019, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14020, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14021, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14022, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14023, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14024, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14025, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14026, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14027, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14028, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14029, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14030, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14031, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14032, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14033, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14034, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14035, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14036, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14037, OpguiFrame::OnIoRadio)
    EVT_RADIOBUTTON(14038, OpguiFrame::OnIoRadio)
wxEND_EVENT_TABLE()

/* ============================================================
 *   메인 프레임 생성
 * ============================================================ */

OpguiFrame::OpguiFrame()
    : wxFrame(NULL, wxID_ANY, "Open Programmer",
              wxDefaultPosition, wxSize(920, 640))
{
    /* 글로벌 포인터 설정 (gui_shim에서 사용) */
    g_mainFrame = this;
    g_opguiFrame = this;

    /* 아이콘 - wxArtProvider로 기본 아이콘 사용 */
    SetIcon(wxArtProvider::GetIcon(wxART_EXECUTABLE_FILE, wxART_FRAME_ICON));

    /* 메뉴바 - View 메뉴 추가 (ICD 용) */
    m_menuBar = new wxMenuBar();
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(wxID_EXIT, "E&xit\tAlt-F4");
    wxMenu *viewMenu = new wxMenu();
    wxMenu *helpMenu = new wxMenu();
    helpMenu->Append(wxID_ABOUT, "&About");
    m_menuBar->Append(fileMenu, "&File");
    m_menuBar->Append(viewMenu, "&View");
    m_menuBar->Append(helpMenu, "&Help");
    SetMenuBar(m_menuBar);

    /* 툴바 */
    CreateToolbar();

    /* ICD 타이머 생성 */
    m_icdTimer = new wxTimer(this, 9999);

    /* 노트북 */
    wxPanel *mainPanel = new wxPanel(this);
    wxBoxSizer *mainSizer = new wxBoxSizer(wxVERTICAL);
    wxNotebook *notebook = new wxNotebook(mainPanel, wxID_ANY,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxNB_TOP);

    CreateDataTab(notebook);
    CreateDeviceTab(notebook);
    CreateOptionsTab(notebook);
    CreateI2cSpiTab(notebook);
    CreateIoTab(notebook);
    CreateAvrPanel(notebook);  /* AVR 패널 생성 (탭은 동적 추가) */
    CreateHexToolsTab(notebook);
    /* ICD 탭은 icd_wx.cpp의 create_icd_tab 사용 */
    create_icd_tab(this, notebook, m_menuBar, m_icdTimer);

    /* 노트북 저장 (gui_show_avr_panel에서 사용) */
    m_notebook = notebook;

    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 4);
    mainPanel->SetSizer(mainSizer);

    /* 상태바 */
    m_statusBar = CreateStatusBar(2);
    int widths[] = { -1, 200 };
    SetStatusWidths(2, widths);
    g_statusBar = m_statusBar;

    /* 센터 배치 */
    Centre();
}

/* ============================================================
 *   툴바
 * ============================================================ */

void OpguiFrame::CreateToolbar() {
    wxToolBar *tb = CreateToolBar(wxTB_HORIZONTAL | wxTB_TEXT | wxTB_FLAT);
    tb->SetToolBitmapSize(wxSize(24, 24));

    tb->AddTool(ID_BTN_OPEN, "Open",
                wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR),
                "Open File");
    tb->AddTool(ID_BTN_SAVE, "Save",
                wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR),
                "Save File");
    tb->AddSeparator();
    tb->AddTool(ID_BTN_READ, "Read",
                wxArtProvider::GetBitmap(wxART_GO_DOWN, wxART_TOOLBAR),
                "Read device");
    tb->AddTool(ID_BTN_WRITE, "Write",
                wxArtProvider::GetBitmap(wxART_GO_UP, wxART_TOOLBAR),
                "Write device");
    tb->AddTool(ID_BTN_VERIFY, "Verify",
                wxArtProvider::GetBitmap(wxART_TICK_MARK, wxART_TOOLBAR),
                "Verify device (compare loaded file with IC)");
    tb->AddTool(ID_BTN_STOP, "Stop",
                wxArtProvider::GetBitmap(wxART_QUIT, wxART_TOOLBAR),
                "Stop");
    tb->AddSeparator();
    tb->AddTool(ID_BTN_CONNECT, "Connect",
                wxArtProvider::GetBitmap(wxART_HELP_SETTINGS, wxART_TOOLBAR),
                "Reconnect");
    tb->AddTool(ID_BTN_INFO, "Info",
                wxArtProvider::GetBitmap(wxART_INFORMATION, wxART_TOOLBAR),
                "Info");
    tb->Realize();
}

/* ============================================================
 *   Data 탭
 * ============================================================ */

void OpguiFrame::CreateDataTab(wxNotebook *notebook) {
    wxPanel *panel = new wxPanel(notebook);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

    m_dataText = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH | wxHSCROLL | wxVSCROLL);
    wxFont monoFont(wxFontInfo(10).Family(wxFONTFAMILY_MODERN));
    m_dataText->SetFont(monoFont);

    sizer->Add(m_dataText, 1, wxEXPAND | wxALL, 4);
    panel->SetSizer(sizer);

    /* 글로벌 포인터 설정 - PrintMessage가 여기로 출력 */
    g_dataText = m_dataText;

    notebook->AddPage(panel, "Data", true);
}

/* ============================================================
 *   Device 탭
 * ============================================================ */

void OpguiFrame::CreateDeviceTab(wxNotebook *notebook) {
    wxPanel *panel = new wxPanel(notebook);
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    /* 필터 행 */
    wxFlexGridSizer *filtSizer = new wxFlexGridSizer(2, 4, 4, 4);
    filtSizer->AddGrowableCol(1);

    filtSizer->Add(new wxStaticText(panel, wxID_ANY, "Filter by type:"),
                   0, wxALIGN_CENTER_VERTICAL);
    m_devTypeCombo = new wxComboBox(panel, ID_DEV_TYPE_COMBO, "",
        wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
    m_devTypeCombo->Append("All");
    m_devTypeCombo->Append("PIC10/12");
    m_devTypeCombo->Append("PIC16");
    m_devTypeCombo->Append("PIC18");
    m_devTypeCombo->Append("PIC24");
    m_devTypeCombo->Append("PIC30/33");
    m_devTypeCombo->Append("AVR");
    m_devTypeCombo->Append("EEPROM/I2C");
    m_devTypeCombo->SetSelection(0);
    filtSizer->Add(m_devTypeCombo, 1, wxEXPAND);

    filtSizer->Add(new wxStaticText(panel, wxID_ANY, "Filter:"),
                   0, wxALIGN_CENTER_VERTICAL);
    m_devFilterEntry = new wxTextCtrl(panel, ID_DEV_FILTER);
    filtSizer->Add(m_devFilterEntry, 1, wxEXPAND);

    topSizer->Add(filtSizer, 0, wxEXPAND | wxALL, 6);

    /* 디바이스 트리 (리스트 뷰로 구현) */
    m_devTree = new wxListCtrl(panel, ID_DEV_TREE,
        wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_SUNKEN);
    m_devTree->AppendColumn("Device", wxLIST_FORMAT_LEFT, 200);
    m_devTree->AppendColumn("Type",   wxLIST_FORMAT_LEFT, 140);
    m_devTree->AppendColumn("Index",  wxLIST_FORMAT_LEFT, 60);

    /* 디바이스 정보 라벨 */
    m_devInfoLabel = new wxStaticText(panel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
    topSizer->Add(m_devInfoLabel, 0, wxEXPAND | wxALL, 4);

    topSizer->Add(m_devTree, 1, wxEXPAND | wxALL, 4);

    /* 초기 로드: 모든 디바이스 표시 */
    AddDevices((enum group_t)NUM_GROUPS, "");

    /* 디바이스 옵션 행 */
    wxStaticBoxSizer *optBox = new wxStaticBoxSizer(
        wxVERTICAL, panel, "Device options");

    wxFlexGridSizer *optGrid = new wxFlexGridSizer(3, 4, 4);
    optGrid->AddGrowableCol(0); optGrid->AddGrowableCol(1); optGrid->AddGrowableCol(2);

    wxCheckBox *eeRw = new wxCheckBox(panel, ID_EE_RW,
        "Read and write EEPROM");
    wxCheckBox *resRead = new wxCheckBox(panel, ID_RES_READ,
        "Read reserved area");
    wxCheckBox *progId = new wxCheckBox(panel, ID_PROG_ID,
        "Write ID and BKOscCal");
    wxCheckBox *progCal = new wxCheckBox(panel, ID_PROG_CAL12,
        "Write Calib 1 and 2");
    wxCheckBox *useSafe = new wxCheckBox(panel, ID_USE_SAFLOCK,
        "Use SAFLOCK");
    wxCheckBox *useOsc = new wxCheckBox(panel, ID_USE_OSCCAL,
        "Use OSCCAL");
    wxCheckBox *useBkOsc = new wxCheckBox(panel, ID_USE_BKOSCCAL,
        "Use BK OSCCAL");
    wxCheckBox *useFileCal = new wxCheckBox(panel, ID_USE_FILECAL,
        "Use file cal");

    optGrid->Add(eeRw);       optGrid->Add(resRead);   optGrid->Add(progId);
    optGrid->Add(progCal);    optGrid->Add(useSafe);   optGrid->Add(useOsc);
    optGrid->Add(useBkOsc);   optGrid->Add(useFileCal);
    optBox->Add(optGrid, 1, wxEXPAND | wxALL, 4);
    topSizer->Add(optBox, 0, wxEXPAND | wxALL, 4);

    /* VID/PID/에러 행 */
    wxFlexGridSizer *connGrid = new wxFlexGridSizer(2, 6, 4, 4);
    connGrid->Add(new wxStaticText(panel, wxID_ANY, "VID:"),
                  0, wxALIGN_CENTER_VERTICAL);
    m_vidEntry = new wxTextCtrl(panel, ID_VID_ENTRY, "1209",
        wxDefaultPosition, wxSize(60, -1));
    connGrid->Add(m_vidEntry);
    connGrid->Add(new wxStaticText(panel, wxID_ANY, "PID:"),
                  0, wxALIGN_CENTER_VERTICAL);
    m_pidEntry = new wxTextCtrl(panel, ID_PID_ENTRY, "5432",
        wxDefaultPosition, wxSize(60, -1));
    connGrid->Add(m_pidEntry);
    connGrid->Add(new wxStaticText(panel, wxID_ANY, "Max err:"),
                  0, wxALIGN_CENTER_VERTICAL);
    m_errEntry = new wxTextCtrl(panel, ID_ERR_ENTRY, "200",
        wxDefaultPosition, wxSize(60, -1));
    connGrid->Add(m_errEntry);
    topSizer->Add(connGrid, 0, wxEXPAND | wxALL, 4);

    panel->SetSizer(topSizer);
    notebook->AddPage(panel, "Device");
}

/* ============================================================
 *   Options 탭 - Config Words + ICD + 기타 옵션 체크박스들
 * ============================================================ */

void OpguiFrame::CreateOptionsTab(wxNotebook *notebook) {
    wxPanel *panel = new wxPanel(notebook);
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    /* Config Words */
    wxStaticBoxSizer *box1 = new wxStaticBoxSizer(
        wxVERTICAL, panel, "Configuration Words");
    wxFlexGridSizer *grid1 = new wxFlexGridSizer(4, 4, 4);
    grid1->AddGrowableCol(1); grid1->AddGrowableCol(3);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW1:"));
    m_cw1 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw1);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW2:"));
    m_cw2 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw2);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW3:"));
    m_cw3 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw3);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW4:"));
    m_cw4 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw4);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW5:"));
    m_cw5 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw5);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW6:"));
    m_cw6 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw6);
    grid1->Add(new wxStaticText(panel, wxID_ANY, "CW7:"));
    m_cw7 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(100, -1));
    grid1->Add(m_cw7);
    box1->Add(grid1, 1, wxEXPAND | wxALL, 6);
    m_configForce = new wxCheckBox(panel, wxID_ANY,
        "Force configuration write");
    box1->Add(m_configForce, 0, wxALL, 4);
    topSizer->Add(box1, 0, wxEXPAND | wxALL, 6);

    /* PIC 옵션 체크박스들 */
    wxStaticBoxSizer *box2 = new wxStaticBoxSizer(
        wxVERTICAL, panel, "PIC Options");
    wxFlexGridSizer *gridOpt = new wxFlexGridSizer(4, 4, 4);
    m_eeRw     = new wxCheckBox(panel, ID_EE_RW,        "Read and write EEPROM");
    m_resRead  = new wxCheckBox(panel, ID_RES_READ,     "Read reserved area");
    m_progId   = new wxCheckBox(panel, ID_PROG_ID,      "Write ID and BKOscCal");
    m_progCal12= new wxCheckBox(panel, ID_PROG_CAL12,   "Write Calib 1 and 2");
    m_useSaflock = new wxCheckBox(panel, ID_USE_SAFLOCK,"Use SAFLOCK");
    m_useOsc   = new wxCheckBox(panel, ID_USE_OSCCAL,   "Use OSCCAL");
    m_useBkOsc = new wxCheckBox(panel, ID_USE_BKOSCCAL, "Use BK OSCCAL");
    m_useFileCal = new wxCheckBox(panel, ID_USE_FILECAL,"Use file cal");
    gridOpt->Add(m_eeRw);     gridOpt->Add(m_resRead);  gridOpt->Add(m_progId);   gridOpt->Add(m_progCal12);
    gridOpt->Add(m_useSaflock); gridOpt->Add(m_useOsc); gridOpt->Add(m_useBkOsc); gridOpt->Add(m_useFileCal);
    box2->Add(gridOpt, 1, wxEXPAND | wxALL, 4);
    topSizer->Add(box2, 0, wxEXPAND | wxALL, 6);

    /* ICD + 기타 */
    wxStaticBoxSizer *box3 = new wxStaticBoxSizer(
        wxVERTICAL, panel, "ICD / Misc");
    wxFlexGridSizer *grid3 = new wxFlexGridSizer(4, 6, 6);
    m_icdEnable = new wxCheckBox(panel, wxID_ANY, "Enable ICD");
    grid3->Add(m_icdEnable);
    grid3->Add(new wxStaticText(panel, wxID_ANY, "ICD addr:"));
    m_icdAddrEntry = new wxTextCtrl(panel, wxID_ANY, "1FF0",
        wxDefaultPosition, wxSize(60, -1));
    grid3->Add(m_icdAddrEntry);
    grid3->AddSpacer(0);
    m_logCheckbox = new wxCheckBox(panel, wxID_ANY, "Enable log");
    grid3->Add(m_logCheckbox);
    m_v33Check = new wxCheckBox(panel, wxID_ANY, "Skip V33 check");
    grid3->Add(m_v33Check);
    m_waitS1 = new wxCheckBox(panel, wxID_ANY, "Wait S1");
    grid3->Add(m_waitS1);
    box3->Add(grid3, 1, wxEXPAND | wxALL, 4);
    topSizer->Add(box3, 0, wxEXPAND | wxALL, 6);

    panel->SetSizer(topSizer);
    notebook->AddPage(panel, "Options");
}

/* ============================================================
 *   I2C/SPI 탭 - 송수신 활성화
 * ============================================================ */

void OpguiFrame::CreateI2cSpiTab(wxNotebook *notebook) {
    wxPanel *panel = new wxPanel(notebook);
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticBoxSizer *box = new wxStaticBoxSizer(
        wxVERTICAL, panel, "I2C/SPI Manual Control");

    /* 모드 라디오 버튼들 */
    wxStaticBoxSizer *modeBox = new wxStaticBoxSizer(
        wxHORIZONTAL, panel, "Mode");
    m_i2c8bit  = new wxRadioButton(panel, wxID_ANY, "I2C 8-bit",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_i2c16bit = new wxRadioButton(panel, wxID_ANY, "I2C 16-bit");
    m_spi00 = new wxRadioButton(panel, wxID_ANY, "SPI 0,0",
        wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
    m_spi01 = new wxRadioButton(panel, wxID_ANY, "SPI 0,1");
    m_spi10 = new wxRadioButton(panel, wxID_ANY, "SPI 1,0");
    m_spi11 = new wxRadioButton(panel, wxID_ANY, "SPI 1,1");
    modeBox->Add(m_i2c8bit, 0, wxALL, 4);
    modeBox->Add(m_i2c16bit, 0, wxALL, 4);
    modeBox->Add(m_spi00, 0, wxALL, 4);
    modeBox->Add(m_spi01, 0, wxALL, 4);
    modeBox->Add(m_spi10, 0, wxALL, 4);
    modeBox->Add(m_spi11, 0, wxALL, 4);
    box->Add(modeBox, 0, wxEXPAND | wxALL, 4);

    /* 파라미터 */
    wxFlexGridSizer *grid = new wxFlexGridSizer(2, 6, 4);
    grid->AddGrowableCol(1); grid->AddGrowableCol(3);
    grid->Add(new wxStaticText(panel, wxID_ANY, "Speed:"));
    m_i2cSpeed = new wxComboBox(panel, wxID_ANY, "100 kHz",
        wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_READONLY);
    m_i2cSpeed->Append("100 kHz");
    m_i2cSpeed->Append("400 kHz");
    m_i2cSpeed->Append("1 MHz");
    m_i2cSpeed->Append("3.4 MHz");
    grid->Add(m_i2cSpeed);
    grid->Add(new wxStaticText(panel, wxID_ANY, "N bytes:"));
    m_i2cNbyte = new wxSpinCtrl(panel, wxID_ANY, "1",
        wxDefaultPosition, wxSize(60, -1));
    m_i2cNbyte->SetRange(0, 60);
    grid->Add(m_i2cNbyte);
    box->Add(grid, 0, wxEXPAND | wxALL, 4);

    /* 송수신 데이터 - Send와 Receive 각각 별도 행으로 분리 */
    wxBoxSizer *dataBox = new wxBoxSizer(wxVERTICAL);
    /* Send 행: 라벨 + entry + Send 버튼 */
    wxBoxSizer *sendRow = new wxBoxSizer(wxHORIZONTAL);
    sendRow->Add(new wxStaticText(panel, wxID_ANY, "Send (hex):"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_i2cDataSend = new wxTextCtrl(panel, wxID_ANY, "A0 00",
        wxDefaultPosition, wxDefaultSize);
    sendRow->Add(m_i2cDataSend, 1, wxEXPAND | wxRIGHT, 6);
    sendRow->Add(new wxButton(panel, ID_I2C_SEND, "Send"), 0, 0);
    dataBox->Add(sendRow, 0, wxEXPAND | wxALL, 4);
    /* Receive 라벨 */
    dataBox->Add(new wxStaticText(panel, wxID_ANY, "Receive:"),
                 0, wxTOP | wxLEFT | wxRIGHT, 4);
    /* Receive 멀티라인 + Receive 버튼을 한 행으로 */
    wxBoxSizer *recvRow = new wxBoxSizer(wxHORIZONTAL);
    m_i2cDataRecv = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxSize(-1, 80), wxTE_READONLY | wxTE_MULTILINE | wxTE_DONTWRAP | wxHSCROLL);
    recvRow->Add(m_i2cDataRecv, 1, wxEXPAND | wxRIGHT, 6);
    recvRow->Add(new wxButton(panel, ID_I2C_RECV, "Receive"), 0, 0);
    dataBox->Add(recvRow, 1, wxEXPAND | wxALL, 4);
    box->Add(dataBox, 1, wxEXPAND | wxALL, 4);

    topSizer->Add(box, 0, wxEXPAND | wxALL, 6);
    panel->SetSizer(topSizer);
    notebook->AddPage(panel, "I2C/SPI");

    /* 글로벌 포인터 설정 - PrintMessageI2C가 이 필드를 갱신 */
    g_i2cDataRecv = m_i2cDataRecv;
}

/* ============================================================
 *   IO 탭 - 13개 핀 수동 제어 + 전원 + 직접 명령
 * ============================================================ */

void OpguiFrame::CreateIoTab(wxNotebook *notebook) {
    wxPanel *panel = new wxPanel(notebook);
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    /* 전원 + IO active */
    wxStaticBoxSizer *pwrBox = new wxStaticBoxSizer(
        wxVERTICAL, panel, "Power Control");
    wxFlexGridSizer *pwrGrid = new wxFlexGridSizer(2, 8, 4);
    m_vddOn = new wxCheckBox(panel, ID_VDD_ON, "VDD ON");
    m_vppOn = new wxCheckBox(panel, ID_VPP_ON, "VPP ON");
    m_dcdcOn = new wxCheckBox(panel, ID_DCDC_ON, "DCDC ON");
    pwrGrid->Add(m_vddOn);
    pwrGrid->Add(m_vppOn);
    pwrGrid->Add(m_dcdcOn);
    pwrGrid->Add(new wxStaticText(panel, wxID_ANY, "DCDC V:"));
    m_dcdcVoltage = new wxSlider(panel, ID_DCDC_VOLT, 120, 50, 150,
        wxDefaultPosition, wxSize(150, -1));
    pwrGrid->Add(m_dcdcVoltage);
    pwrGrid->Add(new wxStaticText(panel, wxID_ANY, "(5.0-15.0V)"));
    pwrGrid->AddSpacer(0);
    m_ioActive = new wxCheckBox(panel, ID_IO_ACTIVE, "IO active");
    pwrGrid->Add(m_ioActive);
    pwrBox->Add(pwrGrid, 0, wxEXPAND | wxALL, 4);
    topSizer->Add(pwrBox, 0, wxEXPAND | wxALL, 6);

    /* 13개 핀 라디오 버튼 그리드 */
    wxStaticBoxSizer *ioBox = new wxStaticBoxSizer(
        wxVERTICAL, panel, "IO Pins (RB0-RB7, RC7, RC6, RA5, RA4, RA3)");
    /* 헤더 */
    wxFlexGridSizer *ioGrid = new wxFlexGridSizer(5, 1, 1);
    ioGrid->Add(new wxStaticText(panel, wxID_ANY, "Pin"));
    ioGrid->Add(new wxStaticText(panel, wxID_ANY, "0"));
    ioGrid->Add(new wxStaticText(panel, wxID_ANY, "1"));
    ioGrid->Add(new wxStaticText(panel, wxID_ANY, "I"));
    ioGrid->Add(new wxStaticText(panel, wxID_ANY, "State"));
    /* 13개 핀 */
    const char *pinNames[13] = {
        "RB0", "RB1", "RB2", "RB3", "RB4", "RB5", "RB6", "RB7",
        "RC7", "RC6", "RA5", "RA4", "RA3"
    };
    /* 라디오 그룹은 각 핀마다 별도이므로 ID를 핀마다 다르게 */
    for (int i = 0; i < 13; i++) {
        ioGrid->Add(new wxStaticText(panel, wxID_ANY, pinNames[i]));
        /* 각 핀마다 라디오 그룹: 첫 번째=wxRB_GROUP */
        m_ioR0[i] = new wxRadioButton(panel, 14000 + i*3,
            "", wxDefaultPosition, wxDefaultSize, (i==0)?wxRB_GROUP:0);
        m_ioR1[i] = new wxRadioButton(panel, 14000 + i*3 + 1, "");
        m_ioRI[i] = new wxRadioButton(panel, 14000 + i*3 + 2, "");
        m_ioRI[i]->SetValue(true);  /* 기본 입력 */
        m_ioLabel[i] = new wxStaticText(panel, wxID_ANY, "?",
            wxDefaultPosition, wxSize(30, -1), wxALIGN_CENTER | wxST_NO_AUTORESIZE);
        ioGrid->Add(m_ioR0[i]);
        ioGrid->Add(m_ioR1[i]);
        ioGrid->Add(m_ioRI[i]);
        ioGrid->Add(m_ioLabel[i]);
    }
    ioBox->Add(ioGrid, 1, wxEXPAND | wxALL, 4);
    topSizer->Add(ioBox, 1, wxEXPAND | wxALL, 6);

    /* 직접 USB 명령 - 라벨 + entry + 버튼을 한 행으로, Response는 별도 행 */
    wxStaticBoxSizer *cmdBox = new wxStaticBoxSizer(
        wxVERTICAL, panel, "Direct USB Command");
    /* 첫 번째 행: "Hex (64B):" + entry + Send 버튼 (한 줄) */
    wxBoxSizer *cmdRow1 = new wxBoxSizer(wxHORIZONTAL);
    cmdRow1->Add(new wxStaticText(panel, wxID_ANY, "Hex (64B):"),
                 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    m_cmdEntry = new wxTextCtrl(panel, ID_HEX_ENTRY, "",
        wxDefaultPosition, wxDefaultSize);
    cmdRow1->Add(m_cmdEntry, 1, wxEXPAND | wxRIGHT, 6);
    cmdRow1->Add(new wxButton(panel, ID_COMMAND_SEND, "Send"), 0, 0);
    cmdBox->Add(cmdRow1, 0, wxEXPAND | wxALL, 4);
    /* 두 번째 행: "Response:" 라벨 */
    cmdBox->Add(new wxStaticText(panel, wxID_ANY, "Response:"),
                0, wxTOP | wxLEFT | wxRIGHT, 4);
    /* 세 번째 행: Response 멀티라인 (최소 높이 80px) */
    m_cmdResp = new wxTextCtrl(panel, ID_COMMAND_TRANSFER, "",
        wxDefaultPosition, wxSize(-1, 80), wxTE_READONLY | wxTE_MULTILINE | wxHSCROLL);
    cmdBox->Add(m_cmdResp, 1, wxEXPAND | wxALL, 4);
    topSizer->Add(cmdBox, 1, wxEXPAND | wxALL, 6);

    panel->SetSizer(topSizer);
    notebook->AddPage(panel, "IO");

    /* 글로벌 포인터 설정 */
    g_cmdResp = m_cmdResp;

    /* IO 타이머 생성 (아직 시작 안 함) */
    m_ioTimer = new wxTimer(this, 9998);
    Connect(9998, wxEVT_TIMER, wxTimerEventHandler(OpguiFrame::OnIoTimer), NULL, this);
}

/* ============================================================
 *   이벤트 핸들러
 * ============================================================ */

void OpguiFrame::OnOpen(wxCommandEvent &e) {
    (void)e;
    wxFileDialog dlg(this, "Open file", "", "",
        "Hex files (*.hex)|*.hex|Intel HEX (*.hex)|*.hex|"
        "Binary (*.bin)|*.bin|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        strncpy(loadfile, dlg.GetPath().mb_str(wxConvUTF8), sizeof(loadfile) - 1);
        loadfile[sizeof(loadfile) - 1] = 0;
        PrintMessage1("Loaded file: %s", loadfile);
        /* fileIO.c의 Load 호출 - devType은 deviceRW.c에서 GetDevType */
        int dt = GetDevType(dev);
        if (dt >= 0) {
            Load(dt, loadfile);
        } else {
            PrintMessage("Select a device first");
        }
    }
}

void OpguiFrame::OnSave(wxCommandEvent &e) {
    (void)e;
    wxFileDialog dlg(this, "Save file", "", "",
        "Hex files (*.hex)|*.hex|All files (*.*)|*.*",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        strncpy(savefile, dlg.GetPath().mb_str(wxConvUTF8), sizeof(savefile) - 1);
        savefile[sizeof(savefile) - 1] = 0;
        PrintMessage1("Saving file: %s", savefile);
        int dt = GetDevType(dev);
        if (dt >= 0) {
            Save(dt, savefile);
        } else {
            PrintMessage("Select a device first");
        }
    }
}

void OpguiFrame::OnRead(wxCommandEvent &e) {
    (void)e;
    if (!dev || !dev[0]) {
        wxMessageDialog dlg(this, "No device selected",
            "Read", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    /* getOptions 동기화 */
    getOptions();

    /* DoReadDevice에서 "Reading %s..." 메시지 출력하므로 여기서는 중복 출력 안 함 */
    m_statusBar->SetStatusText("Reading device...");
    RWstop = 0;

    /* 원본 DevRead와 동일 - 읽기 수행 */
    DoReadDevice(0, 0);

    m_statusBar->SetStatusText("Ready");
}

void OpguiFrame::OnWrite(wxCommandEvent &e) {
    (void)e;
    if (!dev || !dev[0]) {
        wxMessageDialog dlg(this, "No device selected",
            "Write", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    /* getOptions 동기화 */
    getOptions();

    /* DoWriteDevice에서 "Writing %s..." 메시지 출력하므로 여기서는 중복 출력 안 함 */
    m_statusBar->SetStatusText("Writing device...");
    RWstop = 0;

    /* 원본 DevWrite와 동일 - 쓰기 수행 */
    DoWriteDevice(0);

    m_statusBar->SetStatusText("Ready");
}

/* ============================================================
 *   Verify 핸들러 - 로드된 파일과 IC의 펌웨어 비교
 * ============================================================ */

void OpguiFrame::OnVerify(wxCommandEvent &e) {
    (void)e;
    if (!dev || !dev[0]) {
        wxMessageDialog dlg(this, "No device selected",
            "Verify", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    if (!DeviceDetected) {
        wxMessageDialog dlg(this, "No programmer connected",
            "Verify", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    if (!memCODE_W) {
        wxMessageDialog dlg(this, "No firmware loaded. Open a file first.",
            "Verify", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }

    /* getOptions 동기화 */
    getOptions();

    m_statusBar->SetStatusText("Verifying...");
    RWstop = 0;

    /* 디바이스 타입 확인 */
    int dt = GetDevType(dev);
    if (dt < 0) {
        PrintMessage("Cannot determine device type\n");
        m_statusBar->SetStatusText("Ready");
        return;
    }

    PrintMessage("\n--- Verify ---\n");

    /* 디바이스 정보에서 코드 영역 크기 가져오기 */
    int devIdx = FindDeviceIndex(dev);
    if (devIdx < 0) {
        PrintMessage("Device not found in database\n");
        m_statusBar->SetStatusText("Ready");
        return;
    }
    int codeSize = DEVLIST[devIdx].ReadParam[0];
    if (codeSize <= 0 || codeSize > 0x8000) codeSize = 0x800;

    /* 백업: 현재 memCODE_W를 저장 (읽기에서 덮어쓰이므로) */
    int verifySize = (sizeW > 0) ? sizeW : codeSize;
    if (verifySize > 0x8400) verifySize = 0x8400;
    unsigned short *savedCode = (unsigned short *)malloc(verifySize * sizeof(unsigned short));
    if (!savedCode) {
        PrintMessage("Memory allocation failed\n");
        m_statusBar->SetStatusText("Ready");
        return;
    }
    memcpy(savedCode, memCODE_W, verifySize * sizeof(unsigned short));

    /* IC에서 읽기 (DoReadDevice 호출) */
    PrintMessage1("Reading %s for verification...\n", dev);
    DoReadDevice(0, 0);

    /* 비교 */
    PrintMessage("\nComparing...\n");
    int mismatches = 0;
    int checked = 0;
    int firstMismatch = -1;
    int firstMismatchExpected = 0;
    int firstMismatchActual = 0;
    int i;
    for (i = 0; i < codeSize && i < verifySize; i++) {
        unsigned short expected = savedCode[i] & 0x3FFF;
        unsigned short actual = memCODE_W[i] & 0x3FFF;
        /* 0x3FFF (빈 워드)는 비교에서 제외 */
        if (expected == 0x3FFF) continue;
        checked++;
        if (expected != actual) {
            mismatches++;
            if (firstMismatch < 0) {
                firstMismatch = i;
                firstMismatchExpected = expected;
                firstMismatchActual = actual;
            }
        }
        if (RWstop) break;
    }

    /* 결과 출력 */
    {
        char msg[512];
        if (mismatches == 0) {
            sprintf(msg, "Verify PASSED: %d words checked, all match.\n", checked);
            PrintMessage(msg);
            m_statusBar->SetStatusText("Verify: PASSED");
        } else {
            sprintf(msg, "Verify FAILED: %d mismatches out of %d words checked.\n",
                    mismatches, checked);
            PrintMessage(msg);
            sprintf(msg, "  First mismatch at 0x%04X: expected 0x%04X, got 0x%04X\n",
                    firstMismatch, firstMismatchExpected, firstMismatchActual);
            PrintMessage(msg);
            m_statusBar->SetStatusText("Verify: FAILED");
        }
    }

    /* 원본 데이터 복원 (읽기로 덮어쓴 것을 되돌림) */
    memcpy(memCODE_W, savedCode, verifySize * sizeof(unsigned short));
    free(savedCode);

    PrintMessage("--- Verify End ---\n\n");
    m_statusBar->SetStatusText("Ready");
}

void OpguiFrame::OnStop(wxCommandEvent &e) {
    (void)e;
    RWstop = 1;
    PrintMessage("Stop requested");
}

void OpguiFrame::OnConnect(wxCommandEvent &e) {
    (void)e;
    PrintMessage("Connecting to programmer...");
    /* 현재 VID/PID 읽어서 시도 */
    long v = 0, p = 0;
    wxString vidStr = m_vidEntry->GetValue();
    wxString pidStr = m_pidEntry->GetValue();
    /* 빈 문자열이거나 변환 실패 시 전역 변수(vid/pid) 사용 */
    if (!vidStr.IsEmpty() && vidStr.ToLong(&v, 16) && v > 0) {
        vid = (int)v;
    } else {
        v = vid;  /* 전역 변수 사용 */
    }
    if (!pidStr.IsEmpty() && pidStr.ToLong(&p, 16) && p > 0) {
        pid = (int)p;
    } else {
        p = pid;  /* 전역 변수 사용 */
    }

    /* 1차: 사용자 입력 VID/PID */
    int found = ::FindDevice((int)v, (int)p);
    /* 2차: new_vid/new_pid (0x1209/0x5432) */
    if (!found) {
        found = ::FindDevice(new_vid, new_pid);
        if (found) { v = new_vid; p = new_pid; }
    }
    /* 3차: old_vid/old_pid (0x04D8/0x0100) */
    if (!found) {
        found = ::FindDevice(old_vid, old_pid);
        if (found) { v = old_vid; p = old_pid; }
    }

    if (found) {
        DeviceDetected = 1;
        vid = (int)v;  /* 전역 변수 업데이트 */
        pid = (int)p;
        /* UI의 VID/PID 필드도 실제 연결된 값으로 갱신 */
        char buf[16];
        sprintf(buf, "%X", (int)v);
        m_vidEntry->SetValue(buf);
        sprintf(buf, "%X", (int)p);
        m_pidEntry->SetValue(buf);
        /* saveLog가 true면 로그 파일 열기 */
        if (saveLog) {
            OpenLogFile();
        }
        {
            char msg[128];
            sprintf(msg, "Connected: VID=%04X PID=%04X", (int)v, (int)p);
            PrintMessage(msg);
        }
        /* 펌웨어 버전 및 하드웨어 ID 읽기 (원본 Connect의 ProgID 호출) */
        hvreg = 0;
        ProgID();
    } else {
        PrintMessage("No programmer found");
    }
}

void OpguiFrame::OnInfo(wxCommandEvent &e) {
    (void)e;
    char info[1024];
    sprintf(info,
        "Open Programmer GUI (wxWidgets port)\n"
        "Version: %s\n"
        "Platform: %s\n\n"
        "Original author: Alberto Maccioni\n"
        "http://openprog.altervista.org/\n\n"
        "wxWidgets port - foundation for migrating from GTK.\n"
        "Currently device read/write functions require\n"
        "additional integration with deviceRW.c",
        VERSION, SYSNAME);
    wxMessageDialog dlg(this, wxString::FromUTF8(info),
        "About Open Programmer", wxOK | wxICON_INFORMATION);
    dlg.ShowModal();
}

void OpguiFrame::OnDevTypeFilter(wxCommandEvent &e) {
    (void)e;
    int sel = m_devTypeCombo->GetSelection();
    int g = (sel == 0) ? NUM_GROUPS : (sel - 1);
    wxString filt = m_devFilterEntry->GetValue();
    AddDevices((enum group_t)g, filt.mb_str(wxConvUTF8));
}

void OpguiFrame::OnDevFilter(wxCommandEvent &e) {
    (void)e;
    int sel = m_devTypeCombo->GetSelection();
    int g = (sel == 0) ? NUM_GROUPS : (sel - 1);
    wxString filt = m_devFilterEntry->GetValue();
    AddDevices((enum group_t)g, filt.mb_str(wxConvUTF8));
}

void OpguiFrame::OnDevTreeSel(wxListEvent &e) {
    long idx = e.GetIndex();
    if (idx < 0) return;
    wxString name = m_devTree->GetItemText(idx, 0);
    strncpy(dev, name.mb_str(wxConvUTF8), sizeof(dev) - 1);
    dev[sizeof(dev) - 1] = 0;
    /* 디바이스 정보 표시 */
    ShowDeviceInfo(dev);
}

void OpguiFrame::OnIoActive(wxCommandEvent &e) {
    (void)e;
}

/* ============================================================
 *   IO 핀 제어 핸들러들
 * ============================================================ */

/* IO active 체크박스 - 100ms 타이머 시작/중지 */
void OpguiFrame::OnIoToggle(wxCommandEvent &e) {
    (void)e;
    if (m_ioActive->IsChecked() && m_ioTimer) {
        m_ioTimer->Start(100);  /* 100ms 주기 */
    } else if (m_ioTimer) {
        m_ioTimer->Stop();
    }
}

/* 100ms IO 폴링 타이머 - IOchangedCore 호출 */
void OpguiFrame::OnIoTimer(wxTimerEvent &e) {
    (void)e;
    if (!m_ioActive->IsChecked()) return;
    if (!DeviceDetected) return;

    struct io_btn_state_local { int r_0_active, r_1_active, r_I_active; char label[8]; };
    /* hw_io.c의 io_btn_state와 동일 레이아웃 */
    struct io_btn_state_local states[13];
    for (int i = 0; i < 13; i++) {
        states[i].r_0_active = m_ioR0[i]->GetValue() ? 1 : 0;
        states[i].r_1_active = m_ioR1[i]->GetValue() ? 1 : 0;
        states[i].r_I_active = m_ioRI[i]->GetValue() ? 1 : 0;
        states[i].label[0] = 0;
    }
    double vpp = IOchangedCore((struct io_btn_state *)states);

    /* 라벨 갱신 */
    for (int i = 0; i < 13; i++) {
        m_ioLabel[i]->SetLabel(wxString::FromUTF8(states[i].label));
    }
    /* 상태바에 VPP 표시 */
    char vppStr[32];
    sprintf(vppStr, "VPP=%.2fV", vpp);
    m_statusBar->SetStatusText(wxString::FromUTF8(vppStr), 1);
}

/* VPP/VDD 토글 */
void OpguiFrame::OnVppVddToggle(wxCommandEvent &e) {
    (void)e;
    if (!DeviceDetected) return;
    VPPVDDactiveCore(m_vppOn->IsChecked() ? 1 : 0,
                     m_vddOn->IsChecked() ? 1 : 0);
}

/* DCDC 토글 */
void OpguiFrame::OnDcdcToggle(wxCommandEvent &e) {
    (void)e;
    if (!DeviceDetected) return;
    double v = m_dcdcVoltage->GetValue() / 10.0;
    DCDCactiveCore(m_dcdcOn->IsChecked() ? 1 : 0, v);
}

/* DCDC 전압 슬라이더 변경 */
void OpguiFrame::OnDcdcVoltage(wxScrollEvent &e) {
    (void)e;
    if (!DeviceDetected) return;
    if (!m_dcdcOn->IsChecked()) return;
    double v = m_dcdcVoltage->GetValue() / 10.0;
    DCDCactiveCore(1, v);
}

/* 13개 핀 라디오 버튼 변경 - 즉시 한 번 IO 업데이트 */
void OpguiFrame::OnIoRadio(wxCommandEvent &e) {
    (void)e;
    if (!m_ioActive->IsChecked()) return;
    if (!DeviceDetected) return;
    /* IO 타이머가 있으면 즉시 트리거. 아니면 직접 로직 수행. */
    if (m_ioTimer && m_ioTimer->IsRunning()) {
        /* 타이머가 실행 중이면 다음 tick에서 자동 갱신됨 */
        return;
    }
    /* 타이머가 멈춰 있으면 한 번 수동 실행 */
    if (m_ioActive->IsChecked() && DeviceDetected) {
        struct io_btn_state_local { int r_0_active, r_1_active, r_I_active; char label[8]; };
        struct io_btn_state_local states[13];
        for (int i = 0; i < 13; i++) {
            states[i].r_0_active = m_ioR0[i]->GetValue() ? 1 : 0;
            states[i].r_1_active = m_ioR1[i]->GetValue() ? 1 : 0;
            states[i].r_I_active = m_ioRI[i]->GetValue() ? 1 : 0;
            states[i].label[0] = 0;
        }
        double vpp = IOchangedCore((struct io_btn_state *)states);
        for (int i = 0; i < 13; i++) {
            m_ioLabel[i]->SetLabel(wxString::FromUTF8(states[i].label));
        }
        char vppStr[32];
        sprintf(vppStr, "VPP=%.2fV", vpp);
        m_statusBar->SetStatusText(wxString::FromUTF8(vppStr), 1);
    }
}

/* ============================================================
 *   I2C/SPI 송수신 핸들러
 * ============================================================ */

/* I2CSend/I2CReceive - I2CSPI.c에서 제공 */
extern "C" void PrintMessageI2C(const char *msg);

void OpguiFrame::OnI2cSend(wxCommandEvent &e) {
    (void)e;
    if (!DeviceDetected) {
        wxMessageDialog dlg(this, "No programmer connected",
            "I2C/SPI", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    /* saveLog 동기화 */
    saveLog = m_logCheckbox->IsChecked() ? 1 : 0;

    int nbyte = m_i2cNbyte->GetValue();
    if (nbyte < 0) nbyte = 0;
    if (nbyte > 57) nbyte = 57;

    /* 모드 결정 */
    int mode = 0;
    if (m_i2c16bit->GetValue()) mode = 1;
    else if (m_spi00->GetValue()) mode = 2;
    else if (m_spi01->GetValue()) mode = 3;
    else if (m_spi10->GetValue()) mode = 4;
    else if (m_spi11->GetValue()) mode = 5;

    /* 송신 데이터 파싱 */
    wxString sendStr = m_i2cDataSend->GetValue();
    std::string s = std::string(sendStr.mb_str(wxConvUTF8));
    unsigned char tmpbuf[128];
    int i = 0;
    const char *p = s.c_str();
    while (*p && i < 128) {
        while (*p && !isxdigit((unsigned char)*p)) p++;
        if (!*p) break;
        char hex[3] = { p[0], p[1] ? p[1] : '\0', 0 };
        unsigned int v;
        if (sscanf(hex, "%x", &v) == 1) {
            tmpbuf[i++] = (unsigned char)v;
        }
        p += 2;
    }
    for (; i < 128; i++) tmpbuf[i] = 0;

    int speed = m_i2cSpeed->GetSelection();
    I2CSend(mode, speed, nbyte, tmpbuf);
}

void OpguiFrame::OnI2cReceive(wxCommandEvent &e) {
    (void)e;
    if (!DeviceDetected) {
        wxMessageDialog dlg(this, "No programmer connected",
            "I2C/SPI", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    saveLog = m_logCheckbox->IsChecked() ? 1 : 0;

    int nbyte = m_i2cNbyte->GetValue();
    if (nbyte < 0) nbyte = 0;
    if (nbyte > 60) nbyte = 60;

    int mode = 0;
    if (m_i2c16bit->GetValue()) mode = 1;
    else if (m_spi00->GetValue()) mode = 2;
    else if (m_spi01->GetValue()) mode = 3;
    else if (m_spi10->GetValue()) mode = 4;
    else if (m_spi11->GetValue()) mode = 5;

    /* I2CReceive는 송신 버퍼에서 control+addr을 읽음 */
    wxString sendStr = m_i2cDataSend->GetValue();
    std::string s = std::string(sendStr.mb_str(wxConvUTF8));
    unsigned char tmpbuf[128];
    int i = 0;
    const char *p = s.c_str();
    while (*p && i < 128) {
        while (*p && !isxdigit((unsigned char)*p)) p++;
        if (!*p) break;
        char hex[3] = { p[0], p[1] ? p[1] : '\0', 0 };
        unsigned int v;
        if (sscanf(hex, "%x", &v) == 1) {
            tmpbuf[i++] = (unsigned char)v;
        }
        p += 2;
    }
    for (; i < 128; i++) tmpbuf[i] = 0;

    int speed = m_i2cSpeed->GetSelection();
    I2CReceive(mode, speed, nbyte, tmpbuf);
    /* PrintMessageI2C가 I2C/SPI 탭의 Receive 필드를 직접 갱신함 (위 구현 참조) */
}

/* ============================================================
 *   Direct USB Command Send 핸들러
 *   원본 opgui.c의 CommandIO() 포팅
 * ============================================================ */

/* PrintMessageCMD - 원본 opgui.c에서 commandTransfer 위젯에 텍스트 설정.
 * wxWidgets에서는 m_cmdResp (g_cmdResp)에 출력. */
extern "C" void PrintMessageCMD(const char *msg) {
    if (g_cmdResp) {
        g_cmdResp->Clear();
        g_cmdResp->AppendText(wxString::FromUTF8(msg));
        g_cmdResp->ShowPosition(0);
    }
    if (wxTheApp) wxTheApp->Yield();
}

void OpguiFrame::OnCommandSend(wxCommandEvent &e) {
    (void)e;
    if (!DeviceDetected) {
        wxMessageDialog dlg(this, "No programmer connected",
            "Direct USB Command", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }
    /* saveLog 동기화 */
    saveLog = m_logCheckbox->IsChecked() ? 1 : 0;

    /* m_cmdEntry에서 hex 바이트들을 파싱하여 bufferU[0..DIMBUF-1]에 저장.
     * 원본: strtok(tokbuf, " ") + sscanf(tok, "%x", &x) */
    wxString cmdStr = m_cmdEntry->GetValue();
    std::string s = std::string(cmdStr.mb_str(wxConvUTF8));
    int i = 0, x;
    const char *p = s.c_str();
    char tokbuf[512];
    strncpy(tokbuf, p, sizeof(tokbuf) - 1);
    tokbuf[sizeof(tokbuf) - 1] = 0;

    char *tok = strtok(tokbuf, " ");
    while (tok && i < DIMBUF) {
        if (sscanf(tok, "%x", &x) == 1) {
            bufferU[i] = (unsigned char)x;
            i++;
        }
        tok = strtok(NULL, " ");
    }
    for (; i < DIMBUF; i++) bufferU[i] = 0x0;

    /* 패킷 송수신 (150ms 대기) */
    PacketIO(150);

    /* 응답 포맷팅 - 원본 CommandIO 형식:
     * >[ hex... ] \n <[ hex... ] */
    char outbuf[4096];
    int pos = 0;
    pos += sprintf(outbuf + pos, ">[");
    for (i = 0; i < DIMBUF; i++) {
        pos += sprintf(outbuf + pos, " %02X", bufferU[i]);
        if (i % 32 == 31 && i != DIMBUF - 1) {
            pos += sprintf(outbuf + pos, "\n    ");
        }
    }
    pos += sprintf(outbuf + pos, " ]\n<[");
    for (i = 0; i < DIMBUF; i++) {
        pos += sprintf(outbuf + pos, " %02X", bufferI[i]);
        if (i % 32 == 31 && i != DIMBUF - 1) {
            pos += sprintf(outbuf + pos, "\n     ");
        }
    }
    pos += sprintf(outbuf + pos, " ]");

    /* m_cmdResp에 출력 (PrintMessageCMD 호출) */
    PrintMessageCMD(outbuf);
}

/* ============================================================
 *   AVR 퓨즈 패널 - 별도 탭으로 동적 추가/제거
 * ============================================================ */

void OpguiFrame::CreateAvrPanel(wxNotebook *notebook) {
    /* 패널을 노트북의 자식으로 생성 (나중에 InsertPage/RemovePage로 관리) */
    m_avrPanel = new wxPanel(notebook);
    m_avrTabIndex = -1;  /* 아직 탭에 추가 안 함 */

    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);
    wxStaticBoxSizer *box = new wxStaticBoxSizer(
        wxVERTICAL, m_avrPanel, "AVR Fuse/Lock Bits");

    /* 퓨즈 입력 영역: 4행 (라벨 + entry + write checkbox) */
    wxFlexGridSizer *grid = new wxFlexGridSizer(3, 6, 4);
    grid->AddGrowableCol(1);

    grid->Add(new wxStaticText(m_avrPanel, wxID_ANY, "Fuse Low:"), 0, wxALIGN_CENTER_VERTICAL);
    m_avrFuseLow = new wxTextCtrl(m_avrPanel, ID_AVR_FUSE_LOW, "FF",
        wxDefaultPosition, wxSize(80, -1));
    grid->Add(m_avrFuseLow);
    m_avrFuseLowW = new wxCheckBox(m_avrPanel, ID_AVR_FUSE_LOW_W, "Write");
    grid->Add(m_avrFuseLowW);

    grid->Add(new wxStaticText(m_avrPanel, wxID_ANY, "Fuse High:"), 0, wxALIGN_CENTER_VERTICAL);
    m_avrFuseHigh = new wxTextCtrl(m_avrPanel, ID_AVR_FUSE_HIGH, "FF",
        wxDefaultPosition, wxSize(80, -1));
    grid->Add(m_avrFuseHigh);
    m_avrFuseHighW = new wxCheckBox(m_avrPanel, ID_AVR_FUSE_HIGH_W, "Write");
    grid->Add(m_avrFuseHighW);

    grid->Add(new wxStaticText(m_avrPanel, wxID_ANY, "Fuse Ext:"), 0, wxALIGN_CENTER_VERTICAL);
    m_avrFuseExt = new wxTextCtrl(m_avrPanel, ID_AVR_FUSE_EXT, "FF",
        wxDefaultPosition, wxSize(80, -1));
    grid->Add(m_avrFuseExt);
    m_avrFuseExtW = new wxCheckBox(m_avrPanel, ID_AVR_FUSE_EXT_W, "Write");
    grid->Add(m_avrFuseExtW);

    grid->Add(new wxStaticText(m_avrPanel, wxID_ANY, "Lock:"), 0, wxALIGN_CENTER_VERTICAL);
    m_avrLock = new wxTextCtrl(m_avrPanel, ID_AVR_LOCK, "FF",
        wxDefaultPosition, wxSize(80, -1));
    grid->Add(m_avrLock);
    m_avrLockW = new wxCheckBox(m_avrPanel, ID_AVR_LOCK_W, "Write");
    grid->Add(m_avrLockW);

    box->Add(grid, 0, wxEXPAND | wxALL, 6);

    /* 버튼 행: Read Fuses + Write LF @ 3kHz */
    wxBoxSizer *btnRow = new wxBoxSizer(wxHORIZONTAL);
    btnRow->Add(new wxButton(m_avrPanel, ID_AVR_READ_FUSES, "Read Fuses"), 0, wxRIGHT, 8);
    btnRow->Add(new wxButton(m_avrPanel, ID_AVR_WFUSE_LF, "Write LF @ 3kHz"), 0, 0);
    box->Add(btnRow, 0, wxALIGN_LEFT | wxALL, 6);

    topSizer->Add(box, 0, wxEXPAND | wxALL, 6);
    m_avrPanel->SetSizer(topSizer);

    /* 패널을 노트북에 페이지로 추가한 후 즉시 제거. */
    notebook->AddPage(m_avrPanel, "AVR");
    notebook->RemovePage(notebook->GetPageCount() - 1);
}

/* ============================================================
 *   Hex 도구 탭 (Intel HEX 변환기)
 * ============================================================ */

void OpguiFrame::CreateHexToolsTab(wxNotebook *notebook) {
    wxPanel *panel = new wxPanel(notebook);
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    /* HEX → Data 디코딩 */
    wxStaticBoxSizer *box1 = new wxStaticBoxSizer(
        wxVERTICAL, panel, "Intel HEX → Data (decode)");
    wxFlexGridSizer *g1 = new wxFlexGridSizer(2, 4, 4);
    g1->AddGrowableCol(1);
    g1->Add(new wxStaticText(panel, wxID_ANY, "HEX line:"), 0, wxALIGN_CENTER_VERTICAL);
    m_hexIn = new wxTextCtrl(panel, ID_HEX_ENTRY, "",
        wxDefaultPosition, wxDefaultSize);
    g1->Add(m_hexIn, 1, wxEXPAND);
    g1->Add(new wxButton(panel, ID_HEX_CONVERT, "Decode"));
    g1->AddSpacer(0);
    g1->Add(new wxStaticText(panel, wxID_ANY, "Result:"), 0, wxALIGN_CENTER_VERTICAL);
    m_hexOut = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    g1->Add(m_hexOut, 1, wxEXPAND);
    box1->Add(g1, 0, wxEXPAND | wxALL, 4);
    topSizer->Add(box1, 0, wxEXPAND | wxALL, 6);

    /* Data → HEX 인코딩 */
    wxStaticBoxSizer *box2 = new wxStaticBoxSizer(
        wxVERTICAL, panel, "Data → Intel HEX (encode)");
    wxFlexGridSizer *g2 = new wxFlexGridSizer(2, 4, 4);
    g2->AddGrowableCol(1);
    g2->Add(new wxStaticText(panel, wxID_ANY, "Address:"), 0, wxALIGN_CENTER_VERTICAL);
    m_addrIn = new wxTextCtrl(panel, ID_ADDRESS_ENTRY, "0000",
        wxDefaultPosition, wxSize(80, -1));
    g2->Add(m_addrIn);
    g2->Add(new wxStaticText(panel, wxID_ANY, "Data (hex):"), 0, wxALIGN_CENTER_VERTICAL);
    m_dataIn = new wxTextCtrl(panel, ID_DATA_ENTRY, "",
        wxDefaultPosition, wxDefaultSize);
    g2->Add(m_dataIn, 1, wxEXPAND);
    g2->Add(new wxButton(panel, ID_DATA_TO_HEX, "Encode"));
    g2->Add(new wxButton(panel, ID_HEX_SAVE, "Save..."));
    g2->Add(new wxStaticText(panel, wxID_ANY, "HEX line:"), 0, wxALIGN_CENTER_VERTICAL);
    m_hexOut2 = new wxTextCtrl(panel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
    g2->Add(m_hexOut2, 1, wxEXPAND);
    box2->Add(g2, 0, wxEXPAND | wxALL, 4);
    topSizer->Add(box2, 0, wxEXPAND | wxALL, 6);

    panel->SetSizer(topSizer);
    notebook->AddPage(panel, "Hex Tools");
}

/* ============================================================
 *   AVR Write Fuse Low @3kHz 핸들러
 * ============================================================ */

void OpguiFrame::OnAvrWfuseLF(wxCommandEvent &e) {
    (void)e;
    if (!m_avrFuseLowW->IsChecked()) {
        wxMessageDialog dlg(this, "Check 'Write' to enable fuse low writing",
            "AVR Fuse", wxOK | wxICON_INFORMATION);
        dlg.ShowModal();
        return;
    }
    /* AVRfuse 값 읽기 */
    long val = 0;
    if (!m_avrFuseLow->GetValue().ToLong(&val, 16)) {
        wxMessageDialog dlg(this, "Invalid hex value",
            "Error", wxOK | wxICON_ERROR);
        dlg.ShowModal();
        return;
    }
    AVRfuse = (int)val;
    {
        char buf[128];
        sprintf(buf, "Writing AVR fuse low @ 3kHz: 0x%02X", AVRfuse);
        PrintMessage(buf);
    }
    WriteATfuseSlow(AVRfuse);
    PrintMessage("AVR fuse low write complete");
}

/* ============================================================
 *   AVR 퓨즈 읽기 핸들러
 *   원본 progAVR.c ReadAT()의 퓨즈 읽기 부분만 추출
 * ============================================================ */

/* progAVR.c에서 제공하는 함수 */
extern "C" double SyncSPI(void);
extern "C" void AtmelID(unsigned char id[]);

void OpguiFrame::OnAvrReadFuses(wxCommandEvent &e) {
    (void)e;
    if (!DeviceDetected) {
        wxMessageDialog dlg(this, "No programmer connected",
            "Read Fuses", wxOK | wxICON_WARNING);
        dlg.ShowModal();
        return;
    }

    PrintMessage("Reading AVR fuses...\n");
    saveLog = m_logCheckbox->IsChecked() ? 1 : 0;

    /* SyncSPI로 SPI 속도 측정 */
    double Tbyte = SyncSPI();
    if (Tbyte == 0) {
        PrintMessage("Cannot sync SPI\n");
        return;
    }

    /* 퓨즈/락 읽기 패킷 구성 (원본 ReadAT에서 퓨즈 부분만) */
    int j = 0;
    unsigned char bufferU_local[128] = {0};

    /* signature + LOCK + FUSE + FUSE_H + FUSE_X 읽기 */
    bufferU_local[j++] = SPI_WRITE;  /* signature byte 0 */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x30;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_WRITE;  /* signature byte 1 */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x30;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_WRITE;  /* signature byte 2 */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x30;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = 2;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_WRITE;  /* LOCK byte */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x58;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_WRITE;  /* FUSE low byte */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x50;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_WRITE;  /* FUSE high byte */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x58;
    bufferU_local[j++] = 8;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = SPI_WRITE;  /* FUSE ext byte */
    bufferU_local[j++] = 3;
    bufferU_local[j++] = 0x50;
    bufferU_local[j++] = 8;
    bufferU_local[j++] = 0;
    bufferU_local[j++] = SPI_READ;
    bufferU_local[j++] = 1;
    bufferU_local[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU_local[j] = 0;

    /* bufferU에 복사 후 PacketIO */
    memcpy(bufferU, bufferU_local, DIMBUF);
#if defined _WIN32 || defined __CYGWIN__
    bufferU0[0] = 0;  /* report ID (Windows only) */
#endif
    PacketIO(1.5 + 7 * 4.5 * Tbyte);

    /* 응답 파싱 */
    int z;
    unsigned char sig[3] = {0};
    int lockVal = 0xFF, fuseLow = 0xFF, fuseHigh = 0xFF, fuseExt = 0xFF;

    /* signature 3바이트 */
    for (z = 0; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    sig[0] = bufferI[z + 2];
    for (z += 3; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    sig[1] = bufferI[z + 2];
    for (z += 3; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    sig[2] = bufferI[z + 2];

    /* LOCK */
    for (z += 3; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    lockVal = bufferI[z + 2];

    /* FUSE low */
    for (z += 3; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    fuseLow = bufferI[z + 2];

    /* FUSE high */
    for (z += 3; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    fuseHigh = bufferI[z + 2];

    /* FUSE ext */
    for (z += 3; z < DIMBUF - 2 && bufferI[z] != SPI_READ; z++);
    fuseExt = bufferI[z + 2];

    /* 결과 출력 */
    {
        char msg[256];
        sprintf(msg, "CHIP ID: %02X %02X %02X\n", sig[0], sig[1], sig[2]);
        PrintMessage(msg);
        AtmelID(sig);
        sprintf(msg, "Lock bits:     0x%02X\n", lockVal);
        PrintMessage(msg);
        sprintf(msg, "Fuse Low:      0x%02X\n", fuseLow);
        PrintMessage(msg);
        sprintf(msg, "Fuse High:     0x%02X\n", fuseHigh);
        PrintMessage(msg);
        sprintf(msg, "Fuse Extended: 0x%02X\n", fuseExt);
        PrintMessage(msg);
    }

    /* UI 필드에 읽은 값 표시 */
    char buf[8];
    sprintf(buf, "%02X", fuseLow);
    m_avrFuseLow->SetValue(buf);
    sprintf(buf, "%02X", fuseHigh);
    m_avrFuseHigh->SetValue(buf);
    sprintf(buf, "%02X", fuseExt);
    m_avrFuseExt->SetValue(buf);
    sprintf(buf, "%02X", lockVal);
    m_avrLock->SetValue(buf);

    /* 전역 변수도 업데이트 */
    AVRfuse = fuseLow;
    AVRfuse_h = fuseHigh;
    AVRfuse_x = fuseExt;
    AVRlock = lockVal;

    PrintMessage("Read fuses complete.\n");
}

/* ============================================================
 *   Hex 변환 도구 핸들러
 * ============================================================ */

extern "C" unsigned int htoi(const char *hex, int length);

void OpguiFrame::OnHexConvert(wxCommandEvent &e) {
    (void)e;
    /* Intel HEX 라인 파싱: ":LLAAAATT[DD...]CC" */
    wxString lineStr = m_hexIn->GetValue();
    if (lineStr.IsEmpty() || lineStr[0] != ':') {
        m_hexOut->SetValue("Invalid: must start with ':'");
        return;
    }
    std::string s = std::string(lineStr.mb_str(wxConvUTF8));
    if (s.length() < 11) {
        m_hexOut->SetValue("Invalid: too short");
        return;
    }
    unsigned int len = htoi(s.c_str() + 1, 2);
    unsigned int addr = htoi(s.c_str() + 3, 4);
    unsigned int type = htoi(s.c_str() + 7, 2);

    /* 체크섬 계산 */
    unsigned int sum = 0;
    for (size_t i = 1; i + 1 < s.length(); i += 2) {
        sum += htoi(s.c_str() + i, 2);
    }
    if ((sum & 0xFF) != 0) {
        m_hexOut->SetValue("Checksum error");
        return;
    }

    char result[1024];
    if (type == 0x00) {
        /* Data record */
        char dataBuf[512] = "";
        for (unsigned int i = 0; i < len; i++) {
            char b[8];
            sprintf(b, "%02X ", htoi(s.c_str() + 9 + i * 2, 2));
            strcat(dataBuf, b);
        }
        sprintf(result, "address: 0x%04X\ndata: %s", addr, dataBuf);
    } else if (type == 0x04) {
        /* Extended linear address */
        unsigned int ula = htoi(s.c_str() + 9, 4);
        sprintf(result, "extended linear address = %04X", ula);
    } else if (type == 0x01) {
        sprintf(result, "End of file");
    } else {
        sprintf(result, "Record type 0x%02X (addr=0x%04X, len=%u)", type, addr, len);
    }
    m_hexOut->SetValue(wxString::FromUTF8(result));
}

void OpguiFrame::OnDataToHexConvert(wxCommandEvent &e) {
    (void)e;
    /* Address + Data → Intel HEX line */
    wxString addrStr = m_addrIn->GetValue();
    wxString dataStr = m_dataIn->GetValue();
    long addr = 0;
    if (!addrStr.ToLong(&addr, 16)) {
        m_hexOut2->SetValue("Invalid address");
        return;
    }

    /* 데이터 파싱 (hex 바이트들, 공백/콤마 구분) */
    std::string ds = std::string(dataStr.mb_str(wxConvUTF8));
    unsigned char bytes[256];
    int n = 0;
    const char *p = ds.c_str();
    while (*p && n < 256) {
        while (*p && !isxdigit((unsigned char)*p)) p++;
        if (!*p) break;
        char hex[3] = { p[0], p[1] ? p[1] : '\0', 0 };
        bytes[n++] = (unsigned char)htoi(hex, 2);
        p += 2;
    }

    /* Intel HEX 라인 생성 */
    char line[1024];
    int pos = 0;
    pos += sprintf(line + pos, ":%02X%04X00", n, (unsigned int)addr & 0xFFFF);
    unsigned int sum = n + ((addr >> 8) & 0xFF) + (addr & 0xFF) + 0x00;
    for (int i = 0; i < n; i++) {
        pos += sprintf(line + pos, "%02X", bytes[i]);
        sum += bytes[i];
    }
    unsigned char checksum = (~(sum & 0xFF) + 1) & 0xFF;
    sprintf(line + pos, "%02X", checksum);
    m_hexOut2->SetValue(wxString::FromUTF8(line));
}

void OpguiFrame::OnHexSave(wxCommandEvent &e) {
    (void)e;
    wxString hexLine = m_hexOut2->GetValue();
    if (hexLine.IsEmpty()) {
        wxMessageDialog dlg(this, "Nothing to save. Encode first.",
            "Save", wxOK | wxICON_INFORMATION);
        dlg.ShowModal();
        return;
    }
    wxFileDialog dlg(this, "Save HEX file", "", "",
        "Hex files (*.hex)|*.hex|All files (*.*)|*.*",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        FILE *f = fopen(dlg.GetPath().mb_str(wxConvUTF8), "w");
        if (!f) {
            wxMessageDialog err(this, "Cannot open file",
                "Error", wxOK | wxICON_ERROR);
            err.ShowModal();
            return;
        }
        fprintf(f, "%s\n", (const char *)hexLine.mb_str(wxConvUTF8));
        fclose(f);
        PrintMessage1("Saved HEX line: %s",
            (const char *)dlg.GetPath().mb_str(wxConvUTF8));
    }
}

/* ============================================================
 *   ICD 명령 핸들러
 * ============================================================ */

void OpguiFrame::OnIcdCommand(wxCommandEvent &e) {
    (void)e;
    /* icd_wx.cpp에 정의된 핸들러 호출 */
    on_icd_command_enter(e);
}

/* F1/F5/F7/F8/F9 키 후킹 */
void OpguiFrame::OnCharHook(wxKeyEvent &e) {
    int key = e.GetKeyCode();
    if (key == WXK_F1 || key == WXK_F5 || key == WXK_F7 ||
        key == WXK_F8 || key == WXK_F9) {
        on_icd_key(e);
        return;
    }
    e.Skip();
}

/* ============================================================
 *   getOptions 구현 - device_db.c에서 호출됨
 *   UI 상태를 전역 변수로 동기화
 * ============================================================ */

/* device_db.c 콜백 함수들 */
extern "C" {

void gui_dev_list_clear(void) {
    /* 메인 스레드에서 안전하게 호출 - wxMutexGuiLeave/Guard 필요할 수 있으나
     * 현재는 단순 직접 호출 */
    if (g_opguiFrame && g_opguiFrame->m_devTree) {
        g_opguiFrame->m_devTree->DeleteAllItems();
    }
}

void gui_dev_list_add(const char *name, const char *group, int idx) {
    if (g_opguiFrame && g_opguiFrame->m_devTree) {
        long i = g_opguiFrame->m_devTree->InsertItem(g_opguiFrame->m_devTree->GetItemCount(),
            wxString::FromUTF8(name));
        g_opguiFrame->m_devTree->SetItem(i, 1, wxString::FromUTF8(group));
        char idxStr[16];
        sprintf(idxStr, "%d", idx);
        g_opguiFrame->m_devTree->SetItem(i, 2, idxStr);
    }
}

void gui_dev_list_done(void) {
    /* nothing */
}

void gui_show_dev_info(const char *name, int family, int hasEE,
                       int v33, double hv, const char *features) {
    if (g_opguiFrame && g_opguiFrame->m_devInfoLabel) {
        char info[512];
        sprintf(info, "%s | family=%d | EE=%d | V33=%d | HV=%.1f | %s",
                name, family, hasEE, v33, hv, features ? features : "");
        g_opguiFrame->m_devInfoLabel->SetLabel(wxString::FromUTF8(info));
    }
}

void gui_show_avr_panel(int show) {
    if (!g_opguiFrame || !g_opguiFrame->m_avrPanel || !g_opguiFrame->m_notebook)
        return;

    OpguiFrame *frame = g_opguiFrame;
    wxNotebook *nb = frame->m_notebook;

    if (show) {
        /* AVR 탭이 없으면 추가 */
        if (frame->m_avrTabIndex < 0) {
            /* Device 탭(인덱스 1) 다음에 삽입 */
            int insertPos = 2;  /* Data(0), Device(1), AVR(2), ... */
            nb->InsertPage(insertPos, frame->m_avrPanel, "AVR", false);
            frame->m_avrTabIndex = insertPos;
        }
    } else {
        /* AVR 탭이 있으면 제거 */
        if (frame->m_avrTabIndex >= 0) {
            nb->RemovePage(frame->m_avrTabIndex);
            frame->m_avrTabIndex = -1;
        }
    }
}

void gui_show_pic_panel(int show) { (void)show; }
void gui_show_pic18_cw(int show) { (void)show; }
void gui_show_pic16_cw(int show) { (void)show; }
void gui_show_pic12_cw(int show) { (void)show; }
void gui_show_icd_panel(int show) { (void)show; }
void gui_show_osc_panel(int show) { (void)show; }
void gui_show_configw_panel(int show) { (void)show; }
void gui_show_id_bkcal_button(int show) { (void)show; }
void gui_show_calib12_button(int show) { (void)show; }

void gui_set_dev_status(const char *msg) {
    if (g_opguiFrame && g_opguiFrame->m_statusBar) {
        g_opguiFrame->m_statusBar->SetStatusText(wxString::FromUTF8(msg));
    }
}

/* getOptions - UI 위젯에서 옵션 값 읽어 전역 변수에 저장 */
void getOptions(void) {
    if (!g_opguiFrame) return;

    /* VID/PID */
    long v = 0, p = 0, err = 0;
    g_opguiFrame->m_vidEntry->GetValue().ToLong(&v, 16);
    g_opguiFrame->m_pidEntry->GetValue().ToLong(&p, 16);
    g_opguiFrame->m_errEntry->GetValue().ToLong(&err, 10);
    vid = (int)v; pid = (int)p;
    max_err = (int)err;

    /* AVR 퓨즈 */
    long fl = 0xFF, fh = 0xFF, fx = 0xFF, lk = 0xFF;
    g_opguiFrame->m_avrFuseLow->GetValue().ToLong(&fl, 16);
    g_opguiFrame->m_avrFuseHigh->GetValue().ToLong(&fh, 16);
    g_opguiFrame->m_avrFuseExt->GetValue().ToLong(&fx, 16);
    g_opguiFrame->m_avrLock->GetValue().ToLong(&lk, 16);
    AVRfuse     = g_opguiFrame->m_avrFuseLowW->IsChecked()  ? (int)fl : 0x100;
    AVRfuse_h   = g_opguiFrame->m_avrFuseHighW->IsChecked() ? (int)fh : 0x100;
    AVRfuse_x   = g_opguiFrame->m_avrFuseExtW->IsChecked()  ? (int)fx : 0x100;
    AVRlock     = g_opguiFrame->m_avrLockW->IsChecked()     ? (int)lk : 0x100;

    /* 로깅 */
    saveLog = g_opguiFrame->m_logCheckbox->IsChecked() ? 1 : 0;

    /* V33 / S1 */
    skipV33check = g_opguiFrame->m_v33Check->IsChecked() ? 1 : 0;
    waitS1 = g_opguiFrame->m_waitS1->IsChecked() ? 1 : 0;

    /* PIC 옵션 */
    ee = g_opguiFrame->m_eeRw->IsChecked() ? 1 : 0;
    readRes = g_opguiFrame->m_resRead->IsChecked() ? 1 : 0;
    programID = g_opguiFrame->m_progId->IsChecked() ? 1 : 0;
    load_calibword = g_opguiFrame->m_progCal12->IsChecked() ? 1 : 0;
    useSAFLOCK_flag = g_opguiFrame->m_useSaflock->IsChecked() ? 1 : 0;
    use_osccal = g_opguiFrame->m_useOsc->IsChecked() ? 1 : 0;
    use_BKosccal = g_opguiFrame->m_useBkOsc->IsChecked() ? 1 : 0;
    load_osccal = g_opguiFrame->m_useFileCal->IsChecked() ? 1 : 0;

    /* ICD */
    ICDenable = g_opguiFrame->m_icdEnable->IsChecked() ? 1 : 0;
    long icdAddr = 0x1FF0;
    g_opguiFrame->m_icdAddrEntry->GetValue().ToLong(&icdAddr, 16);
    ICDaddr = (int)icdAddr;

    /* Config Words - getOptions에서 ApplyConfigWordsCore 호출 */
    long cw1 = 0x10000, cw2 = 0x10000, cw3 = 0x10000, cw4 = 0x10000;
    long cw5 = 0x10000, cw6 = 0x10000, cw7 = 0x10000;
    if (!g_opguiFrame->m_cw1->GetValue().IsEmpty())
        g_opguiFrame->m_cw1->GetValue().ToLong(&cw1, 16);
    if (!g_opguiFrame->m_cw2->GetValue().IsEmpty())
        g_opguiFrame->m_cw2->GetValue().ToLong(&cw2, 16);
    if (!g_opguiFrame->m_cw3->GetValue().IsEmpty())
        g_opguiFrame->m_cw3->GetValue().ToLong(&cw3, 16);
    if (!g_opguiFrame->m_cw4->GetValue().IsEmpty())
        g_opguiFrame->m_cw4->GetValue().ToLong(&cw4, 16);
    if (!g_opguiFrame->m_cw5->GetValue().IsEmpty())
        g_opguiFrame->m_cw5->GetValue().ToLong(&cw5, 16);
    if (!g_opguiFrame->m_cw6->GetValue().IsEmpty())
        g_opguiFrame->m_cw6->GetValue().ToLong(&cw6, 16);
    if (!g_opguiFrame->m_cw7->GetValue().IsEmpty())
        g_opguiFrame->m_cw7->GetValue().ToLong(&cw7, 16);

    int forceCfg = g_opguiFrame->m_configForce->IsChecked() ? 1 : 0;
    forceConfig = forceCfg;

    /* devType은 deviceRW.c의 GetDevType에서 가져옴 */
    int dt = GetDevType(dev);
    if (dt >= 0) devType = dt;

    ApplyConfigWordsCore(devType, dev,
        (unsigned int)cw1, (unsigned int)cw2, (unsigned int)cw3, (unsigned int)cw4,
        (unsigned int)cw5, (unsigned int)cw6, (unsigned int)cw7, forceCfg);
}

} /* extern "C" */


void OpguiFrame::OnExit(wxCommandEvent &e) {
    (void)e;
    Close(true);
}

void OpguiFrame::OnAbout(wxCommandEvent &e) {
    (void)e;
    OnInfo(e);
}

/* ============================================================
 *   앱 초기화
 * ============================================================ */

bool OpguiApp::OnInit() {
    if (!wxApp::OnInit()) return false;

    /* 언어 초기화 (영어 기본; -l it 옵션으로 이탈리아어 가능) */
    strinit();
    strings = strings_en;
    /* TODO: --lang it 옵션 처리 시 strings = strings_it; */

    /* 이미지 핸들러 초기화 */
    wxInitAllImageHandlers();

    /* 명령행 옵션 파싱: -c / --command → 명령행 모드 (GUI 없이) */
    /* wxWidgets의 argc/argv 사용 */
    int argc_wx = argc;
    bool cmdMode = false;
    int optind = 1;
    for (int i = 1; i < argc_wx; i++) {
        wxString arg = argv[i];
        if (arg == "-c" || arg == "--command" || arg == "--c") {
            cmdMode = true;
            optind = i + 1;
            break;
        }
        if (arg == "-h" || arg == "--help" || arg == "-?") {
            printf("Open Programmer v%s (wxWidgets port)\n", VERSION);
            printf("Usage: opgui_wx [options] [hex bytes...]\n");
            printf("Options:\n");
            printf("  -h, --help     Show this help\n");
            printf("  -c, --command  Command-line mode (no GUI)\n");
            printf("  --version      Show version\n");
            return false;
        }
        if (arg == "--version") {
            printf("opgui_wx %s\n", VERSION);
            return false;
        }
    }

    if (cmdMode) {
        /* 명령행 모드: GUI 없이 USB 패킷 송수신 */
        strinit();
        strings = strings_en;

        /* 설정 파일에서 vid/pid/max_err 불러오기 */
        char devBuf[64] = "";
        char devIni[64] = "";
        int vidIni, pidIni, maxErrIni;
        LoadConfigFile(devBuf, sizeof(devBuf), &vid, &pid, &max_err,
                       devIni, sizeof(devIni), &vidIni, &pidIni, &maxErrIni);

        /* argv의 나머지 인자들을 16진수로 파싱 */
        unsigned int tmpbuf[128];
        int tmpbuf_sz = 128;
        int j = 0;
        for (int i = optind; i < argc_wx && j < tmpbuf_sz; i++, j++) {
            sscanf(argv[i].mb_str(wxConvUTF8), "%x", &tmpbuf[j]);
        }
        for (; j < tmpbuf_sz; j++) tmpbuf[j] = 0;

        RunCommandLineMode(tmpbuf, tmpbuf_sz);
        return false;  /* GUI 진입 안 함 */
    }

    /* 프레임 생성 */
    OpguiFrame *frame = new OpguiFrame();
    frame->Show(true);

    /* 원본 main()에서 수행하는 중요한 초기화 - 누락되어 있던 부분 */
    /* 1. memCODE_W 메모리 할당 (원본: sizeW=0x8400, malloc) */
    sizeW = 0x8400;
    memCODE_W = (WORD *)malloc(sizeW * sizeof(WORD));
    if (memCODE_W) {
        for (int i = 0; i < 0x8400; i++) memCODE_W[i] = 0x3fff;
    }
    /* 2. 로그 파일명 설정 (원본: strings[S_LogFile] = "Log.txt") */
    strncpy(LogFileName, "Log.txt", sizeof(LogFileName) - 1);
    LogFileName[sizeof(LogFileName) - 1] = 0;
    /* 3. ICD 변수 초기화 */
    initVar();

    /* 설정 파일에서 device/vid/pid/max_err 로드 */
    LoadConfigFile(frame->m_devIni, sizeof(frame->m_devIni),
                   &frame->m_vidIni, &frame->m_pidIni, &frame->m_maxErrIni,
                   frame->m_devIni, sizeof(frame->m_devIni),
                   &frame->m_vidIni, &frame->m_pidIni, &frame->m_maxErrIni);
    /* UI에 반영 */
    if (dev[0]) {
        /* 디바이스 이름은 사용자가 직접 선택하도록 함 */
    }
    char vidStr[16], pidStr[16], errStr[16];
    sprintf(vidStr, "%X", frame->m_vidIni);
    sprintf(pidStr, "%X", frame->m_pidIni);
    sprintf(errStr, "%d", frame->m_maxErrIni);
    frame->m_vidEntry->SetValue(vidStr);
    frame->m_pidEntry->SetValue(pidStr);
    frame->m_errEntry->SetValue(errStr);

    /* 환영 메시지 */
    char welcome[512];
    sprintf(welcome,
        "Open Programmer v%s (wxWidgets port)\n"
        "Platform: %s\n"
        "Ready.", VERSION, SYSNAME);
    PrintMessage(welcome);

    return true;
}

int OpguiApp::OnExit() {
    /* 로그 파일 닫기 */
    CloseLogFile();
    /* memCODE_W 메모리 해제 */
    if (memCODE_W) {
        free(memCODE_W);
        memCODE_W = NULL;
    }
    /* 설정 파일 저장 (변경이 있을 때만) */
    OpguiFrame *frame = (OpguiFrame *)GetTopWindow();
    if (frame) {
        SaveConfigFile(dev, vid, pid, max_err,
                       frame->m_devIni, frame->m_vidIni,
                       frame->m_pidIni, frame->m_maxErrIni);
    }
    return wxApp::OnExit();
}

/* ============================================================
 *   main - 명령행 옵션 처리 후 wxApp 진입
 * ============================================================ */

extern "C" {
    /* strings.c에서 제공 - 언어 초기화 */
    void strinit(void);
    extern char **strings;
    extern char *strings_en[];
}

/* main 은 wxIMPLEMENT_APP 매크로가 생성함.
 * OnInit() 에서 strinit() 및 strings 설정 수행. */
