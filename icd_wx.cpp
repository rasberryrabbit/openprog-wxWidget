/**
 * \file icd_wx.cpp
 * \brief ICD (In-Circuit Debugger) 탭 - wxWidgets GUI 구현.
 *
 * icd_core.c에서 호출하는 gui_icd_* 콜백 함수들을 구현.
 * 또한 icdRun/icdHalt/icdStep/icdStepOver/icdStop/icdRefresh/icdCheck
 * ICD 제어 함수들도 본 파일에서 구현 (원본 icd.c에서 GTK 부분만 교체).
 */

#include <wx/wx.h>
#include <wx/textctrl.h>
#include <wx/menu.h>
#include <wx/timer.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/panel.h>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/splitter.h>
#include <wx/notebook.h>

#include <cstring>
#include <cstdio>
#include <cstdlib>

extern "C" {
#include "common.h"
#include "coff.h"
#include "icd.h"
/* deviceRW.h 의 family_t / group_t */
#include "deviceRW.h"
}

/* Tck는 icd_core.c에 #define 되어 있으나 헤더에 없으므로 여기서 정의 */
#ifndef Tck
#define Tck 30
#endif

/* common.h에 extern 으로 선언된 변수들 - hw_io.c에서 정의됨 */
extern int DeviceDetected;
extern int saveLog;

/* icd_wx.cpp 에서 PrintMessageICD/AppendMessageICD/scrollToLine 등은
 * icd_core.c에 정의됨. forward 선언 */
extern "C" void PrintMessageICD(const char *msg);
extern "C" void AppendMessageICD(const char *msg);
extern "C" void scrollToLine(int line);

/* OpenLogFile, WriteLogIO, msDelay - fileIO.c, hw_io.c */
extern "C" void OpenLogFile(void);
extern "C" void WriteLogIO();
extern "C" void msDelay(double delay);

/* icd_core.c에서 제공하는 함수 선언 */
extern "C" {
    void startICD(int tck);
    int  isRunning();
    void cont(int break_addr, int freeze);
    void step();
    void run();
    int  version();
    void Halt();
    int  ReadRegister(int addr);
    void WriteRegister(int addr, int data);
    int  ReadProgMem(int addr);
    int  ReadProgMemN(int addr, int n, int *buf);
    int  ReadDataMem(int addr);
    int  ReadDataMemN(int addr, int n, unsigned char *buf);
    void ShowContext();
    int  executeCommand(char *command);
    void initVar();
    int  addWatch(struct symbol s);
    int  removeWatch(char *name);
    char *decodeCmd(int cmd, char *str, int addrH);
    void PrintMessageICD(const char *msg);
    void AppendMessageICD(const char *msg);
    void scrollToLine(int line);
    void SourceHilightLine(int line);
    void SourceRemoveHilightLine(int line);

    /* icd_core.c 전역 변수 */
    extern int break_addr, print_addr;
    extern int currentSource, sourceHilight;
    extern char lastCmd[64];
    extern int UseCoff;
    extern struct src_i source_info[];
    extern struct srcfile *s_files;
    extern struct symbol *sym;
    extern int nsym;
    extern struct symbol *watch;
    extern int nwatch;
    extern int ver, reset, freeze, icdConnected, running;
    extern double Tcom;
    extern int icdTimer;
    extern char **Slabel;
    extern char **Sulabel;
    extern unsigned short coff_data[];

    /* coff.c에서 제공 - coff.h에 선언되어 있으므로 extern 생략 */
}

/* ============================================================
 *   wxWidgets 측 전역 포인터 - icd_core.c gui_icd_* 콜백이 사용
 * ============================================================ */

