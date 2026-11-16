/*
 * fileIO.c - file read and write
 * Copyright (C) 2010-2025 Alberto Maccioni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111 USA
 * or see <http://www.gnu.org/licenses/>
 */

//configure for GUI or command-line
#include "common.h"
#include "progP12.h"
#include "progP16.h"
#include "progP18.h"
#include "progP24.h"
#include "progAVR.h"
#include "deviceRW.h"

unsigned int htoi(const char *hex, int length)
{
	int i;
	unsigned int v = 0;
	for (i = 0; i < length; i++) {
		v <<= 4;
		if (hex[i] >= '0' && hex[i] <= '9') v += hex[i] - '0';
		else if (hex[i] >= 'a' && hex[i] <= 'f') v += hex[i] - 'a' + 10;
		else if (hex[i] >= 'A' && hex[i] <= 'F') v += hex[i] - 'A' + 10;
		else PrintMessage1(strings[S_Inohex],hex);	//"Error: '%.4s' doesn't look very hexadecimal, right?\n"
	}
	return v;
}

void Save(int devType,char* savefile)
{
	FILE* f=fopen(savefile,"w");
	if(!f) return;
	char str[512],str1[512]="";
	int i,sum=0,count=0,ext=0,s,base;
//**************** 10-16F *******************************************
	if(devType==PIC12||devType==PIC16){
		int x=0x3fff,addr;
		//if(!strncmp(dev,"16",2)||!strncmp(dev,"12F6",4)) x=0x3fff;
		fprintf(f,":020000040000FA\n");			//extended address=0
		for(i=0;i<sizeW;i++) memCODE_W[i]&=x;
		for(i=0;i<sizeW&&memCODE_W[i]>=x;i++); //remove leading 0xFFF
		for(;i<sizeW;i++){
			sum+=(memCODE_W[i]>>8)+(memCODE_W[i]&0xff);
			sprintf(str,"%02X%02X",memCODE_W[i]&0xff,memCODE_W[i]>>8);
			strcat(str1,str);
			count++;
			if(count==8||i==sizeW-1){
				base=i-count+1;
				for(s=i;s>=base&&memCODE_W[s]>=x;s--){	//remove trailing 0xFFF
					sum-=(memCODE_W[s]>>8)+(memCODE_W[s]&0xff);
					str1[strlen(str1)-4]=0;
				}
				count-=i-s;
				addr=(s-count+1)*2;
				sum+=count*2+(addr&0xff)+(addr>>8);
				if(base>>15>ext){
					ext=base>>15;
					fprintf(f,":02000004%04X%02X\n",ext,(-6-ext)&0xff);
				}
				if(count) fprintf(f,":%02X%04X00%s%02X\n",count*2,addr&0xFFFF,str1,(-sum)&0xff);
				str1[0]=0;
				count=sum=0;
			}
		}
		if(sizeEE){		//this is only for 16F1xxx
			if(ext!=0x01) fprintf(f,":020000040001F9\n");
			for(i=0,count=sum=0;i<sizeEE;i++){
				sum+=memEE[i];
				sprintf(str,"%02X00",memEE[i]&0xff);
				strcat(str1,str);
				count++;
				if(count==8||i==sizeEE-1){
					for(s=i;s>i-count&&memEE[s]>=0xff;s--){	//remove trailing 0xFF
						sum-=memEE[s]&0xff;
						str1[strlen(str1)-4]=0;
					}
					count-=i-s;
					addr=(s-count+1)*2+0xE000;
					sum+=count*2+(addr&0xff)+(addr>>8);
					if(count){
						fprintf(f,":%02X%04X00%s%02X\n",count*2,addr,str1,(-sum)&0xff);
					}
					str1[0]=0;
					count=sum=0;
				}
			}
		}
		fprintf(f,":00000001FF\n");
	}
//**************** 18F *******************************************
	else if(devType==PIC18){
		fprintf(f,":020000040000FA\n");			//extended address=0
		for(i=0;i<size&&memCODE[i]==0xff;i++); //remove leading 0xFF
		for(;i<size;i++){
			sum+=memCODE[i];
			sprintf(str,"%02X",memCODE[i]);
			strcat(str1,str);
			count++;
			if(count==16||i==size-1){
				base=i-count+1;
				for(s=i;s>=base&&memCODE[s]==0xff;s--){	//remove trailing 0xFF
					sum-=memCODE[s];
					str1[strlen(str1)-2]=0;
				}
				count-=i-s;
				sum+=count+(base&0xff)+((base>>8)&0xff);
				if(base>>16>ext){
					ext=base>>16;
					fprintf(f,":02000004%04X%02X\n",ext,(-6-ext)&0xff);
				}
				if(count){
					fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
				}
				str1[0]=0;
				count=sum=0;
			}
		}
		for(i=0,count=sum=0;i<8;i++){
			sum+=memID[i];
			sprintf(str,"%02X",memID[i]&0xff);
			strcat(str1,str);
			count++;
			if(count==8){
				fprintf(f,":020000040020DA\n");
					base=i-count+1;
				for(s=i;s>i-count&&memID[s]>=0xff;s--){	//remove trailing 0xFF
					sum-=memID[s]&0xff;
					str1[strlen(str1)-2]=0;
				}
				count-=i-s;
				sum+=count+(base&0xff)+((base>>8)&0xff);
				if(count){
					fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
				}
				str1[0]=0;
				count=sum=0;
			}
		}
		for(i=0,count=sum=0;i<14;i++){
			sum+=memCONFIG[i];
			sprintf(str,"%02X",memCONFIG[i]&0xff);
			strcat(str1,str);
			count++;
			if(count==14){
				fprintf(f,":020000040030CA\n");
				base=i-count+1;
				for(s=i;s>i-count&&memCONFIG[s]>=0xff;s--){	//remove trailing 0xFF
					sum-=memCONFIG[s]&0xff;
					str1[strlen(str1)-2]=0;
				}
				count-=i-s;
				sum+=count+(base&0xff)+((base>>8)&0xff);
				if(count){
					fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
				}
				str1[0]=0;
				count=sum=0;
			}
		}
		if(sizeEE){
			fprintf(f,":0200000400F00A\n");
			for(i=0,count=sum=0;i<sizeEE;i++){
				sum+=memEE[i];
				sprintf(str,"%02X",memEE[i]&0xff);
				strcat(str1,str);
				count++;
				if(count==16||i==sizeEE-1){
					base=i-count+1;
					for(s=i;s>i-count&&memEE[s]>=0xff;s--){	//remove trailing 0xFF
						sum-=memEE[s]&0xff;
						str1[strlen(str1)-2]=0;
					}
					count-=i-s;
					sum+=count+(base&0xff)+((base>>8)&0xff);
					if(count){
						fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
					}
					str1[0]=0;
					count=sum=0;
				}
			}
		}
		fprintf(f,":00000001FF\n");
	}
//**************** 24F *******************************************
	else if(devType==PIC24){
		int valid;
		fprintf(f,":020000040000FA\n");			//extended address=0
		int sum=0,count=0,s,word;
		word=memCODE[0]+(memCODE[1]<<8)+(memCODE[2]<<16)+(memCODE[3]<<24);
		for(i=0;i<size&&word==0xffffffff;i+=4) //remove leading 0xFFFFFFFF
			word=memCODE[i]+(memCODE[i+1]<<8)+(memCODE[i+2]<<16)+(memCODE[i+3]<<24);
		for(;i<size;i++){
			sum+=memCODE[i];
			sprintf(str,"%02X",memCODE[i]);
			strcat(str1,str);
			count++;
			if(count==16||i==size-1){
				base=i-count+1;
				for(s=base,valid=0;s<=i&&!valid;s+=4){	//remove empty lines
					if(memCODE[s]<0xFF||memCODE[s+1]<0xFF||+memCODE[s+2]<0xFF) valid=1;
				}
				sum+=count+(base&0xff)+((base>>8)&0xff);
				if(base>>16>ext){
					ext=base>>16;
					fprintf(f,":02000004%04X%02X\n",ext,(-6-ext)&0xff);
				}
				if(count&&valid){
					fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
				}
				str1[0]=0;
				count=sum=0;
			}
		}
		if(sizeCONFIG){
			fprintf(f,":0200000401F009\n");
			for(i=0,count=sum=0;i<sizeCONFIG&&i<48;i++){
				sum+=memCONFIG[i];
				sprintf(str,"%02X",memCONFIG[i]);
				strcat(str1,str);
				count++;
				if(count==4||i==sizeCONFIG-1){
					base=i-count+1;
					sum+=count+(base&0xff)+((base>>8)&0xff);
					if(count){
						fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
					}
					str1[0]=0;
					count=sum=0;
				}
			}
		}
		if(sizeEE){
			fprintf(f,":0200000400FFFB\n");
			str1[0]=0;
			for(i=0,count=sum=0;i<sizeEE;i+=2){		//append 0000 every 2 bytes
				sum+=memEE[i]+memEE[i+1];
				sprintf(str,"%02X%02X0000",memEE[i]&0xff,memEE[i+1]&0xff);
				strcat(str1,str);
				count+=4;
				if(count==16||i==sizeEE-2){
					base=2*i-count+4;
					for(s=base/2,valid=0;s<=i&&!valid;s+=2){	//remove empty lines
						if(memEE[s]<0xFF||memEE[s+1]<0xFF) valid=1;
					}
					sum+=0xE0+count+(base&0xff)+(base>>8);
					if(count&&valid){
						fprintf(f,":%02X%04X00%s%02X\n",count,base+0xE000,str1,(-sum)&0xff);
					}
					str1[0]=0;
					count=sum=0;
				}
			}
		}
		fprintf(f,":00000001FF\n");
	}
//**************** ATxxxx *******************************************
	else if(devType==AVR){
		fprintf(f,":020000040000FA\n");			//extended address=0
		for(i=0;i<size&&memCODE[i]==0xff;i++); //remove leading 0xFF
		for(;i<size;i++){
			sum+=memCODE[i];
			sprintf(str,"%02X",memCODE[i]);
			strcat(str1,str);
			count++;
			if(count==16||i==size-1){
				base=i-count+1;
				for(s=i;s>=base&&memCODE[s]==0xff;s--){	//remove trailing 0xFF
					sum-=memCODE[s];
					str1[strlen(str1)-2]=0;
				}
				count-=i-s;
				sum+=count+(base&0xff)+((base>>8)&0xff);
				if(base>>16>ext){
					ext=base>>16;
					fprintf(f,":02000004%04X%02X\n",ext,(-6-ext)&0xff);
				}
				if(count){
					fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
				}
				str1[0]=0;
				count=sum=0;
			}
		}
		fprintf(f,":00000001FF\n");
	}
//**************** 24xxx / 93xxx / 25xxx / 95xxx / DSxxxx *******************************************
	else if(devType==I2CEE||devType==SPIEE||devType==UWEE||devType==OWEE||devType==UNIOEE){
		if(strstr(savefile,".bin")||strstr(savefile,".BIN")){
			#ifdef _WIN32
			//brain-damaged op. systems need this to avoid messing with some bytes
			f=freopen(savefile,"wb",f);
			if(!f) return;
			#endif
			fwrite(memEE,1,sizeEE,f);
		}
		else{			//HEX
			int valid;
			fprintf(f,":020000040000FA\n");			//extended address=0
			for(i=0;i<sizeEE;i++){
				sum+=memEE[i];
				sprintf(str,"%02X",memEE[i]);
				strcat(str1,str);
				count++;
				if(count==16||i==sizeEE-1){
					for(s=valid=0;str1[s]&&!valid;s++) if(str1[s]!='F') valid=1;
					if(valid){
						base=i-count+1;
						sum+=count+(base&0xff)+((base>>8)&0xff);
						if(base>>16>ext){
							ext=base>>16;
							fprintf(f,":02000004%04X%02X\n",ext,(-6-ext)&0xff);
						}
						if(count){
							fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
						}
					}
					str1[0]=0;
					count=sum=0;
				}
			}
			fprintf(f,":00000001FF\n");
		}
	}
	if(f) fclose(f);
}

