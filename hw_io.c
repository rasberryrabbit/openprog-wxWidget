/**
 * \file hw_io.c
 * \brief OPGUI wxWidgets 포팅용 핵심 USB I/O 함수들.
 *
 * 원본 opgui.c에서 GTK 의존도가 없는(또는 거의 없는) 핵심 함수들만 추출:
 *   - FindDevice() : USB HID 장치 검색
 *   - PacketIO()   : 패킷 송수신
 *   - msDelay()    : millisecond 대기
 *   - GetTickCount(): 시스템 시간 (Linux)
 *
 * opgui.c에서는 이 함수들도 GTK 전역변수를 사용하지만, 본 파일에서는
 * gui_shim.h의 PrintMessage를 통해 GUI 의존성을 분리함.
 */

#include "common.h"
#include "I2CSPI.h"
#include "coff.h"
#include "icd.h"
#include "deviceRW.h"
#include "fileIO.h"
#include "progAVR.h"
#include <string.h>

/* Windows에서 hidapi 사용하여 USB HID 장치 접근 */
#if defined _WIN32 || defined __CYGWIN__
    #include <hidapi/hidapi.h>
#endif

/* Linux에서 hidraw를 사용하므로 path는 빌드 시점에 결정 */
#if !defined _WIN32 && !defined __CYGWIN__
    #ifndef hiddevIO
        /* use raw USB device (hidraw) */
    #endif
#endif

/* ----- 전역 변수 (원본 opgui.c에서 선언된 것들, common.h에 extern) ----- */
extern char path[512];
extern int vid, pid;
extern int new_vid, new_pid;
extern int old_vid, old_pid;
extern int DeviceDetected;
extern int FWVersion, HwID;
extern int saveLog;
extern int cmdline;
extern int skipV33check;
extern int IOTimer;
extern int programID;
extern int RWstop;

char path[512] = "";
int vid = 0x1209, pid = 0x5432;
int new_vid = 0x1209, new_pid = 0x5432;
int old_vid = 0x04D8, old_pid = 0x0100;
int DeviceDetected = 0;
int FWVersion = 0, HwID = 0;
int saveLog = 0;
int cmdline = 0;
int IOTimer = 0;
int programID = 0;
int RWstop = 0;

/* common.h에 extern 으로 선언된 변수들 - hw_io.c에서 정의 제공.
 * opgui_wx.cpp에서 정의하던 것을 이전하여 테스트 단독 실행 가능. */
char str[4096] = "";
int statusID = 0;
GtkWidget *status_bar = NULL;
int fd = 0;

/* common.h에 extern 으로 선언되었지만 opgui.c 원본에 정의된 변수들.
 * opgui.c를 빌드에서 제외했으므로 여기서 정의. */
int MinDly = 0;
int load_osccal = 0, load_BKosccal = 0;
int use_osccal = 1, use_BKosccal = 0;
int load_calibword = 0, max_err = 200;
int AVRlock = 0x100, AVRfuse = 0x100, AVRfuse_h = 0x100, AVRfuse_x = 0x100;
int ICDenable = 0, ICDaddr = 0x1FF0;
FILE *logfile = NULL;
char LogFileName[512] = "";
char loadfile[512] = "", savefile[512] = "";
WORD *memCODE_W = NULL;
int size = 0, sizeW = 0, sizeEE = 0, sizeCONFIG = 0, sizeUSERID = 0;
unsigned char *memCODE = NULL, *memEE = NULL;
unsigned char memID[512], memCONFIG[48], memUSERID[8];
double hvreg = 0;
int useSAFLOCK_flag = 0;
int forceConfig = 0;
/* opgui.c 원본에 정의된 추가 변수들 */
int skipV33check = 0;
int waitS1 = 0, waitingS1 = 0;
int ee = 0;
int readRes = 0;
int progress = 0;
int devType = -1;

/* strings 포인터 - strings.c에서 strings_it, strings_en 정의됨.
 * opgui_wx.cpp의 OnInit에서 strings_en 으로 설정. */
char **strings = NULL;

/* CheckV33Regulator - 원본 opgui.c에서 포팅 (GTK 독립).
 * 3.3V 레귤레이터 존재 여부를 B1 핀 상태로 검사. */