static wxTextCtrl *g_icdStatus = nullptr;   /* ICD 상태 표시 */
static wxTextCtrl *g_icdSource = nullptr;   /* ICD 소스 코드 표시 */
static wxTextCtrl *g_icdCommand = nullptr;  /* 명령 입력 */
static wxMenuBar  *g_icdMenuBar = nullptr;  /* 메뉴 (체크 항목용) */
static wxTimer    *g_icdTimer = nullptr;
static wxWindow   *g_icdParent = nullptr;

/* 메뉴 항목 ID */
enum IcdMenuId {
    ID_ICD_MENU_PC = 13001,
    ID_ICD_MENU_STAT,
    ID_ICD_MENU_BANK0,
    ID_ICD_MENU_BANK1,
    ID_ICD_MENU_BANK2,
    ID_ICD_MENU_BANK3,
    ID_ICD_MENU_EE,
    ID_ICD_RUN,
    ID_ICD_HALT,
    ID_ICD_STEP,
    ID_ICD_STEPOVER,
    ID_ICD_STOP,
    ID_ICD_REFRESH,
    ID_ICD_LOAD_COFF,
    ID_ICD_HELP,
    ID_ICD_CMD
};

/* ============================================================
 *   gui_icd_* 콜백 함수들 - icd_core.c에서 extern 호출
 * ============================================================ */

extern "C" {

void gui_icd_print_status(const char *msg) {
    if (g_icdStatus) {
        g_icdStatus->Clear();
        g_icdStatus->AppendText(wxString::FromUTF8(msg));
        g_icdStatus->ShowPosition(0);
    }
    if (wxTheApp) wxTheApp->Yield();
}

void gui_icd_append_status(const char *msg) {
    if (g_icdStatus) {
        g_icdStatus->AppendText(wxString::FromUTF8(msg));
        g_icdStatus->ShowPosition(g_icdStatus->GetLastPosition());
    }
    if (wxTheApp) wxTheApp->Yield();
}

void gui_icd_set_source(const char *text) {
    if (g_icdSource) {
        g_icdSource->Clear();
        g_icdSource->AppendText(wxString::FromUTF8(text));
    }
    if (wxTheApp) wxTheApp->Yield();
}

void gui_icd_scroll_to_line(int line) {
    if (g_icdSource && line > 0) {
        /* wxTextCtrl은 라인 번호 0-base. line은 1-base로 들어옴. */
        long pos = g_icdSource->XYToPosition(0, line - 1);
        if (pos >= 0) {
            g_icdSource->SetSelection(pos, pos);
            g_icdSource->ShowPosition(pos);
        }
    }
    if (wxTheApp) wxTheApp->Yield();
}

void gui_icd_hilight_line(int line) {
    /* wxTextCtrl은 색상 태그를 직접 지원하지 않음.
     * 간단히 해당 라인을 선택하는 것으로 대체. */
    if (g_icdSource && line > 0) {
        long pos = g_icdSource->XYToPosition(0, line - 1);
        long endPos = g_icdSource->XYToPosition(0, line);
        if (pos >= 0) {
            if (endPos < 0) endPos = g_icdSource->GetLastPosition();
            g_icdSource->SetSelection(pos, endPos);
            g_icdSource->ShowPosition(pos);
        }
    }
    if (wxTheApp) wxTheApp->Yield();
}

void gui_icd_remove_hilight(int line) {
    (void)line;
    /* 단순히 선택 해제 */
    if (g_icdSource) {
        long pos = g_icdSource->GetInsertionPoint();
        g_icdSource->SetSelection(pos, pos);
    }
    if (wxTheApp) wxTheApp->Yield();
}

int gui_icd_load_source(int src_file_idx) {
    if (!s_files || src_file_idx < 0) return 0;
    FILE *f = s_files[src_file_idx].ptr;
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) return 0;
    char *buf = (char *)malloc(sz + 1);
    if (!buf) return 0;
    sz = fread(buf, 1, sz, f);
    buf[sz] = 0;
    if (g_icdSource) {
        g_icdSource->Clear();
        g_icdSource->AppendText(wxString::From8BitData(buf));
    }
    free(buf);
    if (wxTheApp) wxTheApp->Yield();
    return 1;
}