void SaveEE(int devType,char* savefile){
	FILE* f=fopen(savefile,"w");
	if(!f) return;
//**************** ATMEL *******************************************
	else if(devType==AVR){
		char str[512],str1[512]="";
		int i,base;
		fprintf(f,":020000040000FA\n");			//extended address=0
		int sum=0,count=0,s;
		for(i=0,count=sum=0;i<sizeEE;i++){
			sum+=memEE[i];
			sprintf(str,"%02X",memEE[i]&0xff);
			strcat(str1,str);
			count++;
			if(count==16||i==sizeEE-1){
				base=i-count+1;
				for(s=i;s>i-count&&memEE[s]>=0xff;s--){	//remove trailing 0xFF
					sum-=memEE[s]&0xff;
					str1[strlen(str1)-2]=0;
				}
				count-=i-s;
				sum+=count+(base&0xff)+((base>>8)&0xff);
				if(count){
					fprintf(f,":%02X%04X00%s%02X\n",count,base&0xFFFF,str1,(-sum)&0xff);
				}
				str1[0]=0;
				count=sum=0;
			}
		}
		fprintf(f,":00000001FF\n");
	}
	if(f) fclose(f);
}

int Load(int devType,char*loadfile){
	int i,input_address=0,ext_addr=0,sum,valid;
	char line[256];
	FILE* f=fopen(loadfile,"r");
	if(!f) return -1;
	PrintMessage2("[%d] %s :\r\n",devType,loadfile);
//**************** 10-16F *******************************************
	if(devType==PIC12||devType==PIC16){
		unsigned char buffer[0x20000],bufferEE[0x1000];
		int sizeM=0;
		memset(buffer,0xFF,sizeof(buffer));
		memset(bufferEE,0xFF,sizeof(bufferEE));
		sizeEE=0;
		for(;fgets(line,256,f);){
			if(strlen(line)>9&&line[0]==':'){
				int hex_count = htoi(line+1, 2);
					if((int)strlen(line)-11<hex_count*2) {
						PrintMessage1(strings[S_IhexShort],line);	//"Intel hex8 line too short:\r\n%s\r\n"
				}
				else{
					input_address=(ext_addr<<16)+htoi(line+3,4);
					sum=0;
					for (i=1;i<=hex_count*2+9;i+=2) sum += htoi(line+i,2);
					if ((sum & 0xff)!=0) {
							PrintMessage1(strings[S_IhexChecksum],line);	//"Intel hex8 checksum error in line:\r\n%s\r\n"
					}
					else{
						switch(htoi(line+7,2)){
							case 0:		//Data record
								if(input_address<0x1E000){		//Code
									sizeM=input_address+hex_count;
									if(sizeM>sizeW) sizeW=sizeM;
									for (i=0;i<hex_count;i++){
										buffer[input_address+i]=htoi(line+9+i*2,2);
									}
								}
								else if(input_address>=0x1E000&&input_address<0x1F000){	//EEPROM
									sizeM=(input_address-0x1E000+hex_count)/2;
									if(sizeM>sizeEE) sizeEE=sizeM;
									for (i=0;i<hex_count;i+=2){
										bufferEE[(input_address-0x1E000)/2+i/2]=htoi(line+9+i*2,2);
									}
								}
								break;
							case 4:		//extended linear address record
								if(strlen(line)>14)	ext_addr=htoi(line+9,4);
								break;
							default:
								break;
						}
					}
				}
			}
		}
		sizeW/=2;
		if(memCODE_W) free(memCODE_W);
		memCODE_W=(WORD*)malloc(sizeof(WORD)*sizeW);
		for(i=0;i<sizeW;i++){		//Swap bytes
			memCODE_W[i]=(buffer[i*2+1]<<8)+buffer[i*2];
		}
		if(memEE) free(memEE);
		if(sizeEE){
			memEE=(unsigned char*)malloc(sizeEE);
			memcpy(memEE,bufferEE,sizeEE);
		}
		else memEE=0;
		PrintMessage(strings[S_CodeMem]);	//"\r\nCode memory:\r\n"
		int imax=sizeW>0x8000?0x8500:0x2100;
		DisplayCODE16F(imax);
		if(sizeW>=0x2100&&sizeW<0x3000){	//EEPROM@0x2100
			PrintMessage(strings[S_EEMem]);	//"\r\nEEPROM memory:\r\n"
			DisplayEE16F(0x700);
		}
		else if(sizeEE) DisplayEE();
		PrintMessage("\r\n");
	}
//**************** 18F *******************************************
	else if(devType==PIC18){
		unsigned char buffer[0x30000],bufferEE[0x1000];
		int sizeM;
		memset(buffer,0xFF,sizeof(buffer));
		memset(bufferEE,0xFF,sizeof(bufferEE));
		memset(memID,0xFF,sizeof(memID));
		memset(memCONFIG,0xFF,sizeof(memCONFIG));
		for(;fgets(line,256,f);){
			if(strlen(line)>9&&line[0]==':'){
				int hex_count = htoi(line+1, 2);
				if((int)strlen(line) - 11 < hex_count * 2) {
					PrintMessage1(strings[S_IhexShort],line);	//"Intel hex8 line too short:\r\n%s\r\n"
				}
				else{
					input_address=htoi(line+3,4);
					sum=0;
					for (i=1;i<=hex_count*2+9;i+=2)
						sum += htoi(line+i,2);
					if ((sum & 0xff)!=0) {
						PrintMessage1(strings[S_IhexChecksum],line);	//"Intel hex8 checksum error in line:\r\n%s\r\n"
					}
					else{
						switch(htoi(line+7,2)){
							case 0:		//Data record
								if(ext_addr<0x20){		//Code	<0x200000
									sizeM=(ext_addr<<16)+input_address+hex_count;
									if(sizeM>size) size=sizeM;
									for (i=0;i<hex_count;i++){
										buffer[(ext_addr<<16)+input_address+i]=htoi(line+9+i*2,2);
									}
								}
								else if(ext_addr==0x20&&input_address<64){	//ID: 0x200000
									for (i=0;i<hex_count;i++){
										memID[input_address+i]=htoi(line+9+i*2,2);
									}
								}
								else if(ext_addr==0x30&&input_address<32){	//CONFIG: 0x300000
									for (i=0;i<hex_count;i++){
										memCONFIG[input_address+i]=htoi(line+9+i*2,2);
									}
								}
								else if((ext_addr==0xF0||ext_addr==0x31||ext_addr==0x38)&&input_address<0x1000){	
								//EEPROM: 0xF00000, 0x310000, 0x380000
									for (i=0;i<hex_count;i++){
										bufferEE[input_address+i]=htoi(line+9+i*2,2);
									}
									sizeM=input_address+hex_count;
									if(sizeM>sizeEE) sizeEE=sizeM;

								}
								break;
							case 4:		//extended linear address record
								if(strlen(line)>14)	ext_addr=htoi(line+9,4);
								break;
							default:
								break;
						}
					}
				}
			}
		}
		if(memCODE) free(memCODE);
		memCODE=(unsigned char*)malloc(size);
		memcpy(memCODE,buffer,size);
		if(memEE) free(memEE);
		memEE=(unsigned char*)malloc(sizeEE);
		memcpy(memEE,bufferEE,sizeEE);
		PrintMessage(strings[S_IDMem]);	//"ID memory:\r\n"
		for(i=0;i<8;i+=2)	PrintMessage4("ID%d: 0x%02X   ID%d: 0x%02X\r\n",i,memID[i],i+1,memID[i+1]);
		PrintMessage(strings[S_ConfigMem]);	//"CONFIG memory:\r\n"
		for(i=0;i<12;i++){
			PrintMessage2("[0x%06X] 0x%02X\r\n",0x300000+i,memCONFIG[i]);
		}
		for(;i<32;i++){			//only not empty locations
			if(memCONFIG[i]!=0xFF) PrintMessage2("[0x%06X] 0x%02X\r\n",0x300000+i,memCONFIG[i]);
		}
		PrintMessage(strings[S_CodeMem]);	//"\r\nCODE memory:\r\n"
		DisplayCODE18F(size);
		if(sizeEE) DisplayEE();
		PrintMessage("\r\n");
	}
//**************** 24F *******************************************
	else if(devType==PIC24){
		unsigned char *buffer,bufferEE[0x2000];
		int d;
		buffer=(unsigned char*)malloc(0x100000);
		memset(buffer,0xFF,0x100000);
		memset(bufferEE,0xFF,sizeof(bufferEE));
		memset(memCONFIG,0xFF,sizeof(memCONFIG));
		memset(memUSERID,0xFF,sizeof(memUSERID));
		sizeUSERID=0;
		for(;fgets(line,256,f);){
			if(strlen(line)>9&&line[0]==':'){
				int hex_count = htoi(line+1, 2);
				if((int)strlen(line) - 11 < hex_count * 2) {
						PrintMessage1(strings[S_IhexShort],line);	//"Intel hex8 line too short:\r\n%s\r\n"
				}
				else{
					input_address=htoi(line+3,4);
					sum=0;
					for (i=1;i<=hex_count*2+9;i+=2)
						sum += htoi(line+i,2);
					if ((sum & 0xff)!=0) {
							PrintMessage1(strings[S_IhexChecksum],line);	//"Intel hex8 checksum error in line:\r\n%s\r\n"
					}
					else{
						switch(htoi(line+7,2)){
							case 0:		//Data record
								if(ext_addr<0x20){		//Code
									int end1=(ext_addr<<16)+input_address+hex_count;
									if(size<end1) size=end1;
									for (i=0;i<hex_count;i++){
										buffer[(ext_addr<<16)+input_address+i]=htoi(line+9+i*2,2);
									}
								}
								else if(ext_addr==0x1F0&&input_address<48){	//CONFIG
									sizeCONFIG=input_address+hex_count;
									for (i=0;i<hex_count;i++){
										memCONFIG[input_address+i]=htoi(line+9+i*2,2);
									}
								}
								else if(ext_addr==0xFF&&input_address>=0xE000){	//EEPROM
									for (i=0;i<hex_count;i++){
										bufferEE[input_address-0xE000+i]=htoi(line+9+i*2,2);
									}
									sizeEE=input_address-0xE000+hex_count;
								}
								else if(ext_addr==0x100&&input_address<8){	//USER ID
									sizeUSERID=input_address+hex_count;
									for (i=0;i<hex_count&&(i+input_address)<8;i++){
										memUSERID[input_address+i]=htoi(line+9+i*2,2);
									}
								}
								break;
							case 4:		//extended linear address record
								if(strlen(line)>14)	ext_addr=htoi(line+9,4);
								break;
							default:
								break;
						}
					}
				}
			}
		}
		if(memCODE) free(memCODE);
		memCODE=(unsigned char*)malloc(size);
		memcpy(memCODE,buffer,size);
		free(buffer);
		sizeEE=sizeEE?0x1000:0;
		if(memEE) free(memEE);
		memEE=(unsigned char*)malloc(sizeEE);
		for(i=0;i<sizeEE;i+=2){		//skip voids in the hex file organization
			memEE[i]=bufferEE[i*2]; 	//0 1 4 5 8 9 12 13 ...
			memEE[i+1]=bufferEE[i*2+1];
		}
		for(i=valid=0;i<48;i++) if(memCONFIG[i]<0xFF) valid=1;
		if(valid){
			PrintMessage(strings[S_ConfigMem]);				//"\r\nCONFIG memory:\r\n"
			for(i=0;i<48;i+=4){
				d=(memCONFIG[i+1]<<8)+memCONFIG[i];
				if(i<36||d<0xFFFF)PrintMessage2("0xF800%02X: 0x%04X\r\n",i/2,d);
			}
		}
		if(size) PrintMessage(strings[S_CodeMem]);	//"\r\nCODE memory:\r\n"
		DisplayCODE24F(size);
		if(sizeEE){			//show eeprom with address offset by 0x7FF000
			PrintMessage(strings[S_EEMem]);	//"\r\nEEPROM memory:\r\n"
			DisplayEE24F();
		}
		PrintMessage("\r\n");
	}
//**************** ATxxxx *******************************************
	else if(devType==AVR){
		unsigned char buffer[0x30000];
		memset(buffer,0xFF,sizeof(buffer));
		for(;fgets(line,256,f);){
			if(strlen(line)>9&&line[0]==':'){
				int hex_count = htoi(line+1, 2);
				if((int)strlen(line) - 11 < hex_count * 2) {
					PrintMessage1(strings[S_IhexShort],line);	//"Intel hex8 line too short:\r\n%s\r\n"
				}
				else{
					input_address=htoi(line+3,4);
					sum=0;
					for (i=1;i<=hex_count*2+9;i+=2)
						sum += htoi(line+i,2);
					if ((sum & 0xff)!=0) {
							PrintMessage1(strings[S_IhexChecksum],line);	//"Intel hex8 checksum error in line:\r\n%s\r\n"
					}
					else{
						switch(htoi(line+7,2)){
							case 0:		//Data record
								if(ext_addr<0x20){		//Code
									size=input_address+hex_count;
									for (i=0;i<hex_count;i++){
										buffer[(ext_addr<<16)+input_address+i]=htoi(line+9+i*2,2);
									}
								}
								break;
							case 4:		//extended linear address record
								if(strlen(line)>14)	ext_addr=htoi(line+9,4);
								break;
							default:
								break;
						}
					}
				}
			}
		}
		if(memCODE) free(memCODE);
		memCODE=(unsigned char*)malloc(size);
		memcpy(memCODE,buffer,size);
		if(size) PrintMessage(strings[S_CodeMem]);	//"\r\nmemoria CODICE:\r\n"
		DisplayCODEAVR(size);
		PrintMessage("\r\n");
	}
//**************** 24xxx / 93xxx / 25xxx / 95xxx / DSxxxx /11xxx *******************************
	else if(devType==I2CEE||devType==SPIEE||devType==UWEE||devType==OWEE||devType==UNIOEE){
		if(strstr(loadfile,".bin")||strstr(loadfile,".BIN")){
			#ifdef _WIN32
			//brain-damaged op. systems need this to avoid messing with some bytes
			f=freopen(loadfile,"rb",f);
			if(!f) return -1;
			#endif
			fseek(f, 0L, SEEK_END);
			sizeEE=ftell(f);
			fseek(f, 0L, SEEK_SET);
			if(sizeEE>0x1000000) sizeEE=0x1000000;	//max 16MB
			if(memEE) free(memEE);
			memEE=(unsigned char*)malloc(sizeEE);
			sizeEE=fread(memEE,1,sizeEE,f);
		}
		else{			//Hex file
			int bufSize=0x40000;	//256K
			unsigned char *bufferEE=(unsigned char*)malloc(bufSize);
			memset(bufferEE,0xFF,bufSize);
			for(;fgets(line,256,f);){
				if(strlen(line)>9&&line[0]==':'){
					int hex_count = htoi(line+1, 2);
					if((int)strlen(line) - 11 < hex_count * 2) {
							PrintMessage1(strings[S_IhexShort],line);	//"Intel hex8 line too short:\r\n%s\r\n"
					}
					else{
						input_address=htoi(line+3,4);
						sum=0;
						int end1;
						for (i=1;i<=hex_count*2+9;i+=2) sum+=htoi(line+i,2);
						if ((sum & 0xff)!=0) {
								PrintMessage1(strings[S_IhexChecksum],line);	//"Intel hex8 checksum error in line:\r\n%s\r\n"
						}
						else{
							switch(htoi(line+7,2)){
								case 0:		//Data record
									end1=(ext_addr<<16)+input_address+hex_count;
									if(end1>=0x1000000) break; //max 16MB
									if(sizeEE<end1){			//grow array
										sizeEE=end1;
									}
									if(bufSize<=end1){			//grow buffer
										int newsize=(end1&0xFFFC0000)+0x40000;
										bufferEE=(unsigned char*)realloc(bufferEE,newsize);
										memset(bufferEE+bufSize,0xFF,newsize-bufSize);
										bufSize=newsize;
									}
									for (i=0;i<hex_count;i++){
										bufferEE[(ext_addr<<16)+input_address+i]=htoi(line+9+i*2,2);
									}
									break;
								case 4:		//extended linear address record
									if(strlen(line)>14)	ext_addr=htoi(line+9,4);
									break;
								default:
									break;
							}
						}
					}
				}
			}
			if(memEE) free(memEE);
			memEE=(unsigned char*)malloc(sizeEE);
			memcpy(memEE,bufferEE,sizeEE);
			free(bufferEE);
		}
		DisplayEE();	//visualize
		int sum=0;
		for(i=0;i<sizeEE;i++) sum+=memEE[i];
		PrintMessage1("Checksum: 0x%X\r\n",sum&0xFFFF);
		PrintMessage("\r\n");
	}
	fclose(f);
	return 0;
}

