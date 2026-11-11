/**
 * \file icd_core.c
 * \brief ICD (In-Circuit Debugger) 핵심 로직 - GTK 독립 버전.
 *
 * 원본 icd.c에서 GTK에 직접 의존하는 부분을 제외한 핵심 로직:
 * - startICD, isRunning, cont, step, run, version, Halt
 * - ReadRegister, ReadRegisterN, WriteRegister
 * - ReadProgMem, ReadProgMemN, ReadDataMem, ReadDataMemN
 * - getVar, decodeCmd, initVar
 * - ShowBank, ShowContext (로직 부분)
 * - addWatch, removeWatch, executeCommand
 *
 * GUI 관련 함수 (PrintMessageICD, AppendMessageICD, scrollToLine,
 * SourceHilightLine, loadSource) 는 wxWidgets 측에서
 * gui_icd_* 콜백으로 제공됨.
 *
 * ShowContext, executeCommand, icdRun/Halt/Step/Stop 등은
 * icd_wx.cpp에서 재작성. 단, ShowBank, addWatch, removeWatch 등은
 * 본 파일에 그대로 둠.
 */

#include "common.h"
#include "coff.h"
#include "icd.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---- GUI 추상화 콜백 (icd_wx.cpp에서 제공) ---- */
extern void gui_icd_print_status(const char *msg);
extern void gui_icd_append_status(const char *msg);
extern void gui_icd_set_source(const char *text);
extern void gui_icd_scroll_to_line(int line);
extern void gui_icd_hilight_line(int line);
extern void gui_icd_remove_hilight(int line);
extern int  gui_icd_load_source(int src_file_idx);
extern int  gui_icd_menu_is_checked(int menu_id);
extern void gui_icd_start_timer(int ms);
extern void gui_icd_stop_timer(void);
extern int  gui_icd_load_coff(const char *filename);  /* returns 1 if ok */

/* 메뉴 ID */
#define ICD_MENU_PC     1
#define ICD_MENU_STAT   2
#define ICD_MENU_BANK0  3
#define ICD_MENU_BANK1  4
#define ICD_MENU_BANK2  5
#define ICD_MENU_BANK3  6
#define ICD_MENU_EE     7

/* ---- 전역 변수 (원본 icd.c에서 정의) ---- */
int icdTimer = 0;
int break_addr, print_addr;
int currentSource = -1;
int sourceHilight = 0;
char lastCmd[64] = "";
int UseCoff = 0;
struct src_i source_info[LMAX];
struct srcfile *s_files;
struct symbol *sym;
int nsym = 0;
char *Slabel[LMAX], *Sulabel[ULMAX];
struct symbol *watch;
int nwatch = 0;
unsigned short coff_data[DATA_MAX];
int ver = 0, reset = 1, freeze = 0, icdConnected = 0, running = 0;
#define Tck 30
double Tcom = 0.001 * Tck * 18 + 0.03;

/* GtkWidget 호환 더미 (gui_shim.h와 함께 사용하기 위함) */
GtkWidget *statusTxt = NULL;
GtkWidget *sourceTxt = NULL;
GtkTextBuffer *sourceBuf = NULL;
GtkWidget *icdVbox1 = NULL;
GtkWidget *icdMenuPC = NULL;
GtkWidget *icdMenuSTAT = NULL;
GtkWidget *icdMenuBank0 = NULL;
GtkWidget *icdMenuBank1 = NULL;
GtkWidget *icdMenuBank2 = NULL;
GtkWidget *icdMenuBank3 = NULL;
GtkWidget *icdMenuEE = NULL;
GtkWidget *icdCommand = NULL;
GtkTextBuffer *statusBuf = NULL;

/* variables[] 배열은 icd.h에서 extern 선언됨.
 * 본 파일에서 정의 제공. struct var도 icd.h에 정의됨. */
struct var_def_compat { char *name; int display; };
/* icd.h: extern struct var{char* name; int display;} variables[0x200]; */
/* 본 파일에서 variables[] 정의 (icd.h의 struct var 타입 사용) */
struct var variables[0x200];

/* ---- forward declarations ---- */
void ShowContext();
void startICD(int tck);

/* ====================================================================
 * 핵심 ICD 프로토콜 함수들 - 원본 icd.c에서 그대로 복사
 * ==================================================================== */

void startICD(int tck) {
    int j = 0;
    bufferU[j++] = PROG_RST;
    bufferU[j++] = SET_PARAMETER;
    bufferU[j++] = SET_T1T2;
    bufferU[j++] = tck;
    bufferU[j++] = 100;
    bufferU[j++] = SET_PARAMETER;
    bufferU[j++] = SET_T3;
    bufferU[j++] = 2000 >> 8;
    bufferU[j++] = 2000 & 0xff;
    bufferU[j++] = VREG_DIS;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x0;
    bufferU[j++] = WAIT_T3;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x1;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(3);
    if (saveLog) fprintf(logfile, "startICD()\n");
}

int isRunning() {
    int z, j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = READ_PINS;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(2);
    if (saveLog) fprintf(logfile, "isRunning()\n");
    for (z = 0; z < DIMBUF - 1 && bufferI[z] != READ_PINS; z++);
    if (bufferI[z + 1] & 1) running = 0;
    else running = 1;
    return running;
}

void cont(int break_addr_p, int freeze_p) {
    int j = 0;
    break_addr_p &= 0x1FFF;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x2;
    bufferU[j++] = WREG;
    bufferU[j++] = (break_addr_p >> 8) + (freeze_p ? 0x40 : 0);
    bufferU[j++] = 0x1;
    bufferU[j++] = 0x8E;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x2;
    bufferU[j++] = WREG;
    bufferU[j++] = break_addr_p & 0xFF;
    bufferU[j++] = 0x1;
    bufferU[j++] = 0x8F;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x1;
    bufferU[j++] = GO;
    bufferU[j++] = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 5 * Tcom);
    if (saveLog) fprintf(logfile, "continue()\n");
    running = 1;
}