int CheckV33Regulator(void) {
    int i, j = 0;
    if (skipV33check) return 1;
    bufferU[j++] = WRITE_RAM;  bufferU[j++] = 0x0F; bufferU[j++] = 0x93;
    bufferU[j++] = 0xFE;  /* B0 = output */
    bufferU[j++] = EXT_PORT;  bufferU[j++] = 0x01; bufferU[j++] = 0;  /* B0=1 */
    bufferU[j++] = READ_RAM;  bufferU[j++] = 0x0F; bufferU[j++] = 0x81;  /* Check if B1=1 */
    bufferU[j++] = EXT_PORT;  bufferU[j++] = 0x00; bufferU[j++] = 0;  /* B0=0 */
    bufferU[j++] = READ_RAM;  bufferU[j++] = 0x0F; bufferU[j++] = 0x81;  /* Check if B1=0 */
    bufferU[j++] = WRITE_RAM; bufferU[j++] = 0x0F; bufferU[j++] = 0x93;
    bufferU[j++] = 0xFF;  /* BX = input */
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(5);
    for (j = 0; j < DIMBUF - 3 && bufferI[j] != READ_RAM; j++);
    i = bufferI[j + 3] & 0x2;  /* B1 should be high */
    for (j += 3; j < DIMBUF - 3 && bufferI[j] != READ_RAM; j++);
    return (i + (bufferI[j + 3] & 0x2)) == 2 ? 1 : 0;
}

/* ====================================================================
 * IO 핀 제어 - 원본 opgui.c IOchanged/IOactive/VPPVDDactive/DCDCactive
 * 의 코어 로직 (GTK 독립). UI 값은 인자로 받거나 전역 변수에서 읽음.
 * ==================================================================== */

/* ioButtons 상태를 위한 간단 구조체 - wxWidgets 측에서 채움 */
struct io_btn_state {
    int r_0_active;   /* 0 = 출력 0 */
    int r_1_active;   /* 1 = 출력 1 */
    int r_I_active;   /* 2 = 입력 */
    char label[8];    /* PORT 입력 상태 표시용 라벨 ("0"/"1"/"?") */
};

/* IOchanged 코어 로직: 13개 핀 상태를 받아 패킷 송수신.
 * states: 13개 핀의 라디오 버튼 상태 (입력으로 사용)
 * 반환: VPP 전압 (double). -1이면 에러/b_io_active 비활성.
 * 응답으로 states[].label 갱신. */