void LoadEE(int devType,char*loadfile){
	FILE* f=fopen(loadfile,"r");
	if(!f) return;
	int i;
//**************** ATMEL *******************************************
	if(devType==AVR){
		char line[256];
		int input_address=0,ext_addr=0;
		unsigned char bufferEE[0x1000];
		PrintMessage1("%s :\r\n\r\n",loadfile);
		memset(bufferEE,0xFF,sizeof(bufferEE));
		for(;fgets(line,256,f);){
			if(strlen(line)>9&&line[0]==':'){
				int hex_count = htoi(line+1, 2);
				if((int)strlen(line) - 11 < hex_count * 2) {
					PrintMessage1(strings[S_IhexShort],line);	//"Intel hex8 line too short:\r\n%s\r\n"
				}
				else{
					input_address=htoi(line+3,4);
					int sum = 0;
					for (i=1;i<=hex_count*2+9;i+=2)	sum+=htoi(line+i,2);
					if ((sum & 0xff)!=0) {
						PrintMessage1(strings[S_IhexChecksum],line);	//"Intel hex8 checksum error in line:\r\n%s\r\n"
					}
					else{
						switch(htoi(line+7,2)){
							case 0:		//Data record
								if(ext_addr==0&&input_address<0x1000){	//EEPROM
									for (i=0;i<hex_count;i++){
										bufferEE[input_address+i]=htoi(line+9+i*2,2);
									}
									sizeEE=input_address+hex_count;
								}
								break;
							case 4:		//extended linear address record
								if(strlen(line)>14)	ext_addr=htoi(line+9,4);
								break;
							default:
								break;
						}
					}
				}
			}
		}
		if(memEE) free(memEE);
		memEE=(unsigned char*)malloc(sizeEE);
		memcpy(memEE,bufferEE,sizeEE);
		if(sizeEE) DisplayEE();	//visualize
		PrintMessage("\r\n");
		fclose(f);
	}
}