void step() {
    int j = 0;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x1;
    bufferU[j++] = STEP;
    bufferU[j++] = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + Tcom);
    if (saveLog) fprintf(logfile, "step()\n");
}

void run() {
    int j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x5;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(2);
    if (saveLog) fprintf(logfile, "run()\n");
    running = 1;
}

int version() {
    int j = 0, z;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x1;
    bufferU[j++] = VER;
    bufferU[j++] = 0;
    bufferU[j++] = RX16;
    bufferU[j++] = 0x1;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 2 * Tcom);
    if (saveLog) fprintf(logfile, "version()\n");
    for (z = 0; z < DIMBUF - 2 && bufferI[z] != RX16; z++);
    return bufferI[z + 3];
}

void Halt() {
    int j = 0;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x6;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(2);
    if (saveLog) fprintf(logfile, "halt()\n");
    running = 0;
}

int ReadRegister(int addr) {
    int j = 0, z;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x2;
    bufferU[j++] = RREG;
    bufferU[j++] = 0x1;
    bufferU[j++] = (addr >> 8) & 0xFF;
    bufferU[j++] = addr & 0xFF;
    bufferU[j++] = RX16;
    bufferU[j++] = 0x1;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 3 * Tcom);
    if (saveLog) fprintf(logfile, "ReadRegister(0x%X)\n", addr);
    for (z = 0; z < DIMBUF - 2 && bufferI[z] != RX16; z++);
    return bufferI[z + 3];
}

int ReadRegisterN(int addr, int n, int *buf) {
    int i, j = 0, z, w;
    for (i = 0; i < n; i += w) {
        w = i + (DIMBUF - 9) / 2 < n ? (DIMBUF - 9) / 2 : n - i;
        bufferU[j++] = TX16;
        bufferU[j++] = 0x2;
        bufferU[j++] = RREG;
        bufferU[j++] = w;
        bufferU[j++] = (addr + i) >> 8;
        bufferU[j++] = (addr + i) & 0xFF;
        bufferU[j++] = RX16;
        bufferU[j++] = w;
        bufferU[j++] = FLUSH;
        for (; j < DIMBUF; j++) bufferU[j] = 0x0;
        PacketIO(1 + (w + 2) * Tcom);
        if (saveLog) fprintf(logfile, "ReadRegisterN(0x%X,%d)\n", addr, n);
        for (z = 0; z < DIMBUF - 2 && bufferI[z] != RX16; z++);
        for (j = 0; j < w; j++) buf[i + j] = bufferI[z + 3 + j * 2];
        j = 0;
    }
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(2);
    return i == n ? 0 : -1;
}

void WriteRegister(int addr, int data) {
    int j = 0;
    bufferU[j++] = TX16;
    bufferU[j++] = 0x2;
    bufferU[j++] = WREG;
    bufferU[j++] = data & 0xFF;
    bufferU[j++] = (addr >> 8) & 0xFF;
    bufferU[j++] = addr & 0xFF;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 2 * Tcom);
    if (saveLog) fprintf(logfile, "WriteRegister(0x%X,0x%X)\n", addr, data);
}

int ReadProgMem(int addr) {
    int addr_temp, data_temp, eecon_temp, data;
    addr_temp = (ReadRegister(EEADRH) << 8) + ReadRegister(EEADR);
    data_temp = (ReadRegister(EEDATH) << 8) + ReadRegister(EEDATA);
    eecon_temp = ReadRegister(EECON1);
    WriteRegister(EEADRH, addr >> 8);
    WriteRegister(EEADR, addr & 0xFF);
    WriteRegister(EECON1, eecon_temp | 0x80);
    WriteRegister(EECON1, eecon_temp | 0x81);
    data = (ReadRegister(EEDATH) << 8) + ReadRegister(EEDATA);
    WriteRegister(EEADRH, addr_temp << 8);
    WriteRegister(EEADR, addr_temp & 0xFF);
    WriteRegister(EEDATH, data_temp << 8);
    WriteRegister(EEDATA, data_temp & 0xFF);
    WriteRegister(EECON1, eecon_temp);
    return data;
}