double IOchangedCore(struct io_btn_state *states)
{
    int i;
    int trisa = 1, trisb = 0, trisc = 0x30;
    int latac = 0, latb = 0;
    int port_a = 0, port_b = 0, port_c = 0;
    int raw_adc = 0;
    double vpp = 0;

    /* TRIS/LAT 레지스터 계산 */
    for (i = 0; i < 8; i++) {  /* RB0-RB7 */
        if (states[i].r_I_active) trisb |= (1 << i);
        if (states[i].r_1_active) latb |= (1 << i);
    }
    /* RC7 (idx 8) */
    if (states[8].r_I_active) trisc |= 0x80;
    if (states[8].r_1_active) latac |= 0x80;
    /* RC6 (idx 9) */
    if (states[9].r_I_active) trisc |= 0x40;
    if (states[9].r_1_active) latac |= 0x40;
    /* RA5 (idx 10) */
    if (states[10].r_I_active) trisa |= 0x20;
    if (states[10].r_1_active) latac |= 0x20;
    /* RA4 (idx 11) */
    if (states[11].r_I_active) trisa |= 0x10;
    if (states[11].r_1_active) latac |= 0x10;
    /* RA3 (idx 12) */
    if (states[12].r_I_active) trisa |= 0x08;
    if (states[12].r_1_active) latac |= 0x08;

    /* bufferU 패킷 빌드 */
    int j = 0;
    bufferU[j++] = READ_RAM;  bufferU[j++] = 0x0F; bufferU[j++] = 0x80;  /* PORTA */
    bufferU[j++] = READ_RAM;  bufferU[j++] = 0x0F; bufferU[j++] = 0x81;  /* PORTB */
    bufferU[j++] = READ_RAM;  bufferU[j++] = 0x0F; bufferU[j++] = 0x82;  /* PORTC */
    bufferU[j++] = WRITE_RAM; bufferU[j++] = 0x0F; bufferU[j++] = 0x92;  /* TRISA */
    bufferU[j++] = trisa;
    bufferU[j++] = WRITE_RAM; bufferU[j++] = 0x0F; bufferU[j++] = 0x93;  /* TRISB */
    bufferU[j++] = trisb;
    bufferU[j++] = WRITE_RAM; bufferU[j++] = 0x0F; bufferU[j++] = 0x94;  /* TRISC */
    bufferU[j++] = trisc;
    bufferU[j++] = EXT_PORT;  bufferU[j++] = latb; bufferU[j++] = latac; /* LATB, LATAC */
    bufferU[j++] = READ_ADC;  /* VPP ADC */
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0;

    PacketIO(2);

    /* 응답 파싱 */
    int z;
    /* PORTA */
    for (z = 0; z < DIMBUF - 5 && bufferI[z] != READ_RAM; z++);
    port_a = bufferI[z + 3];
    z += 4;
    for (; z < DIMBUF - 5 && bufferI[z] != READ_RAM; z++);
    port_b = bufferI[z + 3];
    z += 4;
    for (; z < DIMBUF - 5 && bufferI[z] != READ_RAM; z++);
    port_c = bufferI[z + 3];
    z += 4;
    for (; z < DIMBUF - 3 && bufferI[z] != READ_ADC; z++);
    raw_adc = (bufferI[z + 1] << 8) + bufferI[z + 2];
    vpp = raw_adc / 1024.0 * 5.0 * 34.0 / 12.0;

    /* 라벨 갱신 */
    for (i = 0; i < 8; i++) {
        strcpy(states[i].label, (port_b & (1 << i)) ? "1" : "0");
    }
    strcpy(states[8].label,  (port_c & 0x80) ? "1" : "0");  /* RC7 */
    strcpy(states[9].label,  (port_c & 0x40) ? "1" : "0");  /* RC6 */
    strcpy(states[10].label, (port_a & 0x20) ? "1" : "0");  /* RA5 */
    strcpy(states[11].label, (port_a & 0x10) ? "1" : "0");  /* RA4 */
    strcpy(states[12].label, (port_a & 0x08) ? "1" : "0");  /* RA3 */

    return vpp;
}

/* VPPVDDactive 코어 로직 */
void VPPVDDactiveCore(int vpp_on, int vdd_on)
{
    int vdd_vpp = 0;
    if (vpp_on) vdd_vpp += 4;  /* bit 2 = VPP */
    if (vdd_on) vdd_vpp += 1;  /* bit 0 = VDD */
    int j = 0;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = vdd_vpp;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0;
    PacketIO(2);
}

/* DCDCactive 코어 로직 */
void DCDCactiveCore(int dcdc_on, double voltage)
{
    int j = 0;
    if (dcdc_on) {
        int vreg = (int)(voltage * 10.0);
        bufferU[j++] = VREG_EN;
        bufferU[j++] = SET_VPP;
        bufferU[j++] = vreg;
        bufferU[j++] = FLUSH;
    } else {
        bufferU[j++] = VREG_DIS;
        bufferU[j++] = FLUSH;
    }
    for (; j < DIMBUF; j++) bufferU[j] = 0;
    PacketIO(2);
}

/* getOptions 의 Config Words 처리 코어 로직.
 * cw1..cw7: 0x10000 = 미설정. 아니면 해당 값.
 * devType: PIC12/PIC16/PIC18/...
 * dev: 디바이스 이름 (16F1xxx 판별용)
 * sizeW, memCODE_W, memCONFIG는 common.h extern 으로 접근.
 * 반환: 적용된 config word 개수 */
