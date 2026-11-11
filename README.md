# Open Programmer GUI - wxWidgets 포팅 (v3 - 완전 포팅)

## 개요

원본 Open Programmer (OPGUI) v0.12.4 의 GTK3 GUI를 **wxWidgets 3.2** 로 포팅한
버전입니다. Linux와 Windows 두 플랫폼 모두에서 빌드되며, Windows 버전은
GTK Runtime Environment 없이 단일 .exe 파일로 동작합니다.

## v3 업데이트 (이전 버전 대비)

이전 v2에서는 일부 핸들러가 stub 상태였습니다. v3에서는 **모든 TODO가 완료**되어
원본과 기능적으로 동등한 수준에 도달했습니다:

1. **IO 핀 수동 제어** - 13개 핀 (RB0-RB7, RC7, RC6, RA5, RA4, RA3) 라디오 버튼
   (0/1/입력) + 100ms 폴링 타이머로 PORT 상태 실시간 표시 + VPP 전압 표시.
2. **VPP/VDD/DCDC 전원 제어** - 체크박스 토글 + DCDC 전압 슬라이더 (5.0-15.0V).
3. **PIC Config Words** - CW1-CW7 hex entry + ConfigForce 체크박스. PIC12/16/18
   계열별 주소 매핑 자동 적용 (`memCODE_W` 또는 `memCONFIG`).
4. **I2C/SPI 송수신** - 6개 모드 (I2C 8-bit, I2C 16-bit, SPI 0,0/0,1/1,0/1,1) +
   4단계 속도 + N bytes + hex 데이터 파싱.
5. **로깅 활성화** - b_log checkbox → OpenLogFile/CloseLogFile 연동.
6. **V33 체크 / S1 대기 옵션** - b_V33check, b_WaitS1 checkbox 처리.
7. **설정 파일 (opgui.ini)** - 프로그램 시작 시 device/vid/pid/max_err 로드,
   종료 시 변경이 있으면 저장. `~/.opgui/opgui.ini` 경로.
8. **명령행 모드 (-c)** - GUI 없이 USB 패킷 송수신. `opgui_wx -c 12 34 56...`
9. **PIC 옵션 체크박스** - EEPROM_RW, ReadReserved, Write_ID_BKCal, WriteCalib12,
   UseSaflock, UseOSCCAL, UseBKOSCCAL, UseFileCal 모두 getOptions에 연동.
10. **ICD 활성화 옵션** - ICDenable checkbox + ICD address entry.

## 산출물

| 파일 | 크기 | 설명 |
|---|---|---|
| `opgui_wx.exe` | ~8.6 MB | Windows x86_64 단일 실행 파일 (정적 링크) |
| `opgui_wx_linux` | ~8.8 MB | Linux x86_64 실행 파일 (테스트/개발용) |
| `opgui_wx.cpp` | ~71 KB | wxWidgets 메인 GUI |
| `icd_wx.cpp` | ~20 KB | ICD 디버거 wxWidgets GUI |
| `icd_core.c` | ~40 KB | ICD 핵심 로직 (GTK 독립) |
| `device_db.c` | ~7 KB | 디바이스 DB 접근 레이어 |
| `hw_io.c` | ~21 KB | 핵심 USB I/O + IO/전원/Config/명령행/설정파일 코어 로직 |
| `gui_shim.h` | ~11 KB | GTK → wxWidgets 추상화 레이어 |
| `common.h` | (수정됨) | GTK 헤더 대신 gui_shim.h를 include + 추가 extern 선언 |
| `Makefile` | | Linux + Windows 크로스 컴파일 타겟 |

## 포팅 전략

### 1. GUI Shim 접근법
원본 `common.h`가 `<gtk/gtk.h>`를 include 하고 `PrintStatus` 매크로가 직접
GTK 함수를 호출하는 구조이기 때문에, 코어 .c 파일들이 GTK 의존 없이
컴파일되도록 `gui_shim.h`를 작성:

- `GtkWidget*`, `GtkTextBuffer*` 등을 opaque `void*`로 typedef
- `gtk_statusbar_push()` 등을 wxWidgets 백엔드 함수를 호출하는 inline으로 재정의
- `g_free()`, `g_locale_to_utf8()` 등 glib 헬퍼도 호환 래퍼로 제공

이로써 코어 .c 파일 11개 (prog*.c, fileIO.c, deviceRW.c, I2CSPI.c, coff.c,
strings.c)가 **소스 수정 없이** wxWidgets 빌드에 참여.

### 2. 코어 로직 분리 (hw_io.c)
원본 `opgui.c`의 GTK-의존 핵심 로직을 `hw_io.c`로 추출:

- `FindDevice`, `PacketIO`, `msDelay`, `GetTickCount` - USB 통신 핵심
- `IOchangedCore` - 13개 핀 상태 → TRIS/LAT 레지스터 계산 → 패킷 송수신 →
  PORT 응답 파싱 + VPP ADC 계산
- `VPPVDDactiveCore`, `DCDCactiveCore` - 전원 제어 패킷
- `ApplyConfigWordsCore` - PIC12/16/18 계열별 Config Word 주소 매핑
- `LoadConfigFile`, `SaveConfigFile` - opgui.ini 읽기/쓰기
- `ParseCommandLinePackets`, `RunCommandLineMode` - 명령행 모드

이들 함수는 GTK에 독립적이며, UI 값은 인자로 받거나 전역 변수에서 읽음.

### 3. icd.c 분할 포팅
원본 `icd.c` (1,541 라인)를 두 파일로 분할:

- **`icd_core.c`** (GTK 독립): `startICD`, `isRunning`, `cont`, `step`, `run`,
  `version`, `Halt`, `ReadRegister`, `WriteRegister`, `ReadProgMem`, `ReadDataMem`,
  `getVar`, `decodeCmd`, `initVar`, `ShowBank`, `ShowContext`, `addWatch`,
  `removeWatch`, `executeCommand` 등. GTK 호출은 `gui_icd_*` 콜백으로 추상화.
- **`icd_wx.cpp`** (wxWidgets GUI): `gui_icd_*` 콜백 구현 + ICD 제어 함수
  (`wx_icd_run`, `wx_icd_halt`, `wx_icd_step`, `wx_icd_step_over`, `wx_icd_stop`,
  `wx_icd_refresh`, `wx_icd_help`) + ICD 탭 UI.

### 4. device_db.c - 디바이스 DB 연동
원본 `AddDevices`는 GTK TreeView/ListStore에 강결합. wxWidgets 버전은:
- `gui_dev_list_clear`, `gui_dev_list_add`, `gui_dev_list_done` 콜백으로 UI와 분리
- `ShowDeviceInfo()` 함수로 디바이스 선택 시 정보 표시 + 패밀리별 AVR/PIC 프레임 토글
- `DoReadDevice()`, `DoWriteDevice()` 로 `deviceRW.c`의 `Read`/`Write` 호출

### 5. AVR 퓨즈 UI
- Fuse Low / High / Extended / Lock 각각 hex entry + "Write" checkbox
- `Write LF @ 3kHz` 버튼 → `WriteATfuseSlow()` 호출
- `getOptions()`에서 체크박스 상태를 읽어 `AVRfuse`/`AVRfuse_h`/`AVRfuse_x`/`AVRlock`
  전역 변수에 저장 (체크 안 된 항목은 0x100 = "쓰지 않음")
- 디바이스가 AVR 패밀리일 때만 AVR 패널 표시

### 6. Hex Tools 탭
- **Intel HEX → Data**: HEX 라인을 파싱하여 주소 + 데이터 바이트 표시.
  체크섬 검증 포함. record type 0x00 (Data), 0x04 (Ext Linear Addr), 0x01 (EOF) 지원.
- **Data → Intel HEX**: 주소 + hex 바이트들로부터 Intel HEX 라인 생성.
  체크섬 자동 계산.
- **Save**: 인코딩된 HEX 라인을 파일로 저장.