int ReadProgMemN(int addr, int n, int *buf) {
    int addr_temp, data_temp, eecon_temp;
    if (saveLog) fprintf(logfile, "ReadProgMemN(0x%X,%d)\n", addr, n);
    int i, j = 0, z, w, k;
    bufferU[j++] = TX16;
    bufferU[j++] = 2;
    bufferU[j++] = RREG;
    bufferU[j++] = 4;
    bufferU[j++] = (EEDATA >> 8) & 0xFF;
    bufferU[j++] = EEDATA & 0xFF;
    bufferU[j++] = RX16;
    bufferU[j++] = 4;
    bufferU[j++] = TX16;
    bufferU[j++] = 2;
    bufferU[j++] = RREG;
    bufferU[j++] = 1;
    bufferU[j++] = (EECON1 >> 8) & 0xFF;
    bufferU[j++] = EECON1 & 0xFF;
    bufferU[j++] = RX16;
    bufferU[j++] = 1;
    bufferU[j++] = TX16;
    bufferU[j++] = 2;
    bufferU[j++] = WREG;
    bufferU[j++] = 0x80;
    bufferU[j++] = (EECON1 >> 8) & 0xFF;
    bufferU[j++] = EECON1 & 0xFF;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 13 * Tcom);
    j = 0;
    for (z = 0; z < DIMBUF - 5 && bufferI[z] != RX16; z++);
    data_temp = bufferI[z + 3] + (bufferI[z + 7] << 8);
    addr_temp = bufferI[z + 5] + (bufferI[z + 9] << 8);
    for (z += 10; z < DIMBUF - 3 && bufferI[z] != RX16; z++);
    eecon_temp = bufferI[z + 3];
    w = k = 0;
    for (i = 0; i < n; i++) {
        bufferU[j++] = TX16;
        bufferU[j++] = 8;
        bufferU[j++] = WREG;
        bufferU[j++] = (addr + i) & 0xFF;
        bufferU[j++] = (EEADR >> 8) & 0xFF;
        bufferU[j++] = EEADR & 0xFF;
        bufferU[j++] = WREG;
        bufferU[j++] = ((addr + i) >> 8) & 0xFF;
        bufferU[j++] = (EEADRH >> 8) & 0xFF;
        bufferU[j++] = EEADRH & 0xFF;
        bufferU[j++] = WREG;
        bufferU[j++] = 0x81;
        bufferU[j++] = (EECON1 >> 8) & 0xFF;
        bufferU[j++] = EECON1 & 0xFF;
        bufferU[j++] = RREG;
        bufferU[j++] = 3;
        bufferU[j++] = (EEDATA >> 8) & 0xFF;
        bufferU[j++] = EEDATA & 0xFF;
        bufferU[j++] = RX16;
        bufferU[j++] = 3;
        w++;
        if (j > DIMBUF - 21 || i == n - 1) {
            bufferU[j++] = FLUSH;
            for (; j < DIMBUF; j++) bufferU[j] = 0x0;
            PacketIO(2 + 13 * Tcom * w);
            j = 0;
            w = 0;
            for (z = 0; z < DIMBUF - 5; z++) {
                if (bufferI[z] == RX16) {
                    buf[k++] = bufferI[z + 3] + (bufferI[z + 7] << 8);
                    z += 8;
                }
            }
        }
    }
    bufferU[j++] = TX16;
    bufferU[j++] = 10;
    bufferU[j++] = WREG;
    bufferU[j++] = eecon_temp;
    bufferU[j++] = (EECON1 >> 8) & 0xFF;
    bufferU[j++] = EECON1 & 0xFF;
    bufferU[j++] = WREG;
    bufferU[j++] = data_temp & 0xFF;
    bufferU[j++] = (EEDATA >> 8) & 0xFF;
    bufferU[j++] = EEDATA & 0xFF;
    bufferU[j++] = WREG;
    bufferU[j++] = data_temp >> 8;
    bufferU[j++] = (EEDATH >> 8) & 0xFF;
    bufferU[j++] = EEDATH & 0xFF;
    bufferU[j++] = WREG;
    bufferU[j++] = addr_temp & 0xFF;
    bufferU[j++] = (EEADR >> 8) & 0xFF;
    bufferU[j++] = EEADR & 0xFF;
    bufferU[j++] = WREG;
    bufferU[j++] = addr_temp >> 8;
    bufferU[j++] = (EEADRH >> 8) & 0xFF;
    bufferU[j++] = EEADRH & 0xFF;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 10 * Tcom);
    return i;
}

int ReadDataMem(int addr) {
    int addr_temp, data_temp, eecon_temp, data;
    addr_temp = ReadRegister(EEADR);
    data_temp = ReadRegister(EEDATA);
    eecon_temp = ReadRegister(EECON1);
    WriteRegister(EEADR, addr);
    WriteRegister(EECON1, eecon_temp & 0x7F);
    WriteRegister(EECON1, (eecon_temp & 0x7F) | 0x1);
    data = ReadRegister(EEDATA);
    WriteRegister(EEADR, addr_temp);
    WriteRegister(EEDATA, data_temp);
    WriteRegister(EECON1, eecon_temp);
    return data;
}