int ApplyConfigWordsCore(int devType, const char *devName,
                         unsigned int cw1, unsigned int cw2,
                         unsigned int cw3, unsigned int cw4,
                         unsigned int cw5, unsigned int cw6,
                         unsigned int cw7, int forceConfig)
{
    (void)cw3; (void)cw4; (void)cw5; (void)cw6; (void)cw7;
    if (!forceConfig) return 0;
    int applied = 0;

    if (devType == PIC16) {
        if (!strncmp(devName, "16F1", 4) || !strncmp(devName, "12F1", 4)) {
            /* PIC16F1xxx / PIC12F1xxx */
            if (cw1 <= 0x3FFF && sizeW > 0x8007) {
                memCODE_W[0x8007] = cw1;
                applied++;
            }
            if (cw2 <= 0x3FFF && sizeW > 0x8008) {
                memCODE_W[0x8008] = cw2;
                applied++;
            }
        } else {
            /* 일반 PIC16F */
            if (cw1 <= 0x3FFF && sizeW > 0x2007) {
                memCODE_W[0x2007] = cw1;
                applied++;
            }
            if (cw2 <= 0x3FFF && sizeW > 0x2008) {
                memCODE_W[0x2008] = cw2;
                applied++;
            }
        }
    } else if (devType == PIC12) {
        if (cw1 <= 0xFFF && sizeW > 0xFFF) {
            memCODE_W[0xFFF] = cw1;
            applied++;
        }
    } else if (devType == PIC18) {
        if (cw1 <= 0xFFFF) { memCONFIG[0] = cw1 & 0xFF; memCONFIG[1] = (cw1 >> 8) & 0xFF; applied++; }
        if (cw2 <= 0xFFFF) { memCONFIG[2] = cw2 & 0xFF; memCONFIG[3] = (cw2 >> 8) & 0xFF; applied++; }
        if (cw3 <= 0xFFFF) { memCONFIG[4] = cw3 & 0xFF; memCONFIG[5] = (cw3 >> 8) & 0xFF; applied++; }
        if (cw4 <= 0xFFFF) { memCONFIG[6] = cw4 & 0xFF; memCONFIG[7] = (cw4 >> 8) & 0xFF; applied++; }
        if (cw5 <= 0xFFFF) { memCONFIG[8] = cw5 & 0xFF; memCONFIG[9] = (cw5 >> 8) & 0xFF; applied++; }
        if (cw6 <= 0xFFFF) { memCONFIG[10] = cw6 & 0xFF; memCONFIG[11] = (cw6 >> 8) & 0xFF; applied++; }
        if (cw7 <= 0xFFFF) { memCONFIG[12] = cw7 & 0xFF; memCONFIG[13] = (cw7 >> 8) & 0xFF; applied++; }
    }
    return applied;
}

/* ====================================================================
 * 설정 파일 (opgui.ini) 읽기/쓰기
 * ==================================================================== */

#include <sys/stat.h>

/* 프로그램 시작 시 호출. device/vid/pid/max_err 를 파일에서 읽어옴.
 * 초기값 백업을 위해 *_ini 포인터에 값을 채움 (변경 감지용). */
void LoadConfigFile(char *dev_out, int dev_sz,
                    int *vid_out, int *pid_out, int *max_err_out,
                    char *dev_ini, int dev_ini_sz,
                    int *vid_ini, int *pid_ini, int *max_err_ini)
{
    const char *homedir = getenv("HOME");
    if (!homedir) {
#if defined _WIN32
        homedir = getenv("USERPROFILE");
#endif
    }
    if (!homedir) return;

    char config_dir[1024];
    snprintf(config_dir, sizeof(config_dir), "%s/.opgui", homedir);
#if defined _WIN32
    mkdir(config_dir);
#else
    mkdir(config_dir, 0755);
#endif
    char fname[1024];
    snprintf(fname, sizeof(fname), "%s/.opgui/opgui.ini", homedir);

    /* 설정 파일에서 읽은 값인지 추적 (안 읽었으면 전역 변수 기본값 유지) */
    int vid_read = 0, pid_read = 0, maxerr_read = 0;
    int vid_val = 0, pid_val = 0, maxerr_val = 0;

    FILE *f = fopen(fname, "r");
    if (f) {
        char line[256], temp[256];
        int X;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "device %63s", temp) > 0) {
                strncpy(dev_out, temp, dev_sz - 1);
                dev_out[dev_sz - 1] = 0;
            } else if (sscanf(line, "vid %X", &X) > 0) {
                vid_val = X;
                vid_read = 1;
            } else if (sscanf(line, "pid %X", &X) > 0) {
                pid_val = X;
                pid_read = 1;
            } else {
                int me;
                if (sscanf(line, "maxerr %d", &me) > 0) {
                    maxerr_val = me;
                    maxerr_read = 1;
                }
            }
        }
        fclose(f);
    }

    /* 설정 파일에서 읽은 값만 전역 변수에 반영.
     * 안 읽은 항목은 전역 변수의 기본값 (vid=0x1209, pid=0x5432 등) 유지. */
    if (vid_read && vid_val > 0) *vid_out = vid_val;
    if (pid_read && pid_val > 0) *pid_out = pid_val;
    if (maxerr_read && maxerr_val > 0) *max_err_out = maxerr_val;

    /* 초기값 백업 (변경 감지용) */
    if (dev_ini && dev_ini_sz > 0) {
        strncpy(dev_ini, dev_out, dev_ini_sz - 1);
        dev_ini[dev_ini_sz - 1] = 0;
    }
    if (vid_ini) *vid_ini = *vid_out;
    if (pid_ini) *pid_ini = *pid_out;
    if (max_err_ini) *max_err_ini = *max_err_out;
}