int gui_icd_menu_is_checked(int menu_id) {
    if (!g_icdMenuBar) return 0;
    wxMenu *menu = nullptr;
    /* "View" 메뉴 (인덱스 1) 에서 검색 */
    menu = g_icdMenuBar->GetMenu(1);
    if (!menu) return 0;
    wxMenuItem *item = menu->FindItem(menu_id);
    if (item && item->IsCheckable()) {
        return item->IsChecked() ? 1 : 0;
    }
    return 0;
}

void gui_icd_start_timer(int ms) {
    if (g_icdTimer) {
        g_icdTimer->Stop();
        g_icdTimer->Start(ms);
    }
}

void gui_icd_stop_timer(void) {
    if (g_icdTimer) g_icdTimer->Stop();
}

int gui_icd_load_coff(const char *filename) {
    /* analyzeCOFF는 char*를 받으므로 캐스트 (수정하지 않음) */
    UseCoff = analyzeCOFF((char *)filename, Slabel, Sulabel, source_info,
                          &s_files, coff_data, &sym, &nsym);
    return UseCoff ? 1 : 0;
}

} /* extern "C" */

/* ============================================================
 *   ICD 제어 함수 - 원본 icd.c에서 GTK 부분만 교체
 *   executeCommand 에서 wx_icd_* 이름으로 호출됨
 * ============================================================ */

extern "C" void wx_icd_run(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    if (!icdConnected) {
        saveLog = 0;  /* b_log 체크박스 - wxWidgets 측에서 연결 필요 */
        if (saveLog) {
            OpenLogFile();
            fprintf(logfile, "ICD start\n");
        }
        startICD(Tck);
        run();
        icdConnected = 1;
        gui_icd_start_timer(20);
        PrintMessageICD("running");
    } else if (!running) {
        cont(break_addr, freeze);
        gui_icd_start_timer(20);
        PrintMessageICD("running");
    }
}

extern "C" void wx_icd_halt(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    if (running) {
        gui_icd_stop_timer();
        Halt();
        ShowContext();
    }
}

extern "C" void wx_icd_step(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    if (running) {
        gui_icd_stop_timer();
        Halt();
    }
    step();
    ShowContext();
}

extern "C" void wx_icd_step_over(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    int addr, data;
    if (running) {
        gui_icd_stop_timer();
        Halt();
    }
    addr = ((ReadRegister(0x18E) & 0x1F) << 8) + ReadRegister(0x18F);
    data = ReadProgMem(addr);
    if ((data >> 11) == 4) {
        cont(addr + 1, freeze);
        gui_icd_start_timer(20);
    } else {
        step();
        ShowContext();
    }
}

extern "C" void wx_icd_help(void) {
    if (g_icdParent) {
        const char *help_txt =
            "ICD Commands:\n"
            "  break <addr>     - set breakpoint\n"
            "  clear            - clear status\n"
            "  freeze on|off    - peripheral freeze control\n"
            "  h|halt           - halt execution\n"
            "  help             - this help\n"
            "  list <addr>      - list 10 instructions from addr\n"
            "  print <var>      - print variable/register\n"
            "  print p <addr>   - print program memory\n"
            "  print ee [addr]  - print EEPROM\n"
            "  print bank <n>   - print memory bank 0-3\n"
            "  r|run            - run program\n"
            "  s|step [n]       - step n instructions\n"
            "  ss|step over [n] - step over calls\n"
            "  ver              - debugger version\n"
            "  watch <name>     - add watch\n\n"
            "Function keys:\n"
            "  F1=Help  F5=Halt  F7=Step  F8=StepOver  F9=Run";
        wxMessageDialog dlg(g_icdParent, wxString::FromUTF8(help_txt),
                            "ICD Help", wxOK | wxICON_INFORMATION);
        dlg.ShowModal();
    }
}