int ReadDataMemN(int addr, int n, unsigned char *buf) {
    int addr_temp, data_temp, eecon_temp;
    if (saveLog) fprintf(logfile, "ReadDataMemN(0x%X,%d)\n", addr, n);
    int i, j = 0, z, w, k;
    bufferU[j++] = TX16;
    bufferU[j++] = 2;
    bufferU[j++] = RREG;
    bufferU[j++] = 2;
    bufferU[j++] = (EEDATA >> 8) & 0xFF;
    bufferU[j++] = EEDATA & 0xFF;
    bufferU[j++] = RX16;
    bufferU[j++] = 2;
    bufferU[j++] = TX16;
    bufferU[j++] = 2;
    bufferU[j++] = RREG;
    bufferU[j++] = 1;
    bufferU[j++] = (EECON1 >> 8) & 0xFF;
    bufferU[j++] = EECON1 & 0xFF;
    bufferU[j++] = RX16;
    bufferU[j++] = 1;
    bufferU[j++] = TX16;
    bufferU[j++] = 6;
    bufferU[j++] = WREG;
    bufferU[j++] = 0;
    bufferU[j++] = (EECON1 >> 8) & 0xFF;
    bufferU[j++] = EECON1 & 0xFF;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 13 * Tcom);
    j = 0;
    for (z = 0; z < DIMBUF - 5 && bufferI[z] != RX16; z++);
    data_temp = bufferI[z + 3];
    addr_temp = bufferI[z + 5];
    for (z += 6; z < DIMBUF - 3 && bufferI[z] != RX16; z++);
    eecon_temp = bufferI[z + 3];
    w = k = 0;
    for (i = 0; i < n; i++) {
        bufferU[j++] = TX16;
        bufferU[j++] = 6;
        bufferU[j++] = WREG;
        bufferU[j++] = (addr + i) & 0xFF;
        bufferU[j++] = (EEADR >> 8) & 0xFF;
        bufferU[j++] = EEADR & 0xFF;
        bufferU[j++] = WREG;
        bufferU[j++] = 0x1;
        bufferU[j++] = (EECON1 >> 8) & 0xFF;
        bufferU[j++] = EECON1 & 0xFF;
        bufferU[j++] = RREG;
        bufferU[j++] = 1;
        bufferU[j++] = (EEDATA >> 8) & 0xFF;
        bufferU[j++] = EEDATA & 0xFF;
        bufferU[j++] = RX16;
        bufferU[j++] = 1;
        w++;
        if (j > DIMBUF - 17 || i == n - 1) {
            bufferU[j++] = FLUSH;
            for (; j < DIMBUF; j++) bufferU[j] = 0x0;
            PacketIO(2 + 10 * Tcom * w);
            j = 0;
            w = 0;
            for (z = 0; z < DIMBUF - 5; z++) {
                if (bufferI[z] == RX16) {
                    buf[k++] = bufferI[z + 3];
                    z += 4;
                }
            }
        }
    }
    bufferU[j++] = TX16;
    bufferU[j++] = 6;
    bufferU[j++] = WREG;
    bufferU[j++] = eecon_temp;
    bufferU[j++] = (EECON1 >> 8) & 0xFF;
    bufferU[j++] = EECON1 & 0xFF;
    bufferU[j++] = WREG;
    bufferU[j++] = data_temp & 0xFF;
    bufferU[j++] = (EEDATA >> 8) & 0xFF;
    bufferU[j++] = EEDATA & 0xFF;
    bufferU[j++] = WREG;
    bufferU[j++] = addr_temp & 0xFF;
    bufferU[j++] = (EEADR >> 8) & 0xFF;
    bufferU[j++] = EEADR & 0xFF;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(1 + 10 * Tcom);
    return i;
}

/* ====================================================================
 * 디스어셈블러 및 변수명 - 원본 icd.c에서 그대로 복사
 * ==================================================================== */

char *getVar(int addr, char *var) {
    addr &= 0x1FF;
    if (variables[addr].name) strcpy(var, variables[addr].name);
    else sprintf(var, "0x%03X", addr);
    return var;
}

char *decodeCmd(int cmd, char *str_out, int addrH) {
    char ins[32], reg[32];
    if ((cmd & 0x3F9F) == 0) sprintf(str_out, "nop");
    else if (cmd == 0x0001) sprintf(str_out, "reset");
    else if (cmd == 0x0008) sprintf(str_out, "return");
    else if (cmd == 0x0009) sprintf(str_out, "retfie");
    else if (cmd == 0x000A) sprintf(str_out, "callw");
    else if (cmd == 0x000B) sprintf(str_out, "brw");
    else if (cmd == 0x0062) sprintf(str_out, "option");
    else if (cmd == 0x0063) sprintf(str_out, "sleep");
    else if (cmd == 0x0064) sprintf(str_out, "clrwdt");
    else if (cmd == 0x0065) sprintf(str_out, "trisa");
    else if (cmd == 0x0066) sprintf(str_out, "trisb");
    else if (cmd == 0x0067) sprintf(str_out, "trisc");
    else if ((cmd >> 12) == 0) {
        if ((cmd >> 8) == 0 && cmd & 0x80) sprintf(str_out, "movwf %s", getVar(addrH + (cmd & 0x7F), reg));
        else if ((cmd >> 8) == 1) {
            if (cmd & 0x80) sprintf(str_out, "clrf %s", getVar(addrH + (cmd & 0x7F), reg));
            else sprintf(str_out, "clrf w");
        } else {
            switch (cmd >> 8) {
                case 2: sprintf(ins, "subwf"); break;
                case 3: sprintf(ins, "decf"); break;
                case 4: sprintf(ins, "iorwf"); break;
                case 5: sprintf(ins, "andwf"); break;
                case 6: sprintf(ins, "xorwf"); break;
                case 7: sprintf(ins, "addwf"); break;
                case 8: sprintf(ins, "movf"); break;
                case 9: sprintf(ins, "comf"); break;
                case 10: sprintf(ins, "incf"); break;
                case 11: sprintf(ins, "decfsz"); break;
                case 12: sprintf(ins, "rrf"); break;
                case 13: sprintf(ins, "rlf"); break;
                case 14: sprintf(ins, "swapf"); break;
                case 15: sprintf(ins, "incfsz"); break;
                default: sprintf(ins, "???"); break;
            }
            sprintf(str_out, "%s %s,%c", ins, getVar(addrH + (cmd & 0x7F), reg), cmd & 0x80 ? 'f' : 'w');
        }
    } else if ((cmd >> 12) == 1) {
        switch (cmd >> 10) {
            case 4: sprintf(ins, "bcf"); break;
            case 5: sprintf(ins, "bsf"); break;
            case 6: sprintf(ins, "btfsc"); break;
            case 7: sprintf(ins, "btfss"); break;
            default: sprintf(ins, "??"); break;
        }
        sprintf(str_out, "%s %s,%d", ins, getVar(addrH + (cmd & 0x7F), reg), (cmd & 0x380) >> 7);
    } else if ((cmd >> 12) == 2) sprintf(str_out, "%s 0x%X", cmd & 0x800 ? "goto" : "call", cmd & 0x7FF);
    else if ((cmd >> 10) == 0xC) sprintf(str_out, "movlw 0x%X", cmd & 0xFF);
    else if ((cmd >> 10) == 0xD) sprintf(str_out, "retlw 0x%X", cmd & 0xFF);
    else if ((cmd >> 9) == 0x1E) sprintf(str_out, "sublw 0x%X", cmd & 0xFF);
    else if ((cmd >> 9) == 0x1F) sprintf(str_out, "addlw 0x%X", cmd & 0xFF);
    else if ((cmd >> 8) == 0x38) sprintf(str_out, "iorlw 0x%X", cmd & 0xFF);
    else if ((cmd >> 8) == 0x39) sprintf(str_out, "andlw 0x%X", cmd & 0xFF);
    else if ((cmd >> 8) == 0x3A) sprintf(str_out, "xorlw 0x%X", cmd & 0xFF);
    else sprintf(str_out, "unknown command");
    return str_out;
}