/* 프로그램 종료 시 호출. 변경이 있으면 파일에 저장. */
void SaveConfigFile(const char *dev, int vid, int pid, int max_err,
                    const char *dev_ini, int vid_ini, int pid_ini, int max_err_ini)
{
    /* 변경 감지 */
    if (strcmp(dev_ini, dev) == 0 &&
        vid_ini == vid && pid_ini == pid && max_err_ini == max_err) {
        return;  /* 변경 없음 */
    }

    const char *homedir = getenv("HOME");
    if (!homedir) {
#if defined _WIN32
        homedir = getenv("USERPROFILE");
#endif
    }
    if (!homedir) return;

    char fname[1024];
    snprintf(fname, sizeof(fname), "%s/.opgui/opgui.ini", homedir);
    FILE *f = fopen(fname, "w");
    if (!f) return;
    fprintf(f, "device %s\n", dev);
    fprintf(f, "maxerr %d\n", max_err);
    fprintf(f, "vid %X\n", vid);
    fprintf(f, "pid %X\n", pid);
    fclose(f);
}

/* ====================================================================
 * 명령행 모드 (-c 옵션) - GUI 없이 동작
 * ==================================================================== */

/* forward 선언 - 본 파일의 FindDevice 사용 */
int FindDevice(int vid, int pid);

/* 명령행 인자들을 16진수로 파싱하여 tmpbuf에 저장.
 * argc/argv: 프로그램 인자 (옵션 제외한 나머지)
 * tmpbuf: 출력 버퍼 (최소 128 바이트)
 * 반환: 파싱된 바이트 수 */
int ParseCommandLinePackets(int argc, char **argv, int optind,
                            unsigned int *tmpbuf, int tmpbuf_sz)
{
    int j = 0, i;
    for (i = optind; i < argc && j < tmpbuf_sz; i++, j++) {
        sscanf(argv[i], "%x", &tmpbuf[j]);
    }
    for (; j < tmpbuf_sz; j++) tmpbuf[j] = 0;
    return j;
}

/* 명령행 모드 실행. tmpbuf의 데이터를 USB로 전송하고 응답 출력.
 * 반환: 0 = 성공, -1 = 장치 없음. */
int RunCommandLineMode(unsigned int *tmpbuf, int tmpbuf_sz)
{
    int i;
    cmdline = 1;
    DeviceDetected = FindDevice(vid, pid);
    if (!DeviceDetected) {
        DeviceDetected = FindDevice(new_vid, new_pid);
        if (DeviceDetected) { vid = new_vid; pid = new_pid; }
    }
    if (!DeviceDetected) {
        DeviceDetected = FindDevice(old_vid, old_pid);
    }
    if (DeviceDetected) {
        bufferU[0] = 0;
        for (i = 1; i < DIMBUF && (i - 1) < tmpbuf_sz; i++) {
            bufferU[i] = (unsigned char)tmpbuf[i - 1];
        }
        for (; i < DIMBUF; i++) bufferU[i] = 0;
        PacketIO(100);
        printf("> ");
        for (i = 1; i < DIMBUF; i++) printf("%02X ", bufferU[i]);
        printf("\n< ");
        for (i = 1; i < DIMBUF; i++) printf("%02X ", bufferI[i]);
        printf("\n");
        return 0;
    }
    printf("No programmer found\n");
    return -1;
}

/* dev[] - common.h 가 아닌 opgui_wx.cpp 와 deviceRW.c 에서 사용.
 * 원본 opgui.c에 정의됨. */