void OpenLogFile()
{
	logfile=fopen(LogFileName,"w");
	if(!logfile) return;
	fprintf(logfile,_APPNAME " version %s (%s)\n",VERSION,SYSNAME);
	fprintf(logfile,"Firmware version %d.%d.%d\n",FWVersion>>16,(FWVersion>>8)&0xFF,FWVersion&0xFF);
	struct tm * timeinfo;
	time_t rawtime;
	time( &rawtime );                /* Get time as long integer. */
	timeinfo = localtime( &rawtime ); /* Convert to local time. */
	fprintf(logfile,"%s\n", asctime (timeinfo) );
}

void CloseLogFile()
{
	if(logfile)fclose(logfile);
	logfile=0;
}


struct INSTRUCTIONS{
	const char* name;
	int code;
	int params;		//0xMMNN MM=fixed bytes, NN=multiplier (1N=bits, 0N=bytes)
	int retParams;	//idem
} inst[]={
	{"NOP"				,0x00,0,0},
	{"PROG_RST"			,0x01,0,10},
	{"PROG_ID"			,0x02,0,6},
	{"CHECK_INS"		,0x03,1,1},
	{"FLUSH"			,0x04,0,-1},	//no echo
	{"VREG_EN"			,0x05,0,0},
	{"VREG_DIS"			,0x06,0,0},
	{"SET_PARAMETER"	,0x07,3,0},
	{"WAIT_T1"			,0x08,0,0},
	{"WAIT_T2"			,0x09,0,0},
	{"WAIT_T3"			,0x0A,0,0},
	{"WAIT_US"			,0x0B,1,0},
	{"READ_ADC"			,0x0C,0,2},
	{"SET_VPP"			,0x0D,1,1},
	{"EN_VPP_VCC"		,0x0E,1,0},
	{"SET_CK_D"			,0x0F,1,0},
	{"READ_PINS"		,0x10,0,1},
	{"LOAD_CONF"		,0x11,2,0},
	{"LOAD_DATA_PROG"	,0x12,2,0},
	{"LOAD_DATA_DATA"	,0x13,2,0},
	{"READ_DATA_PROG"	,0x14,0,2},
	{"READ_DATA_DATA"	,0x15,0,1},
	{"INC_ADDR"			,0x16,0,0},
	{"INC_ADDR_N"		,0x17,1,0},
	{"BEGIN_PROG"		,0x18,0,0},
	{"BULK_ERASE_PROG"	,0x19,0,0},
	{"END_PROG"			,0x1A,0,0},
	{"BULK_ERASE_DATA"	,0x1B,0,0},
	{"END_PROG2"		,0x1C,0,0},
	{"ROW_ERASE_PROG"	,0x1D,0,0},
	{"BEGIN_PROG2"		,0x1E,0,0},
	{"CUST_CMD"			,0x1F,1,0},
	{"PROG_C"			,0x20,2,1},
	{"CORE_INS"			,0x21,2,0},
	{"SHIFT_TABLAT"		,0x22,0,1},
	{"TABLE_READ"		,0x23,0,1},
	{"TBLR_INC_N"		,0x24,1,0x101},    //->1+NB
	{"TABLE_WRITE"		,0x25,2,0},
	{"TBLW_INC_N"		,0x26,0x102,0},	//+1+2NB	
	{"TBLW_PROG"		,0x27,4,0},
	{"TBLW_PROG_INC"	,0x28,4,0},
	{"SEND_DATA"		,0x29,3,0},
	{"READ_DATA"		,0x2A,1,1},
	{"I2C_INIT"			,0x2B,1,0},
	{"I2C_READ"			,0x2C,3,0x101},	//->1+NB
	{"I2C_WRITE"		,0x2D,0x301},	//+3+NB ->1B
	{"I2C_READ2"		,0x2E,4,0x101},	//->1+NB
	{"SPI_INIT"			,0x2F,1,0},
	{"SPI_READ"			,0x30,1,0x101},	//->1+NB
	{"SPI_WRITE"		,0x31,0x101,1},	//+1+NB ->1B
	{"EXT_PORT"			,0x32,2,0},
	{"AT_READ_DATA"		,0x33,3,0x102}, 	//->1+2NB
	{"AT_LOAD_DATA"		,0x34,0x302,1},	//+3+2NB ->1B
	{"CLOCK_GEN"		,0x35,1,0},
	{"SIX"				,0x36,3,0},
	{"REGOUT"			,0x37,0,2},
	{"ICSP_NOP"			,0x38,0,0},
	{"TX16"				,0x39,0x102,0},	//+1+2NB
	{"RX16"				,0x3A,1,0x102},	//->1+2NB
	{"uW_INIT"			,0x3B,0,0},
	{"uWTX"				,0x3C,0x111,0},	//+1+NB
	{"uWRX"				,0x3D,1,0x101},  //->+1+NB
	{"SIX_LONG"			,0x3E,3,0},
	{"SIX_N"			,0x3F,0x103,0},	//+1+3NB
	{"OW_RESET"			,0x40,0,1},
	{"OW_WRITE"			,0x41,0x101,0},	//+1+NB
	{"OW_READ"			,0x42,1,0x101},	//+1B   ->1+NB
	{"UNIO_STBY"		,0x43,0,0},
	{"UNIO_COM"			,0x44,0x201,0x101},	//+2+NB ->1+NB
	{"SET_PORT_DIR"		,0x45,2,0},
	{"READ_B"			,0x46,0,1},
	{"READ_AC"			,0x47,0,1},
	{"AT_HV_RTX"		,0x48,0x102,1},	//+1+2NB ->1B
	{"SIX_LONG5"		,0x49,3,0},
	{"LOAD_PC"			,0x50,2,0},
	{"LOAD_DATA_INC"	,0x51,2,0},
	{"READ_DATA_INC"	,0x52,0,2},
	{"JTAG_SET_MODE"	,0x53,1,0},
	{"JTAG_SEND_CMD"	,0x54,1,0},
	{"JTAG_XFER_DATA"	,0x55,4,4},
	{"JTAG_XFER_F_DATA"	,0x56,4,4},
	{"ICSP8_SHORT"		,0x57,1,0},
	{"ICSP8_READ"		,0x58,1,2},
	{"ICSP8_LOAD"		,0x59,3,0},
	{"SPI_TEST"			,0xEF,2,2},
	{"READ_RAM"			,0xF0,2,3},
	{"WRITE_RAM"		,0xF1,3,3},
	{"LOOP"				,0xF2,0,0},
	{"TBLRD"			,0xF3,3,2},
	{"TBLWT"			,0xF4,5,0},
	{"REPEAT"			,0xF5,1,0},
	{"REPEAT_END"		,0xF6,0,0},
	{"ERROR"			,0xFE,0,0},
};