void initVar() {
    int i;
    for (i = 0; i < 0x200; i++) {
        variables[i].name = 0;
        variables[i].display = 0;
    }
    variables[0].name = "INDF";
    variables[1].name = "TMR0";
    variables[2].name = "PCL";
    variables[3].name = "STATUS";
    variables[4].name = "FSR";
    variables[5].name = "PORTA";
    variables[6].name = "PORTB";
    variables[7].name = "PORTC";
    variables[8].name = "PORTD";
    variables[9].name = "PORTE";
    variables[10].name = "PCLATH";
    variables[11].name = "INTCON";
    variables[12].name = "PIR1";
    variables[13].name = "PIR2";
    variables[14].name = "TMR1L";
    variables[15].name = "TMR1H";
    variables[16].name = "T1CON";
    variables[17].name = "TMR2";
    variables[18].name = "T2CON";
    variables[19].name = "SSPBUF";
    variables[20].name = "SSPCON";
    variables[21].name = "CCPR1L";
    variables[22].name = "CCPR1H";
    variables[23].name = "CCP1CON";
    variables[24].name = "RCSTA";
    variables[25].name = "TXREG";
    variables[26].name = "RCREG";
    variables[27].name = "CCPR2L";
    variables[28].name = "CCPR2H";
    variables[29].name = "CCP2CON";
    variables[30].name = "ADRESH";
    variables[31].name = "ADCON0";
    variables[0x6B].name = "DEBUG_VAR1";
    variables[0x6C].name = "DEBUG_VAR2";
    variables[0x6D].name = "DEBUG_VAR3";
    variables[0x6E].name = "DEBUG_VAR4";
    variables[0x6F].name = "DEBUG_VAR5";
    variables[0x70].name = "DEBUG_VAR6";
    variables[0x71].name = "DEBUG_VAR7";
    variables[0x72].name = "DEBUG_VAR8";
    variables[0x80].name = "INDF";
    variables[0x81].name = "OPTION_REG";
    variables[0x82].name = "PCL";
    variables[0x83].name = "STATUS";
    variables[0x84].name = "FSR";
    variables[0x85].name = "TRISA";
    variables[0x86].name = "TRISB";
    variables[0x87].name = "TRISC";
    variables[0x88].name = "TRISD";
    variables[0x89].name = "TRISE";
    variables[0x8A].name = "PCLATH";
    variables[0x8B].name = "INTCON";
    variables[0x8C].name = "PIE1";
    variables[0x8D].name = "PIE2";
    variables[0x8E].name = "PCON";
    variables[0x91].name = "SSPCON2";
    variables[0x92].name = "PR2";
    variables[0x93].name = "SSPADD";
    variables[0x94].name = "SSPSTAT";
    variables[0x98].name = "TXSTA";
    variables[0x99].name = "SPBRG";
    variables[0x9E].name = "ADRESL";
    variables[0x9F].name = "ADCON1";
    variables[0x100].name = "INDF";
    variables[0x101].name = "TMR0";
    variables[0x102].name = "PCL";
    variables[0x103].name = "STATUS";
    variables[0x104].name = "FSR";
    variables[0x106].name = "PORTB";
    variables[0x10A].name = "PCLATH";
    variables[0x10B].name = "INTCON";
    variables[0x10C].name = "EEDATA";
    variables[0x10D].name = "EEADR";
    variables[0x10E].name = "EEDATH";
    variables[0x10F].name = "EEADRH";
    variables[0x180].name = "INDF";
    variables[0x181].name = "OPTION_REG";
    variables[0x182].name = "PCL";
    variables[0x183].name = "STATUS";
    variables[0x184].name = "FSR";
    variables[0x186].name = "TRISB";
    variables[0x18A].name = "PCLATH";
    variables[0x18B].name = "INTCON";
    variables[0x18C].name = "EECON1";
    variables[0x18D].name = "EECON2";
}

/* ====================================================================
 * GUI 관련 함수들 - gui_icd_* 콜백 호출
 * ==================================================================== */

void PrintMessageICD(const char *msg) {
    gui_icd_print_status(msg);
}

void AppendMessageICD(const char *msg) {
    gui_icd_append_status(msg);
}

void scrollToLine(int line) {
    gui_icd_scroll_to_line(line);
}

void SourceHilightLine(int line) {
    gui_icd_hilight_line(line);
}

void SourceRemoveHilightLine(int line) {
    gui_icd_remove_hilight(line);
}

int loadSource(FILE *f) {
    (void)f;  /* wxWidgets 측에서 직접 파일 처리 */
    return 0;
}

/* ---- wxWidgets 측에서 호출하는 loadSource variant ---- */
int loadSourceByIdx(int src_file_idx) {
    return gui_icd_load_source(src_file_idx);
}

/* ====================================================================
 * ShowBank, ShowContext - 원본 icd.c에서 그대로 복사하되
 * gtk_check_menu_item_get_active → gui_icd_menu_is_checked 로 교체
 * ==================================================================== */