char dev[64] = "";

/* groupNames - 원본 opgui.c에 정의됨. deviceRW.h 에서 extern char* groupNames[NUM_GROUPS] */
char *groupNames[NUM_GROUPS] = {
    "PIC10/12",
    "PIC16",
    "PIC18",
    "PIC24",
    "PIC30/33",
    "ATMEL AVR",
    "MEMORY"
};

/* 원본 opgui.c에서 사용하던 hiddev 관련 구조체 (Linux) */
#if !defined _WIN32 && !defined __CYGWIN__
    #ifdef hiddevIO
        #include <linux/hiddev.h>
        static struct hiddev_report_info rep_info_u, rep_info_i;
        static struct hiddev_usage_ref_multi ref_multi_u, ref_multi_i;
    #endif
#endif

/* Windows에서 사용하는 전역 변수들 - hidapi 기반 */
#if defined _WIN32 || defined __CYGWIN__
    unsigned char bufferU0[128] = {0};  /* [0]=report ID (0), [1..]=데이터 */
    unsigned char bufferI0[128] = {0};
    unsigned char *bufferU = bufferU0 + 1;  /* write: report ID 후 데이터 */
    /* hidapi는 read 시 report ID를 반환하지 않으므로 bufferI = bufferI0.
     * (원본 ReadFile은 report ID 포함 65바이트 반환 → bufferI0+1이었음) */
    unsigned char *bufferI = bufferI0;
    /* hidapi 장치 핸들 */
    static hid_device *g_hidDev = NULL;
    /* 호환성을 위한 레거시 변수들 */
    DWORD NumberOfBytesRead = 0, BytesWritten = 0;
    ULONG Result = 0;
    HANDLE WriteHandle = NULL, ReadHandle = NULL;
    OVERLAPPED HIDOverlapped;
    HANDLE hEventObject = NULL;
#else
    unsigned char bufferU[128], bufferI[128];
#endif

/* ----- Linux GetTickCount ----- */
#if !defined _WIN32 && !defined __CYGWIN__
DWORD GetTickCount(void) {
    struct timeb now;
    ftime(&now);
    return (DWORD)(now.time * 1000 + now.millitm);
}
#endif

/* ----- msDelay ----- */
void msDelay(double delay) {
#if !defined _WIN32 && !defined __CYGWIN__
    long x = (long)(delay * 1000.0);
    usleep(x > MinDly ? x : MinDly);
#else
    __int64 stop, freq, timeout;
    QueryPerformanceCounter((LARGE_INTEGER *)&stop);
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    timeout = stop + (__int64)(delay * freq / 1000.0);
    while (stop < timeout) QueryPerformanceCounter((LARGE_INTEGER *)&stop);
#endif
}