//Parse input and output buffer and "decompile" instructions
//Write to log file
void WriteLogParsedIO()
{
	int i,u,r,p;
	for(u=0,r=0;u<DIMBUF;u++){
		for(i=0;i<sizeof(inst)/sizeof(struct INSTRUCTIONS);i++){
			if(bufferU[u]==inst[i].code){
				fprintf(logfile,"%02X %s",bufferU[u],inst[i].name);
				if(inst[i].params<0x100){
					for(p=0;p<inst[i].params;p++){
						u++;
						fprintf(logfile," %02X",bufferU[u]);
					}
				}
				else{	//A+N*x
					int N=bufferU[u+1];
					int A=inst[i].params>>8;
					int x=inst[i].params&0xF;
					int bit=inst[i].params&0x10?1:0;	//bit multiplier
					int tot;
					if(bit==0) tot=A+N*x;
					else tot=A+(((N*x-1)>>3)+1); //convert bit count to byte
					for(p=0;p<tot;p++){
						u++;
						fprintf(logfile," %02X",bufferU[u]);
					}
				}
				if(inst[i].retParams==-1) u=DIMBUF;	//stop parsing after FLUSH
				else if(inst[i].retParams<0x100){
					if(bufferI[r]==inst[i].code){
						r++;
						fprintf(logfile," ->");
						if(inst[i].retParams==0) fprintf(logfile," OK");	//echo only
						for(p=0;p<inst[i].retParams;p++){
							if(p>15&&p%22==0) fprintf(logfile,"\n\t\t\t\t");
							fprintf(logfile," %02X",bufferI[r++]);
						}
					}
					else fprintf(logfile," -> NAK");
				}
				else{	//A+N*x
					if(bufferI[r]==inst[i].code){
						fprintf(logfile," ->");
						int N=bufferI[r+1];
						int A=inst[i].retParams>>8;
						int x=inst[i].retParams&0xF;
						int bit=inst[i].retParams&0x10?1:0;	//bit multiplier
						int tot;
						if(bit==0) tot=A+N*x;
						else tot=A+(((N*x-1)>>3)+1); //convert bit count to byte
						if(N>0xF0) tot=A;	//in case of errors
						for(p=0;p<tot;p++){
							r++;
							if(p>15&&p%22==0) fprintf(logfile,"\n\t\t\t\t");
							fprintf(logfile," %02X",bufferI[r]);
						}
						r++;
					}
					else fprintf(logfile," -> NAK");
				}
				fprintf(logfile,"\n");
				break;
			}
		}
		if(i>=sizeof(inst)/sizeof(struct INSTRUCTIONS)){
			fprintf(logfile,"%02X ??\n",bufferU[u]);
		}
	}
	
}

void WriteLogIO()
{
	int i;
	fprintf(logfile,"bufferU=[");
	for(i=0;i<DIMBUF;i++){
		if(i%32==0) fprintf(logfile,"\n");
		fprintf(logfile,"%02X ",bufferU[i]);
	}
	fprintf(logfile,"]\n");
	fprintf(logfile,"bufferI=[");
		for(i=0;i<DIMBUF;i++){
		if(i%32==0) fprintf(logfile,"\n");
		fprintf(logfile,"%02X ",bufferI[i]);
	}
	fprintf(logfile,"]\n");
	WriteLogParsedIO();
}