void ShowBank(int bank, char *status) {
    if (bank > 3) bank = 3;
    if (bank < 0) bank = 0;
    int b[128];
    char temp[128];
    int i;
    sprintf(temp, "bank %d:", bank);
    strcat(status, temp);
    ReadRegisterN(bank * 0x80, 128, b);
    for (i = 0; i < 128; i++) {
        if (i % 16 == 0) {
            sprintf(temp, "\n0x%03X:", i + bank * 0x80);
            strcat(status, temp);
        }
        sprintf(temp, "%02X", b[i]);
        strcat(status, temp);
    }
    strcat(status, "\n");
}

void ShowContext() {
    int i, addr, data, s;
    char cmd[32] = "";
    char status[4096] = "", temp[128];
    addr = ((ReadRegister(0x18E) & 0x1F) << 8) + ReadRegister(0x18F);
    data = ReadProgMem(addr);
    s = ReadRegister(status_temp);
    s = (s >> 4) + ((s << 4) & 0xF0);
#ifdef DEBUG
    addr = addrDebug;
    s = statusDebug;
    if (UseCoff) data = coff_data[addr];
#endif
    if (gui_icd_menu_is_checked(ICD_MENU_PC)) {
        sprintf(temp, "Next: %s (0x%04X) \nPC=0x%04X\n",
                decodeCmd(data, cmd, (s & 0x60) << 2), data, addr);
        strcat(status, temp);
    }
    if (gui_icd_menu_is_checked(ICD_MENU_STAT)) {
        sprintf(temp, "STATUS=0x%02X (", s);
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x80 ? "IRP" : "   ");
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x40 ? "RP1" : "   ");
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x20 ? "RP0" : "   ");
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x10 ? "TO" : "  ");
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x8 ? "PD" : "  ");
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x4 ? "Z" : " ");
        strcat(status, temp);
        sprintf(temp, "%s ", s & 0x2 ? "DC" : "  ");
        strcat(status, temp);
        sprintf(temp, "%s)\n", s & 0x1 ? "C" : " ");
        strcat(status, temp);
        sprintf(temp, "W=0x%02X PCLATH=0x%02X FSR=0x%02X\n",
                ReadRegister(w_temp), ReadRegister(pclath_temp),
                ReadRegister(fsr_temp));
        strcat(status, temp);
    }
    if (gui_icd_menu_is_checked(ICD_MENU_BANK0)) ShowBank(0, status);
    if (gui_icd_menu_is_checked(ICD_MENU_BANK1)) ShowBank(1, status);
    if (gui_icd_menu_is_checked(ICD_MENU_BANK2)) ShowBank(2, status);
    if (gui_icd_menu_is_checked(ICD_MENU_BANK3)) ShowBank(3, status);

    int rawsource = 0;
    if (UseCoff) {
        if (data != coff_data[addr]) {
            sprintf(temp, "code at 0x%04X (0x%04X) differs from coff (0x%04X)\n",
                    addr, data, coff_data[addr]);
            strcat(status, temp);
            rawsource = 1;
        }
#ifndef DEBUG
        else {
#endif
            if (source_info[addr].src_file != -1) {
                if (currentSource == source_info[addr].src_file)
                    scrollToLine(source_info[addr].src_line);
                else if (loadSourceByIdx(source_info[addr].src_file)) {
                    scrollToLine(source_info[addr].src_line);
                    currentSource = source_info[addr].src_file;
                } else rawsource = 1;
            } else rawsource = 1;
#ifndef DEBUG
        }
#endif
    }
    if (!UseCoff || rawsource == 1) {
#define LINES_BEFORE 5
#define LINES_AFTER 7
#define NLINES (LINES_BEFORE + LINES_AFTER)
        int addr0, addr1, line_pc = 0;
        char tmp[64 * NLINES], t2[64];
        tmp[0] = 0;
        int progmem[NLINES];
        addr0 = addr - LINES_BEFORE < 0 ? 0 : addr - LINES_BEFORE;
        addr1 = addr + LINES_AFTER > 0x1FFF ? 0x1FFF : addr + LINES_AFTER;
        ReadProgMemN(addr0, addr1 - addr0, progmem);
        for (i = addr0; i < addr1; i++) {
            sprintf(t2, "0x%04X: %s (0x%04X)\n", i,
                    decodeCmd(progmem[i - addr0], cmd, (s & 0x60) << 2),
                    progmem[i - addr0]);
            strcat(tmp, t2);
            if (i == addr) line_pc = i;
        }
        gui_icd_set_source(tmp);
        currentSource = -1;
        scrollToLine(line_pc - addr0 + 1);
    }
    for (i = 0; i < nwatch; i++) {
        sprintf(temp, "%s=0x%02X\n", watch[i].name,
                ReadRegister(watch[i].value));
        strcat(status, temp);
    }
    PrintMessageICD(status);
    if (gui_icd_menu_is_checked(ICD_MENU_EE)) {
        unsigned char edata[256];
        str[0] = 0;
        char ss[64], t[9], *g;
        t[8] = 0;
        ReadDataMemN(0, 256, edata);
        strcat(str, "EEPROM:\n");
        for (i = 0; i < 0x100; i++) {
            if (i % 8 == 0) {
                sprintf(ss, "\n0x%02X: ", i);
                strcat(str, ss);
            }
            sprintf(ss, "%02X ", edata[i]);
            strcat(str, ss);
            t[i & 0x7] = isprint(edata[i]) ? edata[i] : '.';
            if (i % 8 == 7) {
                g = g_locale_to_utf8(t, -1, NULL, NULL, NULL);
                if (g) { strcat(str, g); g_free(g); }
            }
        }
        AppendMessageICD(str);
    }
}

/* ====================================================================
 * addWatch, removeWatch, executeCommand - 원본 icd.c에서 그대로 복사
 * ==================================================================== */