/* ----- PacketIO ----- */
#define TIMEOUT 50
void PacketIO(double delay) {
    int delay0 = (int)delay;
#if !defined _WIN32 && !defined __CYGWIN__
    struct timespec ts;
    uint64_t start, stop;
    fd_set set;
    struct timeval timeout;
    int rv;

    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT * 1000;
    clock_gettime(CLOCK_REALTIME, &ts);
    start = ts.tv_nsec / 1000;
    delay -= TIMEOUT - 10;
    if (delay < MinDly) delay = MinDly;

    /* write */
    int res = write(fd, bufferU, DIMBUF);
    if (res < 0) {
        sprintf(str, "PacketIO write error: %d\n", errno);
        PrintMessage(str);
    }
    usleep((int)(delay * 1000.0));

    /* read with select timeout */
    rv = select(fd + 1, &set, NULL, NULL, &timeout);
    if (rv == -1) {
        PrintMessage("PacketIO select error\n");
        if (saveLog && logfile) fprintf(logfile, "select error\n");
        return;
    } else if (rv == 0) {
        PrintMessage("Comm timeout\n");
        if (saveLog && logfile) fprintf(logfile, "Comm timeout\n");
        return;
    }
    res = read(fd, bufferI, DIMBUF);
    if (res < 0) {
        sprintf(str, "PacketIO read error: %d\n", errno);
        PrintMessage(str);
    }

    clock_gettime(CLOCK_REALTIME, &ts);
    stop = ts.tv_nsec / 1000;
    if (saveLog && logfile) {
        WriteLogIO();
        fprintf(logfile, "T=%.2f ms (%+.2f ms)\n",
                (stop - start) / 1000.0,
                (stop - start) / 1000.0 - delay0);
        if (bufferU[0] != bufferI[0])
            fprintf(logfile, "Cannot read correct packet!!\n");
    }
#else  /* Windows - hidapi 기반 */
    __int64 start, stop, freq;
    QueryPerformanceCounter((LARGE_INTEGER *)&start);
    QueryPerformanceFrequency((LARGE_INTEGER *)&freq);
    delay -= TIMEOUT - 10;
    if (delay < MinDly) delay = MinDly;

    if (!g_hidDev) {
        PrintMessage("PacketIO: device not open\n");
        return;
    }

    /* write 전에 bufferU0[0] = 0 (report ID) 명시적 보장 */
    bufferU0[0] = 0;

    /* 1. write 전에 이전 패킷의 잔여 응답 제거 (drain).
     * 단, non-blocking으로 즉시 반환되도록 설정하여
     * 정상 응답이 없으면 바로 진행.
     * 이전 PacketIO에서 read가 완료되지 않은 stale 데이터만 제거. */
    hid_set_nonblocking(g_hidDev, 1);
    {
        unsigned char tmp[256];
        while (hid_read(g_hidDev, tmp, sizeof(tmp)) > 0) { /* drain stale */ }
    }

    /* 2. hidapi로 패킷 송신 (Output Report). */
    int wres = hid_write(g_hidDev, bufferU0, DIMBUF + 1);
    if (wres < 0) {
        sprintf(str, "PacketIO write error: %ls\n", hid_error(g_hidDev));
        PrintMessage(str);
        hid_set_nonblocking(g_hidDev, 0);
        return;
    }

    /* 3. delay 대기 (busy-wait) - 원본과 동일. */
    QueryPerformanceCounter((LARGE_INTEGER *)&stop);
    __int64 timeout_qpc = stop + (__int64)(delay * freq / 1000.0);
    while (stop < timeout_qpc) QueryPerformanceCounter((LARGE_INTEGER *)&stop);

    /* 4. 패킷 수신 - blocking 모드로 전환 후 TIMEOUT ms 타임아웃.
     * 중요: hidapi on Windows는 report ID를 반환하지 않음.
     * 따라서 DIMBUF 바이트만 읽으면 됨 (원본 ReadFile은 DIMBUF+1이었음).
     * bufferI = bufferI0 이므로 bufferI[0]이 첫 응답 바이트. */
    hid_set_nonblocking(g_hidDev, 0);  /* blocking 모드 */
    int total_read = 0;
    int rres = 0;
    int read_attempts = 0;
    while (total_read < DIMBUF && read_attempts < 5) {
        rres = hid_read_timeout(g_hidDev, bufferI0 + total_read,
                                DIMBUF - total_read, TIMEOUT);
        if (rres < 0) {
            sprintf(str, "PacketIO read error: %ls\n", hid_error(g_hidDev));
            PrintMessage(str);
            if (saveLog && logfile) fprintf(logfile, "read error\n");
            return;
        } else if (rres == 0) {
            break;
        }
        total_read += rres;
        read_attempts++;
    }

    if (total_read == 0) {
        PrintMessage("Comm timeout\n");
        if (saveLog && logfile) fprintf(logfile, "Comm timeout\n");
        return;
    }
    if (total_read < DIMBUF) {
        if (saveLog && logfile)
            fprintf(logfile, "Partial read: %d/%d bytes\n", total_read, DIMBUF);
        memset(bufferI0 + total_read, 0, DIMBUF - total_read);
    }

    /* 5. 응답 검증 - bufferI[0]이 bufferU[0]과 일치하는지 확인.
     * bufferI = bufferI0 (hidapi는 report ID 없음)
     * bufferU = bufferU0 + 1 (report ID 후 데이터)
     * 둘 다 첫 명령/응답 바이트를 가리킴. */
    int retries = 0;
    while (bufferI[0] != bufferU[0] && retries < 3) {
        total_read = 0;
        while (total_read < DIMBUF) {
            rres = hid_read_timeout(g_hidDev, bufferI0 + total_read,
                                    DIMBUF - total_read, TIMEOUT);
            if (rres <= 0) break;
            total_read += rres;
        }
        if (total_read == 0) break;
        retries++;
    }
    if (saveLog && logfile && bufferI[0] != bufferU[0]) {
        fprintf(logfile, "Cannot read correct packet!! (after %d retries)\n", retries);
    }

    if (saveLog && logfile) WriteLogIO();

    QueryPerformanceCounter((LARGE_INTEGER *)&stop);
    if (saveLog && logfile)
        fprintf(logfile, "T=%.2f ms (%+.2f ms)\n",
                (stop - start) * 1000.0 / freq,
                (stop - start) * 1000.0 / freq - delay0);
#endif
}

