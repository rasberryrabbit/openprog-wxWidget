extern char* devices[];
extern int Ndevices;
enum family_t {PIC12=0,PIC16,PIC18,PIC24,PIC32,AVR,I2CEE,SPIEE,UWEE,OWEE,UNIOEE,NUM_FAMILIES};
enum group_t {G_PIC_10_12=0,G_PIC_16,G_PIC_18,G_PIC_24,G_PIC_30_33,G_ATMEL,G_EEPROM,NUM_GROUPS};


// This struct stores information about how to program a set of devices
struct DEVICES {
        char *device;                           //comma-separated list of device names which share the same method of programming
        enum family_t family;           //architecture type
        double HV;                                      //High voltage value (-1= turn off HV)
        int V33;                                        //3.3V regulator required (0=not required)
        void (*ReadPtr)();                      //Read function pointer
        int ReadParam[4];                       //Read function parameters; -10 = NU
        int ResArea;                            //reserved area size
        void (*WritePtr)();                     //Write function pointer
        int WriteParam[6];                      //Write function parameters; -10 = NU
        double WriteParamD;                     //Write function parameter, double
};

// This struct stores information about a specific device
struct DevInfo{
        char *device;                           //device name
        enum group_t group;                     //device group (used for selection in GUI only) - see nameToGroup()
        enum family_t family;           //architecture type
//      int type;                                       //type
        double HV;                                      //High voltage value (-1= turn off HV)
        int V33;                                        //3.3V regulator required (0=not required)
        int size;                                       //memory size
        int sizeEE;                                     //EE memory size
        char *features;                         //device features string
};

extern int NDEVLIST;
extern struct DEVICES DEVLIST[];

/// Determine the group of a specific device
enum group_t nameToGroup(const char *devName);

/// Copies the device info from the passed devlist entry into the passed DevInfo
/// Does NOT populate fields 'device' and 'group' as these are device-specific
void populateDevInfo(struct DevInfo *info, const struct DEVICES *devlistEntry);

///Search and return device info. Uses strtok (so don't call inside a strtok loop)
struct DevInfo GetDevInfo(const char* dev);

#if !defined _GTKGUI && !defined _WXGUI
void AddDevices(char *list);
#endif
char* ListDevices();
int GetDevType(const char* device);
struct DevInfo GetDevInfo(const char* dev);
void Write(char* dev,int ee);
void Read(char* dev,int ee,int r);

/* ============================================================
 *   wxWidgets/GTK GUI 확장 API - deviceRW.c 자체 제공
 *   device_db.c 없이도 deviceRW.c만으로 디바이스 DB 접근 가능
 * ============================================================ */

/// DEVLIST 항목 수 반환
int GetDeviceCount(void);

/// devices[] 배열의 항목 수 반환
int GetDeviceNameCount(void);

/// 인덱스로 DEVLIST 항목 직접 접근 (범위 초과 시 NULL)
const struct DEVICES* GetDeviceListEntry(int idx);

/// 인덱스로 디바이스 이름 문자열 반환 (쉼표 포함, free 금지)
const char* GetDeviceFirstName(int idx);

/// 디바이스 읽기/쓰기 지원 여부 확인
int IsDeviceSupported(const char* devName, int readOk, int writeOk);

/// 그룹/텍스트 필터로 디바이스 열거 (콜백 방식)
int EnumerateDevices(enum group_t groupFilter, const char* textFilter,
                     void (*callback)(const char* name, const char* groupName,
                                      int idx, void* userdata),
                     void* userdata);

/// 디바이스 이름으로 DEVLIST 인덱스 검색
int FindDeviceIndex(const char* devName);

/// 특정 DEVLIST 항목의 모든 별칭 열거
int EnumerateDeviceAliases(int devIdx,
                           void (*callback)(const char* name, void* userdata),
                           void* userdata);

/// DevInfo 구조체 메모리 해제 (features, device 문자열 free)
void FreeDevInfo(struct DevInfo *info);

/// 디바이스 ReadParam[4] 반환 (0=성공, -1=not found)
int GetDeviceReadParams(const char* devName, int params[4]);

/// 디바이스 WriteParam[6] 반환 (0=성공, -1=not found)
int GetDeviceWriteParams(const char* devName, int params[6]);

/// 디바이스 Reserved Area 크기 반환 (-1=not found)
int GetDeviceReservedArea(const char* devName);

/// 디바이스 HV 값 반환 (-1=HV 미사용, -2=not found)
double GetDeviceHV(const char* devName);

/// 디바이스 3.3V 레귤레이터 필요 여부 (1=필요, 0=불필요, -1=not found)
int GetDeviceV33Required(const char* devName);

/// 패밀리별 디바이스 통계 (stats[NUM_FAMILIES] 채움)
/// 반환: 전체 DEVLIST 항목 수
int GetDeviceStats(int stats[NUM_FAMILIES]);

/// 그룹별 디바이스 통계 (stats[NUM_GROUPS] 채움, 별칭 포함)
/// 반환: 전체 디바이스 이름 수
int GetGroupStats(int stats[NUM_GROUPS]);

/// DEVLIST 무결성 검사 (문제 시 callback 호출)
/// 반환: 0=모두 OK, 음수=문제 개수
int ValidateDeviceList(void (*callback)(const char* msg, int idx, void* userdata),
                       void* userdata);

/// 디바이스 ReadPtr 함수 포인터 반환 (고급용)
void* GetDeviceReadFunc(const char* devName);

/// 디바이스 WritePtr 함수 포인터 반환 (고급용)
void* GetDeviceWriteFunc(const char* devName);