int addWatch(struct symbol s) {
    int i;
    for (i = 0; i < nwatch && strcmp(watch[i].name, s.name); i++);
    if (i < nwatch) {
        for (; i < nwatch - 1; i++) {
            watch[i].name = watch[i + 1].name;
            watch[i].value = watch[i + 1].value;
        }
        nwatch--;
        watch = realloc(watch, nwatch * sizeof(struct symbol));
        return 0;
    } else {
        nwatch++;
        watch = realloc(watch, nwatch * sizeof(struct symbol));
        watch[nwatch - 1].name = s.name;
        watch[nwatch - 1].value = s.value;
        return 1;
    }
}

int removeWatch(char *name) {
    int i;
    for (i = 0; i < nwatch && strcmp(watch[i].name, name); i++);
    if (i < nwatch) {
        for (; i < nwatch - 1; i++) {
            watch[i].name = watch[i + 1].name;
            watch[i].value = watch[i + 1].value;
        }
        nwatch--;
        watch = realloc(watch, nwatch * sizeof(struct symbol));
        return 1;
    }
    return 0;
}

/* executeCommand - 원본과 동일. 다만 ICDHelp/icdRun/icdHalt/icdStepOver
 * 호출은 wxWidgets 측의 extern 함수로 forward. */
extern void wx_icd_run(void);
extern void wx_icd_halt(void);
extern void wx_icd_step(void);
extern void wx_icd_step_over(void);
extern void wx_icd_help(void);