### 7. IO 패널 (13개 핀)
- 13개 핀 (RB0-RB7, RC7, RC6, RA5, RA4, RA3) 각각 3-way 라디오 (0/1/입력)
- "IO active" 체크 시 100ms 타이머로 `IOchangedCore` 호출
- PORT 응답을 라벨에 표시 ("0"/"1")
- VPP 전압 (ADC)을 상태바에 표시
- VPP/VDD/DCDC 전원 제어 + DCDC 전압 슬라이더 (5.0-15.0V, 0.1V 단위)

### 8. I2C/SPI 패널
- 6개 모드: I2C 8-bit, I2C 16-bit, SPI 0,0 / 0,1 / 1,0 / 1,1
- 4단계 속도: 100 kHz, 400 kHz, 1 MHz, 3.4 MHz
- N bytes (0-60) spin control
- 송신 데이터 hex 파싱 (공백/콤마 구분)
- `I2CSend`/`I2CReceive` (I2CSPI.c) 호출

### 9. Options 패널
- **Config Words**: CW1-CW7 hex entry + "Force configuration write" checkbox
- **PIC Options**: EEPROM_RW, ReadReserved, Write_ID_BKCal, WriteCalib12,
  UseSaflock, UseOSCCAL, UseBKOSCCAL, UseFileCal
- **ICD / Misc**: Enable ICD + ICD address, Enable log, Skip V33 check, Wait S1

## 빌드 방법

### Linux (테스트용)
```bash
make linux
./opgui_wx_linux
```

### Windows 크로스 컴파일
```bash
# llvm-mingw 20240619 (Linux 호스트) + wxWidgets 3.2.6 MSW 정적 빌드 필요
make windows MINGW_WX_CONFIG=/path/to/wx-config
# -> opgui_wx.exe (Windows 10+ 단일 파일 실행, 외부 런타임 불필요)
```

### 명령행 모드 (GUI 없이)
```bash
# Linux
./opgui_wx_linux -c 12 34 56 78

# Windows
opgui_wx.exe -c 12 34 56 78
```
지정한 hex 바이트들을 USB 패킷으로 전송하고 응답을 stdout에 출력.

## 현재 구현된 기능 (전체)

### 메인 UI
- 메인 윈도우, 메뉴 (File / View / Help), 툴바
- 8개 탭: Data / Device / Options / I2C-SPI / IO / AVR / Hex Tools / ICD

### Device 탭
- 디바이스 트리 (293개 디바이스, 실제 DB에서 로드)
- 그룹 필터 (PIC10/12, PIC16, PIC18, PIC24, PIC30/33, ATMEL AVR, MEMORY)
- 텍스트 필터 (대소문자 무시 부분 매칭)
- 디바이스 정보 라벨 (family, EE, V33, HV, features)
- 디바이스 옵션 체크박스
- VID/PID/Max err 엔트리

### Options 탭 (NEW - 완전 구현)
- Config Words CW1-CW7 + Force configuration write
- PIC 옵션 8개 체크박스 (EEPROM_RW, ReadReserved, Write_ID_BKCal, WriteCalib12,
  UseSaflock, UseOSCCAL, UseBKOSCCAL, UseFileCal)
- ICD enable + ICD address
- Enable log + Skip V33 check + Wait S1

### I2C/SPI 탭 (NEW - 완전 구현)
- 6개 모드 라디오 (I2C 8/16-bit, SPI 0,0/0,1/1,0/1,1)
- 4단계 속도 콤보박스
- N bytes spin (0-60)
- 송신/수신 hex 데이터
- I2CSend/I2CReceive 호출

### IO 탭 (NEW - 완전 구현)
- VPP/VDD/DCDC 전원 제어 체크박스
- DCDC 전압 슬라이더 (5.0-15.0V)
- IO active 체크박스 (100ms 폴링 타이머)
- 13개 핀 라디오 (0/1/입력) + PORT 상태 라벨
- 직접 USB 명령 송수신

### AVR 탭
- Fuse Low / High / Extended / Lock hex entry + Write checkbox
- `Write LF @ 3kHz` 버튼 (WriteATfuseSlow 호출)
- AVR 패밀리 디바이스 선택 시 자동 표시

### Hex Tools 탭
- Intel HEX 디코딩 (체크섬 검증)
- Intel HEX 인코딩 (체크섬 자동 계산)
- HEX 라인 파일 저장

