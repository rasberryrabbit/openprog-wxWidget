unsigned int htoi(const char *hex, int length);
void Save(int devType,char* savefile);
int Load(int devType,char*loadfile);
void SaveEE(int devType,char* savefile);
int LoadEE(int devType,char*loadfile);
void OpenLogFile(void);
void WriteLogIO();
void CloseLogFile();