/* ICD 새로고침 */
extern "C" void wx_icd_refresh(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    if (!running) ShowContext();
}

/* ICD 정지 */
extern "C" void wx_icd_stop(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    if (running) {
        gui_icd_stop_timer();
        Halt();
    }
    int j = 0;
    bufferU[j++] = EN_VPP_VCC;
    bufferU[j++] = 0x0;
    bufferU[j++] = WAIT_T3;
    bufferU[j++] = SET_CK_D;
    bufferU[j++] = 0x2;
    bufferU[j++] = FLUSH;
    for (; j < DIMBUF; j++) bufferU[j] = 0x0;
    PacketIO(3);
    if (saveLog) WriteLogIO();
    icdConnected = 0;
    PrintMessageICD("stopped");
    scrollToLine(source_info[0].src_line);
}

/* ICD check timer 콜백 */
extern "C" void wx_icd_check(void) {
#ifndef DEBUG
    if (DeviceDetected != 1) return;
#endif
    if (!isRunning()) {
        gui_icd_stop_timer();
        ShowContext();
    }
}

/* ============================================================
 *   ICD 탭 생성 함수 - OpguiFrame에서 호출
 * ============================================================ */

/* 타이머 이벤트 핸들러를 위한 helper 클래스 */
class IcdTimerHandler {
public:
    static void OnTimer(wxTimerEvent &event) {
        (void)event;
        wx_icd_check();
    }
};

/* ICD 탭을 wxNotebook에 추가.
 * parent: 부모 프레임 (메뉴 바 추가용)
 * notebook: 탭 컨테이너
 * menuBar: 메뉴 바 (View 메뉴 추가용)
 * timer: 타이머 (이벤트 루프에 연결)
 */
extern "C" void create_icd_tab(wxWindow *parent, wxNotebook *notebook,
                                wxMenuBar *menuBar, wxTimer *timer);

/* forward: OpguiFrame에서 전달받을 함수 */
extern wxTextCtrl *g_dataText;  /* opgui_wx.cpp에 정의됨 */