### ICD 탭
- Source 창 (디스어셈블리 / COFF 소스 표시)
- Status 창 (PC, STATUS, 메모리 뱅크, EEPROM 덤프)
- Command 입력 (Enter 키로 실행)
- Run/Halt/Step/StepOver/Stop/Refresh 버튼
- Load COFF 파일 버튼
- View 메뉴 체크항목 (PC/STATUS/Bank0-3/EEPROM 표시 토글)
- F1=Help, F5=Halt, F7=Step, F8=StepOver, F9=Run 단축키
- 지원 명령: break, clear, freeze, h/halt, help, list, print, p,
  r/run, s/step, ss/step over, ver, watch, set variable

### 툴바
- Open / Save (fileIO.c의 Load/Save 호출, 디바이스 타입 자동 감지)
- Read / Write (deviceRW.c의 Read/Write 호출, getOptions로 모든 옵션 동기화)
- Stop / Connect / Info

### 명령행 모드 (NEW)
- `-c` / `--command` 옵션으로 GUI 없이 USB 패킷 송수신
- `-h` / `--help` 도움말
- `--version` 버전 표시

### 설정 파일 (NEW)
- `~/.opgui/opgui.ini` (Linux) 또는 `%USERPROFILE%\.opgui\opgui.ini` (Windows)
- device 이름, vid, pid, max_err 저장
- 변경이 있을 때만 저장 (변경 감지 via 초기값 백업)

## 원본과의 차이점

| 항목 | 원본 (GTK) | 포팅 (wxWidgets) |
|---|---|---|
| GUI 프레임워크 | GTK 3 + Glade XML | wxWidgets 3.2 코드 기반 |
| Windows 런타임 | GTK Runtime 별도 설치 | 단일 .exe (정적 링크) |
| 실행 파일 크기 | ~3 MB + GTK DLL | ~8.6 MB (모든 것 포함) |
| 코어 .c 파일 | 그대로 | 그대로 (수정 없음) |
| icd.c | 1,541 라인 통째로 | icd_core.c + icd_wx.cpp 분할 |
| 디바이스 DB | GTK ListStore | wxListCtrl (gui_dev_list_* 콜백) |
| AVR 퓨즈 UI | Glade XML | wxWidgets 코드 |
| Hex 변환 도구 | Glade XML | wxWidgets 코드 (별도 탭) |
| IO 핀 제어 | Glade XML + 52개 위젯 | wxWidgets 코드 (13×3 라디오) |
| Config Words | Glade XML | wxWidgets 코드 (Options 탭) |
| I2C/SPI | Glade XML | wxWidgets 코드 |
| 설정 파일 | g_get_home_dir + g_build_path | 직접 getenv + snprintf |
| 명령행 모드 | getopt_long_only | wxApp argc/argv 파싱 |
| icons.c / resources.c | 포함 | 제거 (GTK 리소스 매커니즘) |

## 기능 패리티 요약

원본 OPGUI의 모든 주요 기능이 포팅되었습니다:

- ✅ 디바이스 읽기/쓰기 (PIC12/16/18/24, AVR, EEPROM)
- ✅ 디바이스 DB (293개 디바이스, 필터)
- ✅ 파일 입출력 (Intel HEX, binary)
- ✅ AVR 퓨즈/락 비트 쓰기
- ✅ ICD 디버거 (COFF, 브레이크포인트, watch, 디스어셈블리)
- ✅ IO 핀 수동 제어 (13개 핀)
- ✅ VPP/VDD/DCDC 전원 제어
- ✅ I2C/SPI 송수신 (6 모드)
- ✅ Hex 변환 도구
- ✅ Config Words (PIC12/16/18)
- ✅ 로깅
- ✅ 설정 파일 (opgui.ini)
- ✅ 명령행 모드 (-c)

## 라이선스

원본과 동일한 GPL v2 (gpl-2.0.txt 참조)

원본 저작권: © 2009-2025 Alberto Maccioni (http://openprog.altervista.org/)
wxWidgets 포팅: 동일 라이선스 유지