int executeCommand(char *command) {
    /* break */
    if (strstr(command, "break")) {
        if (sscanf(command, "break %x", &break_addr) == 1) {
            if (running) Halt();
            break_addr &= 0x1FFF;
            sprintf(str, "break at address 0x%04X\n", break_addr);
            AppendMessageICD(str);
            SourceRemoveHilightLine(sourceHilight);
            SourceHilightLine(source_info[break_addr].src_line);
            sourceHilight = source_info[break_addr].src_line;
        }
    }
    /* clear */
    if (strstr(command, "clear")) {
        PrintMessageICD("");
    }
    /* freeze */
    else if (strstr(command, "freeze")) {
        char option[32];
        if (sscanf(command, "freeze %s", option) == 1) {
            if (running) Halt();
            if (!strcmp(option, "on")) freeze = 1;
            if (!strcmp(option, "off")) freeze = 0;
            WriteRegister(0x18E, (break_addr >> 8) + (freeze ? 0x40 : 0));
        }
        sprintf(str, "peripheral freeze is %s\n", freeze ? "on" : "off");
        AppendMessageICD(str);
    }
    /* halt */
    else if (!strcmp(command, "h") || !strcmp(command, "halt")) {
        wx_icd_halt();
    }
    /* help */
    else if (!strcmp(command, "help")) {
        wx_icd_help();
    }
    /* list */
    else if (strstr(command, "list ")) {
#define LISTLINES 10
        int addr_l, i;
        char tmp[32 * LISTLINES], t2[32], cmd_l[32] = "";
        tmp[0] = 0;
        int progmem[LISTLINES];
        if (sscanf(command, "list %x", &addr_l) == 1) {
            addr_l &= 0x1FFF;
            ReadProgMemN(addr_l, LISTLINES, progmem);
            for (i = 0; i < LISTLINES; i++) {
                sprintf(t2, "0x%04X: %s (0x%04X)\n", i + addr_l,
                        decodeCmd(progmem[i], cmd_l, 0), progmem[i]);
                strcat(tmp, t2);
            }
            AppendMessageICD(tmp);
        }
    }
    /* print */
    else if (strstr(command, "print ") || strstr(command, "p ")) {
        int bank, i, addr_p, data;
        char var[128];
        if (strstr(command, "print p") || strstr(command, "p p")) {
            int addr2;
            if (sscanf(command, "print p %x", &addr2) == 1 ||
                sscanf(command, "p p %x", &addr2) == 1) {
                addr2 &= 0x1FFF;
                if (running) Halt();
                data = ReadProgMem(addr2);
                sprintf(str, "0x%04X: %s (0x%04X)\n", addr2,
                        decodeCmd(data, var, 0x1000), data);
                AppendMessageICD(str);
            }
        } else if (!strcmp(command, "print ee") || !strcmp(command, "p ee")) {
            unsigned char edata[256];
            str[0] = 0;
            char ss[64], t[9], *g;
            t[8] = 0;
            if (running) Halt();
            ReadDataMemN(0, 256, edata);
            sprintf(str, "EEPROM:\n");
            for (i = 0; i < 0x100; i++) {
                if (i % 8 == 0) {
                    sprintf(ss, "\n0x%02X: ", i);
                    strcat(str, ss);
                }
                sprintf(ss, "%02X ", edata[i]);
                strcat(str, ss);
                t[i & 0x7] = isprint(edata[i]) ? edata[i] : '.';
                if (i % 8 == 7) {
                    g = g_locale_to_utf8(t, -1, NULL, NULL, NULL);
                    if (g) { strcat(str, g); g_free(g); }
                }
            }
            strcat(str, "\n");
            AppendMessageICD(str);
        } else if (sscanf(command, "print ee %x", &addr_p) == 1 ||
                   sscanf(command, "p ee %x", &addr_p) == 1) {
            addr_p &= 0xFF;
            if (running) Halt();
            data = ReadDataMem(addr_p);
            sprintf(str, "eeprom memory at 0x%02X=0x%02X (%c)\n",
                    addr_p, data, isprint(data) ? data : '.');
            AppendMessageICD(str);
        } else if (sscanf(command, "print bank %x", &bank) == 1 ||
                   sscanf(command, "p bank %x", &bank) == 1) {
            str[0] = 0;
            bank &= 0x1FF;
            if (bank > 3) bank /= 0x80;
            if (running) Halt();
            ShowBank(bank, str);
            AppendMessageICD(str);
        } else if (sscanf(command, "print 0x%x", &print_addr) == 1 ||
                   sscanf(command, "p 0x%x", &print_addr) == 1) {
            print_addr &= 0x1FF;
            if (running) Halt();
            sprintf(str, "[0x%03X]=0x%02X\n", print_addr,
                    ReadRegister(print_addr));
            AppendMessageICD(str);
        } else if (sscanf(command, "print %s", var) == 1 ||
                   sscanf(command, "p %s", var) == 1) {
            str[0] = 0;
            if (running) Halt();
            if (!strcmp("W", var) || !strcmp("w", var))
                sprintf(str, "W = 0x%02X\n", ReadRegister(w_temp));
            else if (!strcmp("STATUS", var))
                sprintf(str, "0x003: STATUS = 0x%02X\n", ReadRegister(status_temp));
            else if (!strcmp("FSR", var))
                sprintf(str, "0x004: FSR = 0x%02X\n", ReadRegister(fsr_temp));
            else if (!strcmp("PCLATH", var))
                sprintf(str, "0x00A: PCLATH = 0x%02X\n", ReadRegister(pclath_temp));
            else {
                for (i = 0; i < nsym && strcmp(var, sym[i].name); i++);
                if (i < nsym) {
                    sprintf(str, "0x%03X: %s = 0x%02X\n",
                            sym[i].value, sym[i].name, ReadRegister(sym[i].value));
                } else {
                    for (i = 0; i < 0x200; i++) {
                        if (variables[i].name && !strcmp(var, variables[i].name)) {
                            sprintf(str, "0x%03X: %s = 0x%02X\n",
                                    i, variables[i].name, ReadRegister(i));
                            i = 0x200;
                        }
                    }
                }
            }
            AppendMessageICD(str);
        }
    }
    /* run */
    else if (!strcmp(command, "r") || !strcmp(command, "run")) {
        wx_icd_run();
    }
    /* step */
    else if (!strcmp(command, "s") || !strcmp(command, "step") ||
             strstr(command, "step") || strstr(command, "s ")) {
        int i, n = 1;
        sscanf(command, "step %d", &n);
        sscanf(command, "s %d", &n);
#ifdef DEBUG
        addrDebug += n;
#endif
        if (running) Halt();
        for (i = 0; i < n; i++) step();
        if (n > 1) sprintf(str, "step %d\n", n);
        else str[0] = 0;
        ShowContext();
        AppendMessageICD(str);
    }
    /* step over */
    else if (!strcmp(command, "ss") || !strcmp(command, "step over") ||
             strstr(command, "step over ") || strstr(command, "ss ")) {
        int i, n = 1;
        sscanf(command, "step over %d", &n);
        sscanf(command, "ss %d", &n);
#ifdef DEBUG
        addrDebug += n;
#endif
        for (i = 0; i < n; i++) wx_icd_step_over();
        if (n > 1) sprintf(str, "step over %d\n", n);
        AppendMessageICD(str);
    }
    /* version */
    else if (!strcmp(command, "ver") || !strcmp(command, "version")) {
        if (running) Halt();
        sprintf(str, "debugger version: %.1f\n", version() / 10.0);
        AppendMessageICD(str);
    }
    /* watch */
    else if (strstr(command, "watch ") || strstr(command, "w ")) {
        int i, var_addr;
        char var[64];
        if (sscanf(command, "watch 0x%x", &var_addr) ||
            sscanf(command, "w 0x%x", &var_addr)) {
            struct symbol s;
            sprintf(var, "[0x%X]", var_addr);
            s.name = strdup(var);
            s.value = var_addr;
            addWatch(s);
            if (!running) ShowContext();
        } else if (sscanf(command, "watch %s", var) ||
                   sscanf(command, "w %s", var)) {
            for (i = 0; i < nsym && strcmp(var, sym[i].name); i++);
            if (i < nsym) {
                addWatch(sym[i]);
                if (!running) ShowContext();
            } else {
                for (i = 0; i < 0x200; i++) {
                    if (variables[i].name && !strcmp(var, variables[i].name)) {
                        struct symbol s;
                        s.name = variables[i].name;
                        s.value = i;
                        addWatch(s);
                        if (!running) ShowContext();
                        i = 0x200;
                    }
                }
            }
        }
    }
    /* set variable */
    else {
        char var[64], *p;
        int data, i, addr = -1;
        if ((p = strchr(command, '='))) {
            *p = 0;
            if (sscanf(command, "[%x]", &addr) && sscanf(p + 1, "%x", &data)) {
                if (running) Halt();
                WriteRegister(addr, data);
                ShowContext();
                sprintf(str, "[0x%x]=0x%02X\n", addr, data);
                AppendMessageICD(str);
            } else if (sscanf(command, "%s", var) && sscanf(p + 1, "%x", &data)) {
                for (i = 0; i < nsym && strcmp(var, sym[i].name); i++);
                if (i < nsym && sym[i].value < 0x400) {
                    addr = sym[i].value;
                } else {
                    for (i = 0; i < 0x200; i++) {
                        if (variables[i].name && !strcmp(var, variables[i].name)) {
                            addr = i;
                            i = 0x200;
                        }
                    }
                }
                if (addr != -1) {
                    if (running) Halt();
                    WriteRegister(addr, data);
                    ShowContext();
                    sprintf(str, "%s=0x%02X\n", var, data);
                    AppendMessageICD(str);
                }
            }
        } else return 0;
    }
    return 1;
}