extern "C"
void create_icd_tab(wxWindow *parent, wxNotebook *notebook,
                    wxMenuBar *menuBar, wxTimer *timer) {
    g_icdParent = parent;
    g_icdMenuBar = menuBar;
    g_icdTimer = timer;

    wxPanel *panel = new wxPanel((wxWindow *)notebook);
    wxBoxSizer *topSizer = new wxBoxSizer(wxVERTICAL);

    /* 툴바 (버튼들) */
    wxPanel *btnPanel = new wxPanel(panel);
    wxBoxSizer *btnSizer = new wxBoxSizer(wxHORIZONTAL);

    wxButton *bRun = new wxButton(btnPanel, ID_ICD_RUN, "Run (F9)");
    wxButton *bHalt = new wxButton(btnPanel, ID_ICD_HALT, "Halt (F5)");
    wxButton *bStep = new wxButton(btnPanel, ID_ICD_STEP, "Step (F7)");
    wxButton *bStepOver = new wxButton(btnPanel, ID_ICD_STEPOVER, "StepOver (F8)");
    wxButton *bStop = new wxButton(btnPanel, ID_ICD_STOP, "Stop");
    wxButton *bRefresh = new wxButton(btnPanel, ID_ICD_REFRESH, "Refresh");
    wxButton *bLoadCoff = new wxButton(btnPanel, ID_ICD_LOAD_COFF, "Load COFF...");
    wxButton *bHelp = new wxButton(btnPanel, ID_ICD_HELP, "Help");

    btnSizer->Add(bRun, 0, wxALL, 2);
    btnSizer->Add(bHalt, 0, wxALL, 2);
    btnSizer->Add(bStep, 0, wxALL, 2);
    btnSizer->Add(bStepOver, 0, wxALL, 2);
    btnSizer->Add(bStop, 0, wxALL, 2);
    btnSizer->Add(bRefresh, 0, wxALL, 2);
    btnSizer->Add(bLoadCoff, 0, wxALL, 2);
    btnSizer->Add(bHelp, 0, wxALL, 2);
    btnSizer->AddStretchSpacer(1);
    btnPanel->SetSizer(btnSizer);
    topSizer->Add(btnPanel, 0, wxEXPAND | wxALL, 2);

    /* 소스 + 상태 분할 */
    wxSplitterWindow *splitter = new wxSplitterWindow(panel, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxSP_LIVE_UPDATE);
    splitter->SetSashGravity(0.6);
    splitter->SetMinimumPaneSize(80);

    wxPanel *srcPanel = new wxPanel(splitter);
    wxBoxSizer *srcSizer = new wxBoxSizer(wxVERTICAL);
    srcSizer->Add(new wxStaticText(srcPanel, wxID_ANY, "Source:"),
                  0, wxALL, 2);
    g_icdSource = new wxTextCtrl(srcPanel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxBORDER_SUNKEN);
    wxFont monoFont(wxFontInfo(10).Family(wxFONTFAMILY_MODERN));
    g_icdSource->SetFont(monoFont);
    srcSizer->Add(g_icdSource, 1, wxEXPAND | wxALL, 2);
    srcPanel->SetSizer(srcSizer);

    wxPanel *statPanel = new wxPanel(splitter);
    wxBoxSizer *statSizer = new wxBoxSizer(wxVERTICAL);
    statSizer->Add(new wxStaticText(statPanel, wxID_ANY, "Status:"),
                   0, wxALL, 2);
    g_icdStatus = new wxTextCtrl(statPanel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP | wxBORDER_SUNKEN);
    g_icdStatus->SetFont(monoFont);
    statSizer->Add(g_icdStatus, 1, wxEXPAND | wxALL, 2);
    statPanel->SetSizer(statSizer);

    splitter->SplitHorizontally(srcPanel, statPanel, 300);
    topSizer->Add(splitter, 1, wxEXPAND | wxALL, 2);

    /* 명령 입력 */
    wxPanel *cmdPanel = new wxPanel(panel);
    wxBoxSizer *cmdSizer = new wxBoxSizer(wxHORIZONTAL);
    cmdSizer->Add(new wxStaticText(cmdPanel, wxID_ANY, "Command:"),
                  0, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    g_icdCommand = new wxTextCtrl(cmdPanel, ID_ICD_CMD, "",
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    cmdSizer->Add(g_icdCommand, 1, wxEXPAND | wxALL, 4);
    cmdPanel->SetSizer(cmdSizer);
    topSizer->Add(cmdPanel, 0, wxEXPAND | wxALL, 2);

    panel->SetSizer(topSizer);

    notebook->AddPage(panel, "ICD");

    /* View 메뉴 추가 (체크 항목들) */
    if (menuBar && menuBar->GetMenuCount() >= 2) {
        wxMenu *viewMenu = menuBar->GetMenu(1);
        /* 이미 View 메뉴가 있으면 그 아래에 추가. 없으면 새로 만들기 */
        viewMenu->AppendSeparator();
        viewMenu->AppendCheckItem(ID_ICD_MENU_PC, "Show PC\tShift-P");
        viewMenu->AppendCheckItem(ID_ICD_MENU_STAT, "Show STATUS\tShift-S");
        viewMenu->AppendSeparator();
        viewMenu->AppendCheckItem(ID_ICD_MENU_BANK0, "Show Bank 0");
        viewMenu->AppendCheckItem(ID_ICD_MENU_BANK1, "Show Bank 1");
        viewMenu->AppendCheckItem(ID_ICD_MENU_BANK2, "Show Bank 2");
        viewMenu->AppendCheckItem(ID_ICD_MENU_BANK3, "Show Bank 3");
        viewMenu->AppendSeparator();
        viewMenu->AppendCheckItem(ID_ICD_MENU_EE, "Show EEPROM");

        /* 기본값: PC, STAT 체크 */
        viewMenu->Check(ID_ICD_MENU_PC, true);
        viewMenu->Check(ID_ICD_MENU_STAT, true);
    }

    /* 변수 테이블 초기화 */
    initVar();
}

/* ============================================================
 *   ICD 탭 이벤트 핸들러들
 *   OpguiFrame의 wxBEGIN_EVENT_TABLE에서 Bind 해야 함.
 *   본 파일에서는 별도 클래스로 정의하지 않고, 외부에서
 *   Bind 할 수 있도록 함수 포인터 래퍼 제공.
 * ============================================================ */

extern "C" {

void on_icd_run(wxCommandEvent &e) { (void)e; wx_icd_run(); }
void on_icd_halt(wxCommandEvent &e) { (void)e; wx_icd_halt(); }
void on_icd_step(wxCommandEvent &e) { (void)e; wx_icd_step(); }
void on_icd_stepover(wxCommandEvent &e) { (void)e; wx_icd_step_over(); }
void on_icd_stop(wxCommandEvent &e) { (void)e; wx_icd_stop(); }
void on_icd_refresh(wxCommandEvent &e) { (void)e; wx_icd_refresh(); }

void on_icd_help(wxCommandEvent &e) { (void)e; wx_icd_help(); }

void on_icd_load_coff(wxCommandEvent &e) {
    (void)e;
    wxFileDialog dlg(g_icdParent, "Open COFF file", "", "",
        "COFF files (*.cof;*.out)|*.cof;*.out|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        wxString path = dlg.GetPath();
        if (gui_icd_load_coff(path.mb_str(wxConvUTF8))) {
            gui_icd_print_status("COFF file loaded");
            if (source_info[0].src_file != -1) {
                if (currentSource == source_info[0].src_file) {
                    scrollToLine(source_info[0].src_line);
                } else if (gui_icd_load_source(source_info[0].src_file)) {
                    scrollToLine(source_info[0].src_line);
                    currentSource = source_info[0].src_file;
                }
            }
        } else {
            wxMessageDialog msg(g_icdParent, "Failed to load COFF file",
                                "Error", wxOK | wxICON_ERROR);
            msg.ShowModal();
        }
    }
}

void on_icd_command_enter(wxCommandEvent &e) {
    (void)e;
    if (!g_icdCommand) return;
    wxString cmd = g_icdCommand->GetValue();
    char s[256];
    strncpy(s, cmd.mb_str(wxConvUTF8), sizeof(s) - 1);
    s[sizeof(s) - 1] = 0;
    if (!strlen(s)) {
        strcpy(s, lastCmd);
        g_icdCommand->SetValue(wxString::FromUTF8(s));
        if (wxTheApp) wxTheApp->Yield();
        msDelay(60);
    } else {
        strncpy(lastCmd, s, sizeof(lastCmd) - 1);
        lastCmd[sizeof(lastCmd) - 1] = 0;
    }
    if (executeCommand(s)) {
        g_icdCommand->Clear();
    }
}

void on_icd_menu_view(wxCommandEvent &e) {
    (void)e;
    /* 체크 토글은 wxWidgets이 자동 처리.
     * 상태가 바뀌면 ShowContext를 호출하여 다시 표시. */
    if (!running) ShowContext();
}

void on_icd_key(wxKeyEvent &e) {
    int key = e.GetKeyCode();
    switch (key) {
        case WXK_F1: wx_icd_help(); break;
        case WXK_F5: wx_icd_halt(); break;
        case WXK_F7: wx_icd_step(); break;
        case WXK_F8: wx_icd_step_over(); break;
        case WXK_F9: wx_icd_run(); break;
        default: e.Skip(); return;
    }
}

} /* extern "C" */
