/*-----------------------------------------------------------------------

    PiictoChat - PictoChat client for the Nintendo Wii

	This file is part of https://github.com/abdelali221/PiictoChat.

	Copyright (C) 2026 Abdelali221

	Based from (as in code inspired by) Wii DS ROM Sender by FIX94 

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

-----------------------------------------------------------------------*/

#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <ogc/lwp_watchdog.h>
#include <wiiuse/wpad.h>
#include <gccore.h>

#include "wd.h"

#define VER "0.1a"

static volatile bool refresh_beacon = false;
static volatile bool beaconActive = false;
static volatile bool recv_frame_active = false;

#define STACKSIZE 0x8000

static uint8_t beacon_stack[STACKSIZE];
static uint8_t recv_frame_stack[STACKSIZE];

static lwp_t beacon_thread_ptr;
static lwp_t recv_frame_thread_ptr;

static void hexdump(const void *_src, u32 length)
{
	const u8 *src = (u8*)_src;
	u32 i;

	for (i=0; i < length; i++)
	{
		printf("%02x", src[i]);
		if ((i&15)==15)
			printf("\n");
		else if ((i&3)==3)
			printf(" ");
	}
	if (i & 15)
		printf("\n");
}
/*
void wdDoSend(uint8_t *send_buf, u32 in_len, uint8_t* conf)
{

	uint16_t cTime = (uint16_t)(ticks_to_microsecs(gettick())/64);
	s32 ret = WD_ChangeVTSF(cTime);
	if(ret < 0)	printf("WD_ChangeVTSF=0x%x\n", ret);
	ret = WD_MpSendFrame(send_buf, in_len, send_buf + 0x200, 0x10);
	if(ret) printf("SendFrame Err 0x%x\n", ret);
}
*/

typedef struct PictoChat_beacon {
	uint32_t header;
	uint32_t id;
	uint16_t timer;
	uint16_t unk0;
	uint32_t id_2;
	uint16_t status;
	uint16_t random_id;
	uint8_t current_room;
	uint8_t users_connected;
	uint16_t unk1;
} PictoChat_beacon;

PictoChat_beacon *pictochat_beacon;

static void* beacon_thread(void * nul)
{

	uint16_t beaconIn = (uint16_t)(ticks_to_microsecs(gettick())/64);
	printf("\nBeacon thread startup\n");
	printf("\nStarting beacon");
	uint8_t beacon_data[0x30];
	memcpy(beacon_data, pictochat_beacon, 0x18);
	int ret = WD_ChangeBeacon(beaconIn, beacon_data, 0x20);
	if(ret < 0)	printf("WD_StartBeacon=0x%x\n", ret);
	putchar('.');
	ret = WD_SetLinkState(1);
	if(ret < 0)	printf("WD_SetLinkState=0x%x\n", ret);
	putchar('.');

	//WD_GetLinkState
	do {
		ret = WD_GetLinkState();
	} while(ret == 0);
	if(ret != 1) printf("WD_GetLinkState=0x%x\n", ret);
	printf(".Done.");
	while(beaconActive)
	{
		if(refresh_beacon)
		{
			//beaconIn = (uint16_t)(ticks_to_microsecs(gettick())/64);
			//WD_ChangeBeacon(beaconIn, beacon_data, 0x20);
			//uint8_t *dat = wdGameInfo;
			//wdDoSend(dat, 0x10, wdSendBufconf);
		}
		usleep(200*1000);
	}
	printf("\nBeacon thread shutdown...\n");
	ret = WD_SetLinkState(0);
	if(ret < 0)	printf("WD_SetLinkState=0x%x\n", ret);

	do {
		ret = WD_GetLinkState();
	} while(ret == 1);
	if(ret != 0) printf("WD_GetLinkState=0x%x\n", ret);

	return nul;
}

uint8_t wdRecvFrame[0x2000] __attribute__((aligned(32)));