/* ----- FindDevice ----- */
int FindDevice(int vid, int pid) {
#if !defined _WIN32 && !defined __CYGWIN__
    /* Linux: hidraw 사용 */
    #ifndef hiddevIO
    struct hidraw_devinfo device_info;
    int i = -1;

    if (path[0] == 0) {
        if ((fd = open("/dev/openprogrammer", O_RDWR | O_NONBLOCK)) > 0) {
            sprintf(path, "/dev/openprogrammer");
            ioctl(fd, HIDIOCGRAWINFO, &device_info);
            if (device_info.vendor == vid && device_info.product == pid)
                i = 0;
            else { close(fd); i = -1; }
        }
        if (i) {
            for (i = 0; i < 16; i++) {
                sprintf(path, "/dev/hidraw%d", i);
                if ((fd = open(path, O_RDWR | O_NONBLOCK)) > 0) {
                    ioctl(fd, HIDIOCGRAWINFO, &device_info);
                    if (device_info.vendor == vid &&
                        device_info.product == pid) break;
                    else close(fd);
                }
            }
        }
        if (i == 16) {
            PrintMessage(strings[S_noprog]);
            path[0] = 0;
            return 0;
        }
    } else {
        if ((fd = open(path, O_RDWR | O_NONBLOCK)) < 0) {
            PrintMessage1(strings[S_DevPermission], path);
            return 0;
        }
        ioctl(fd, HIDIOCGRAWINFO, &device_info);
        if (device_info.vendor != vid || device_info.product != pid) {
            PrintMessage(strings[S_noprog]);
            return 0;
        }
    }
    sprintf(str, "Programmer found at %s (VID=%04X PID=%04X)\n",
            path, vid, pid);
    PrintMessage(str);
    return 1;
    #else
    /* hiddev path - older Linux method, deprecated */
    return 0;
    #endif
#else
    /* Windows: hidapi 라이브러리 사용 (SetupAPI/HID 추상화) */
    /* 기존 장치 핸들이 있으면 닫기 */
    if (g_hidDev) {
        hid_close(g_hidDev);
        g_hidDev = NULL;
    }

    /* hidapi 초기화 (첫 호출 시 자동 초기화되지만 명시적으로 호출) */
    if (hid_init() != 0) {
        PrintMessage("hidapi init failed\n");
        return 0;
    }

    /* VID/PID로 장치 열기. serial_number=NULL 이면 첫 번째 매칭 장치. */
    g_hidDev = hid_open(vid, pid, NULL);
    if (!g_hidDev) {
        /* 장치를 못 찾음 - 상세 에러 메시지 */
        const wchar_t *err = hid_error(NULL);
        if (err) {
            /* wchar_t → char 변환 (간단히) */
            char errbuf[256];
            int i;
            for (i = 0; i < 255 && err[i]; i++) {
                errbuf[i] = (err[i] < 128) ? (char)err[i] : '?';
            }
            errbuf[i] = 0;
            sprintf(str, "No programmer found (VID=%04X PID=%04X): %s\n",
                    vid, pid, errbuf);
        } else {
            sprintf(str, "No programmer found (VID=%04X PID=%04X)\n", vid, pid);
        }
        PrintMessage(str);
        return 0;
    }

    /* 장치 발견 - blocking 모드 해제 (타임아웃 기반 읽기 위해) */
    /* hid_set_nonblocking(g_hidDev, 1); */  /* 0=blocking, 1=non-blocking */

    sprintf(str, "Programmer connected (VID=%04X PID=%04X)\n", vid, pid);
    PrintMessage(str);
    return 1;
#endif
}