static void* recv_frame_thread(void * nul)
{
	printf("\nReceive thread startup");
	while(recv_frame_active)
	{
		//s32 ret = WD_ReceiveFrame(wdRecvFrame, 0x2000);
		WD_RecvNotification(wdRecvFrame, 0x2000);
		printf("\x1b[2J");
		hexdump(wdRecvFrame, 0x20);
		memset((void*)wdRecvFrame, 0, 0x20);
	}
	printf("\nReceive thread shutdown");
	return nul;
}
/*
uint16_t UTF8toUTF16(char chr) 
{
	return chr << 8;
}

void u8strtou16(char* src, uint16_t* dest, int n)
{
	for(int i = 0; i < n; i++) {
		dest[i] = src[i] << 8;
	}
}

void ParseScanBuff(u8* ScanBuff, u16 ScanBuffSize)
{
    u16 APs = ScanBuff[0] << 8 | ScanBuff[1];
    BSSDescriptor* ptr = (BSSDescriptor*)((u32)ScanBuff + 2);

    // We check if the first two bytes of the buffer aren't 0.
    // The bytes makes a 16 bit value that represents the number of APs detected.
    if(APs) {
        printf("Found %d APs", APs);
    } else {
        printf("No APs were found.");
    }
    
    for (size_t i = 0; i < APs; i++)
    {
        printf("\n\n  AP %d", i + 1);

        printf("\n\tSSID : %s", ptr->SSID);

        printf("\n\tBSSID : %02X:%02X:%02X:%02X:%02X:%02X",
            ptr->BSSID[0],
            ptr->BSSID[1],
            ptr->BSSID[2],
            ptr->BSSID[3],
            ptr->BSSID[4],
            ptr->BSSID[5]
        );
        
        printf("\n\tStrength : ");
        switch(WD_GetRadioLevel(ptr)) {
            case WD_SIGNAL_STRONG:
                printf("Strong");
            break;

            case WD_SIGNAL_NORMAL:
                printf("Normal");
            break;

            case WD_SIGNAL_FAIR:
                printf("Fair");
            break;

            case WD_SIGNAL_WEAK:
                printf("Weak");
            break;
        }

        printf("\n\tChannel : %d", ptr->channel);

        u8 Security = WD_GetSecurity(ptr);
        printf("\n\tSecurity : %X ", Security);

        if(Security != WD_OPEN) {
            if(Security & WD_WEP) {
                printf("WEP ");
            }
            if(Security & WD_WPA_AES) {
                printf("WPA-AES ");
            }

            if(Security & WD_WPA_TKIP)
                printf("WPA-TKIP ");

            if(Security & WD_WPA2_AES)
                printf("WPA2-AES ");

            if(Security & WD_WPA2_TKIP)
                printf("WPA2-TKIP ");            
        }

        int game_IE_len = WD_GetVendorSpecificIELength(ptr, OUI_GAME);
        printf("\n\tGame_IE_len : %d\n", game_IE_len);
        
        if(game_IE_len > 0) {
            uint8_t IEbuff[game_IE_len];
            WD_GetVendorSpecificIE(ptr, OUI_GAME, IEbuff, game_IE_len);
            hexdump(IEbuff, game_IE_len);
        } 

        // Sometimes length can be 0, which is wrong.
        // And in that case we can use ptr->IEs_length to get the correct length. 
        if (ptr->length == 0) {
            if((ptr->IEs_length + 0x3E) % 2 == 0) {
                ptr = (BSSDescriptor*)((u32)ptr + ptr->IEs_length + 0x3E);
            } else {
                ptr = (BSSDescriptor*)((u32)ptr + ptr->IEs_length + 0x3F);
            }            
        } else { // If it's not then we just use it as it is.
            // Doubling ptr->length seems to match the BSSDescriptor length + IEs_length.
            ptr = (BSSDescriptor*)((u32)ptr + ptr->length*2);
        }
        if(APs > 1 && APs > i + 1) {
            printf("\n\tPress A to get the next AP...");
            while(1) {
                WPAD_ScanPads();
                u32 pressed = WPAD_ButtonsDown(0);
                if (pressed & WPAD_BUTTON_A) break;
            }
        }
    }
}
*/
int main() 
{
	void *xfb = NULL;
	GXRModeObj *rmode = NULL;
	VIDEO_Init();
	rmode = VIDEO_GetPreferredMode(NULL);
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
	VIDEO_Configure(rmode);
	VIDEO_SetNextFramebuffer(xfb);
	VIDEO_SetBlack(FALSE);
	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE)
		VIDEO_WaitVSync();
	//console_init(xfb,24,32,rmode->fbWidth-32,rmode->xfbHeight-48,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	CON_InitEx(rmode, 24, 32, rmode->fbWidth-32, rmode->xfbHeight-48);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	WPAD_Init();

	printf("\nPiict%cChat v%s\nCreated by Abdelali221\n", 1, VER);
	
	u32 out = 0;
	s32 kd_fd = IOS_Open("/dev/net/kd/request", 0);
	if(kd_fd < 0)
	{
		printf("KD Open Err %d\n", kd_fd);
		VIDEO_WaitVSync();
		return 0;
	}
	IOS_Ioctl(kd_fd, 1, NULL, 0, &out, 4);
	IOS_Close(kd_fd);

	printf("\nLocking Wireless Driver...");

	s32 lockid = NCD_LockWirelessDriver();
	if(lockid < 0)
	{
		printf("NCDLockWirelessDriver failed: lockid=0x%x\n", lockid);
		VIDEO_WaitVSync();
		return 0;
	}
	printf("Done.\n");
	int ret = 0;
	printf("\nStarting WD...");
	//WD_Startup
	ret = WD_Init(DSCommunications);
	if(ret < 0)	printf("WD_Init: 0x%x\n", ret);
	printf("Done.");

	printf("\n\nPlease select desired room : <A>");
	uint8_t room = 0;
	while(1) {
		WPAD_ScanPads();
		uint32_t wdown = WPAD_ButtonsDown(0);
		if(wdown & WPAD_BUTTON_RIGHT) {
			room = (room + 1) & 3;
			printf("\rPlease select desired room : <%c>", 'A' + room);
		} else if(wdown & WPAD_BUTTON_LEFT) {
			room = (room - 1) & 3;
			printf("\rPlease select desired room : <%c>", 'A' + room);
		} else if(wdown & WPAD_BUTTON_A)
			break;		
	}

	uint8_t n_players = 0;
	printf("\rPlease select how many players : <01>");
	while(1) {
		WPAD_ScanPads();
		uint32_t wdown = WPAD_ButtonsDown(0);
		if(wdown & WPAD_BUTTON_RIGHT) {
			n_players = (n_players + 1) & 0xF;
			printf("\rPlease select how many players : <%02d>", n_players + 1);
		} else if(wdown & WPAD_BUTTON_LEFT) {
			n_players = (n_players - 1) & 0xF;
			printf("\rPlease select how many players : <%02d>", n_players + 1);
		} else if(wdown & WPAD_BUTTON_A)
			break;		
	}
	/*
	ScanParameters settings;
	WD_SetDefaultScanParameters(&settings);
	uint8_t scanbuff[4096]; 
	WD_Scan(&settings, scanbuff, 4096);

	ParseScanBuff(scanbuff, 4096);
	*/
	WD_Config conf __attribute__((aligned(32))) = {0};
	
	conf.mpParent.connectionTimeout = 4; //Timeout 4s?
	conf.mpParent.beaconPeriod = 0xC8; //beacon period 200ms
	conf.mpParent.maxNodes = 0xF; //max 15 nodes
	switch(room) {
		case 0:
			conf.mpParent.channel = 0x1;
		break;

		case 1:
		case 3:
			conf.mpParent.channel = 0x7;
		break;
		
		case 2:
			conf.mpParent.channel = 0xD;
		break;
	}
	
	
	//uint64_t mask __attribute__((aligned(32))) = 0x00000000003F0000; //default cfg mask
	uint64_t mask __attribute__((aligned(32))) = 0x0003007F00000000; //default cfg mask
	printf("\nSetting Config...");
	ret = WD_SetConfig(&conf, mask);
	if(ret < 0)	printf("WD_SetConfig=0x%x\n", ret);
	printf("Done.");

	PictoChat_beacon beacon_dat = {0};

	beacon_dat.header = 0x01000108;
	beacon_dat.id = 0x00000000;
	beacon_dat.id_2 = 0xc000c000;
	beacon_dat.unk0 = 0x0801;
	beacon_dat.status = 0x4823;
	beacon_dat.current_room = room;
	beacon_dat.users_connected = n_players + 1;
	beacon_dat.unk1 = 0x0400;

	uint16_t rval = gettick();
	beacon_dat.random_id = (rval>>8) | (rval&0xFF);

	pictochat_beacon = &beacon_dat;

	printf("\nPress A to start beacon...");
	
	//mpdl_active = true;
	//Beacon Thread
	//LWP_CreateThread(&beacon_thread_ptr,beacon_thread,NULL,beacon_stack,STACKSIZE,0x40);
	//LWP_CreateThread(&recv_frame_thread_ptr,recv_frame_thread,NULL,recv_frame_stack,STACKSIZE,0x40);

	while(1)
	{
		WPAD_ScanPads();
		uint32_t wdown = WPAD_ButtonsDown(0);
		if(wdown & WPAD_BUTTON_HOME)
			break;
		if(wdown & WPAD_BUTTON_A && !beaconActive) {
			beaconActive = true;
			LWP_CreateThread(&beacon_thread_ptr,beacon_thread,NULL,beacon_stack,STACKSIZE,0x40);
		}
		if(wdown & WPAD_BUTTON_B && beaconActive) {
			beaconActive = false;
			LWP_JoinThread(beacon_thread_ptr, NULL);
		}
		if(wdown & WPAD_BUTTON_1 && !recv_frame_active) {
			recv_frame_active = true;
			LWP_CreateThread(&recv_frame_thread_ptr,recv_frame_thread,NULL,recv_frame_stack,STACKSIZE,0x80);
		}
		if(wdown & WPAD_BUTTON_2 && recv_frame_active) {
			recv_frame_active = false;
			LWP_JoinThread(recv_frame_thread_ptr, NULL);
		}
		VIDEO_WaitVSync();
	}
	if(recv_frame_active) {
		recv_frame_active = false;
		LWP_JoinThread(recv_frame_thread_ptr, NULL);
	}
	if(beaconActive) {
		beaconActive = false;
		LWP_JoinThread(beacon_thread_ptr, NULL);
	}
	
	//WD_Cleanup
	WD_Deinit();

	NCD_UnlockWirelessDriver(lockid);

	printf("\nExiting, goodbye\n");
	return 0;
}
