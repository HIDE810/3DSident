#include <stdarg.h>
#include <stdlib.h>

#include <3ds.h>

#include "ac.h"
#include "actu.h"
#include "C2D_helper.h"
#include "common.h"
#include "config.h"
#include "hardware.h"
#include "kernel.h"
#include "misc.h"
#include "storage.h"
#include "system.h"
#include "utils.h"
#include "wifi.h"

#define DISTANCE_Y  20
#define MENU_Y_DIST 18
#define MAX_ITEMS   8

static bool display_info = false;
static int item_height = 0;
static char kernel_version[100], system_version[100], firm_version[100], initial_version[0xB], nand_lfcs[0xB];
static u32 sd_titles = 0, nand_titles = 0, tickets = 0;

static void Menu_DrawItem(int x, int y, float size, char *item_title, const char* text, ...)
{
	float title_width = 0.0f;
	Draw_GetTextSize(size, &title_width, NULL, item_title);
	Draw_Text(x, y, size, MENU_INFO_TITLE_COLOUR, item_title);
	
	char buffer[256];
	va_list args;
	va_start(args, text);
	vsnprintf(buffer, 256, text, args);
	Draw_Text(x + title_width + 5, y, size, MENU_INFO_DESC_COLOUR, buffer);
	va_end(args);
}

static void Menu_Kernel(void)
{
	Draw_Rect(10, 27, 380, 146, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 144, MENU_BAR_COLOUR);
	Menu_DrawItem(15, 30, 0.5f, "カーネルバージョン:", kernel_version);
	Menu_DrawItem(15, 50, 0.5f, "FIRMバージョン:", firm_version);
	Menu_DrawItem(15, 70, 0.5f, "システムバージョン:", system_version);
	Menu_DrawItem(15, 90, 0.5f, "初期システムバージョン:", initial_version);
	Menu_DrawItem(15, 110, 0.5f, "SDMC CID:", display_info? Kernel_GetSDMCCID() : NULL);
	Menu_DrawItem(15, 130, 0.5f, "NAND CID:", display_info? Kernel_GetNANDCID() : NULL);
	Menu_DrawItem(15, 150, 0.5f, "デバイスID:", "%lu", display_info? Kernel_GetDeviceId() : 0);
}

static void Menu_System(void)
{
	Draw_Rect(10, 27, 380, 146, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 144, MENU_BAR_COLOUR);
	Menu_DrawItem(15, 30, 0.5f, "モデル:", "%s (%s - %s)", System_GetModel(), System_GetRunningHW(), System_GetRegion());
	Menu_DrawItem(15, 50, 0.5f, "言語:", System_GetLang());
	Menu_DrawItem(15, 70, 0.5f, "ECSデバイスID:", "%llu", display_info? System_GetSoapId() : 0);
	Menu_DrawItem(15, 90, 0.5f, "オリジナルLFCS:", "%010llX", display_info? System_GetLocalFriendCodeSeed() : 0);
	Menu_DrawItem(15, 110, 0.5f, "NAND LFCS:", "%s", display_info? nand_lfcs : NULL);
	Menu_DrawItem(15, 130, 0.5f, "MACアドレス:", display_info? System_GetMacAddress() : NULL);
	Menu_DrawItem(15, 150, 0.5f, "シリアルナンバー:", display_info? System_GetSerialNumber() : NULL);
}

static void Menu_Battery(void)
{
	Draw_Rect(10, 27, 380, 126, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 124, MENU_BAR_COLOUR);
	
	Result ret = 0;
	u8 battery_percent = 0, battery_status = 0, battery_volt = 0, fw_ver_high = 0, fw_ver_low = 0;
	bool is_connected = false;

	ret = MCUHWC_GetBatteryLevel(&battery_percent);
	Menu_DrawItem(15, 30, 0.5f, "バッテリー残量:", "%3d%%", R_FAILED(ret)? 0 : (battery_percent));
	
	ret = PTMU_GetBatteryChargeState(&battery_status);
	Menu_DrawItem(15, 50, 0.5f, "バッテリー状態:", R_FAILED(ret)? NULL : (battery_status? "充電中" : "通常"));
	
	if (R_FAILED(ret = MCUHWC_GetBatteryVoltage(&battery_volt)))
		Menu_DrawItem(15, 70, 0.5f, "バッテリー電圧:", "%d (%.1f V)", 0, 0);
	else
		Menu_DrawItem(15, 70, 0.5f, "バッテリー電圧:", "%d (%.1f V)", battery_volt, 5.0 * ((double)battery_volt / 256.0));

	ret = PTMU_GetAdapterState(&is_connected);
	Menu_DrawItem(15, 90, 0.5f, "アダプター状態:", R_FAILED(ret)? NULL : (is_connected? "接続中" : "未接続"));

	if ((R_SUCCEEDED(MCUHWC_GetFwVerHigh(&fw_ver_high))) && (R_SUCCEEDED(MCUHWC_GetFwVerLow(&fw_ver_low))))
		Menu_DrawItem(15, 110, 0.5f, "MCUファームウェア:", "%u.%u", (fw_ver_high - 0x10), fw_ver_low);
	else
		Menu_DrawItem(15, 110, 0.5f, "MCUファームウェア:", "0.0");

	Menu_DrawItem(15, 130, 0.5f, "省電力モード:", Config_IsPowerSaveEnabled()? "ON" : "OFF");
}

static void Menu_NNID(void)
{
	Draw_Rect(10, 27, 380, 126, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 124, MENU_BAR_COLOUR);
	
	Result ret = 0;
	AccountDataBlock accountDataBlock;
	Result accountDataBlockRet = ACTU_GetAccountDataBlock((u8*)&accountDataBlock, 0xA0, 0x11);

	u32 principalID = 0;
	char country[0x3], name[0x16], nnid[0x11], timeZone[0x41];
	
	ret = ACTU_GetAccountDataBlock(nnid, 0x11, 0x8);
	Menu_DrawItem(15, 30, 0.5f, "NNID:", R_FAILED(ret)? NULL : (display_info? nnid : NULL));

	ret = ACTU_GetAccountDataBlock(&principalID, 0x4, 0xC);
	Menu_DrawItem(15, 50, 0.5f, "Principal ID:", "%u", R_FAILED(ret)? 0 : (display_info? principalID : 0));

	Menu_DrawItem(15, 70, 0.5f, "Persistent ID:", "%u", R_FAILED(accountDataBlockRet)? 0 : (display_info? accountDataBlock.persistentID : 0));
	Menu_DrawItem(15, 90, 0.5f, "Transferable ID Base:", "%llu", R_FAILED(accountDataBlockRet)? 0 : (display_info? accountDataBlock.transferableID : 0));
	
	ret = ACTU_GetAccountDataBlock(country, 0x3, 0xB);
	Menu_DrawItem(15, 110, 0.5f, "国:", R_FAILED(ret)? NULL : (display_info? country : NULL));
	
	ret = ACTU_GetAccountDataBlock(timeZone, 0x41, 0x1E);
	Menu_DrawItem(15, 130, 0.5f, "タイムゾーン:", R_FAILED(ret)? NULL : (display_info? timeZone : NULL));
}

static void Menu_Config(void)
{
	Draw_Rect(10, 27, 380, 126, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 124, MENU_BAR_COLOUR);
	
	char username[0x14];
	wcstombs(username, Config_GetUsername(), sizeof(username));

	Menu_DrawItem(15, 30, 0.5f, "ユーザー名: ", username);
	Menu_DrawItem(15, 50, 0.5f, "誕生日:", display_info? Config_GetBirthday() : NULL);
	Menu_DrawItem(15, 70, 0.5f, "EULAバージョン:", Config_GetEulaVersion());
	Menu_DrawItem(15, 90, 0.5f, "ペアレンタルコントロールキー:", display_info? Config_GetParentalPin() : NULL);
	Menu_DrawItem(15, 110, 0.5f, "メールアドレス:", display_info? Config_GetParentalEmail() : NULL);
	Menu_DrawItem(15, 130, 0.5f, "秘密の質問の答え:", display_info? Config_GetParentalSecretAnswer() : NULL);
}

static void Menu_Hardware(void)
{
	Draw_Rect(10, 27, 380, 126, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 124, MENU_BAR_COLOUR);
	
	Result ret = 0;

	Menu_DrawItem(15, 30, 0.5f, "スクリーンタイプ:", System_GetScreenType());
	Menu_DrawItem(15, 50, 0.5f, "ヘッドフォン:", Hardware_GetAudioJackStatus());
	Menu_DrawItem(15, 70, 0.5f, "カードスロット:", Hardware_GetCardSlotStatus());
	Menu_DrawItem(15, 90, 0.5f, "SDカード:", Hardware_DetectSD());
	Menu_DrawItem(15, 110, 0.5f, "音声出力:", Config_GetSoundOutputMode());

	if (Utils_IsN3DS())
	{
		Menu_DrawItem(15, 130, 0.5f, "明るさレベル:", "%s (明るさ自動調整 %s)", Hardware_GetBrightness(GSPLCD_SCREEN_TOP), 
			Config_IsAutoBrightnessEnabled()? "ON" : "OFF");
	}
	else
		Menu_DrawItem(15, 130, 0.5f, "明るさレベル:", Hardware_GetBrightness(GSPLCD_SCREEN_TOP));

}

static void Menu_Misc(void)
{
	Draw_Rect(10, 27, 380, 106, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(11, 28, 378, 104, MENU_BAR_COLOUR);
	
	Result ret = 0;
	Menu_DrawItem(15, 30, 0.5f, "インストール済タイトル:", "SD: %lu (NAND: %lu)", sd_titles, nand_titles);
	Menu_DrawItem(15, 50, 0.5f, "インストール済チケット:", "%lu", tickets);

	u64 homemenuID = 0;
	ret = APT_GetAppletInfo(APPID_HOMEMENU, &homemenuID, NULL, NULL, NULL, NULL);
	Menu_DrawItem(15, 70, 0.5f, "ホームメニューID:", "%016llX", (R_FAILED(ret))? ret : homemenuID);

	double wifi_signal_percent = (osGetWifiStrength() * 33.3333333333);
	Menu_DrawItem(15, 90, 0.5f, "WiFi受信強度:", "%d (%.0lf%%)", osGetWifiStrength(), wifi_signal_percent);
	
	char hostname[128];
	ret = gethostname(hostname, sizeof(hostname));
	if (display_info)
		Menu_DrawItem(15, 110, 0.5f, "IP:", hostname);
	else
		Menu_DrawItem(15, 110, 0.5f, "IP:", NULL);

}

static void Menu_WiFi(void)
{
	char ssid[0x20], passphrase[0x40];
	wifiSlotStructure slotData;

	Draw_Rect(0, 19, 400, 221, BACKGROUND_COLOUR);
	
	if (R_SUCCEEDED(ACI_LoadWiFiSlot(0)))
	{
		Draw_Rect(15, 27, 370, 70, MENU_INFO_TITLE_COLOUR);
		Draw_Rect(16, 28, 368, 68, MENU_BAR_COLOUR);
		
		Draw_Text(20, 30, 0.45f, MENU_INFO_DESC_COLOUR, "WiFiスロット 1:");

		if (R_SUCCEEDED(ACI_GetSSID(ssid)))
			Menu_DrawItem(20, 46, 0.45f, "SSID:", ssid);
		
		if (R_SUCCEEDED(ACI_GetPassphrase(passphrase)))
			Menu_DrawItem(20, 62, 0.45f, "パスワード:", "%s (%s)", display_info? passphrase : NULL, WiFi_GetSecurityMode());

		if ((R_SUCCEEDED(CFG_GetConfigInfoBlk8(CFG_WIFI_SLOT_SIZE, CFG_WIFI_BLKID, (u8*)&slotData))) && (slotData.set))
		{
			if (display_info)
				Menu_DrawItem(20, 78, 0.45f, "Macアドレス:", "%02X:%02X:%02X:%02X:%02X:%02X", slotData.mac_addr[0], slotData.mac_addr[1], slotData.mac_addr[2], 
					slotData.mac_addr[3], slotData.mac_addr[4], slotData.mac_addr[5]);
			else
				Menu_DrawItem(20, 78, 0.45f, "Macアドレス:", NULL);
		}
	}
	
	if (R_SUCCEEDED(ACI_LoadWiFiSlot(1)))
	{
		Draw_Rect(15, 95, 370, 70, MENU_INFO_TITLE_COLOUR);
		Draw_Rect(16, 96, 368, 68, MENU_BAR_COLOUR);
		
		Draw_Text(20, 98, 0.45f, MENU_INFO_DESC_COLOUR, "WiFiスロット 2:");

		if (R_SUCCEEDED(ACI_GetSSID(ssid)))
			Menu_DrawItem(20, 114, 0.45f, "SSID:", ssid);
		
		if (R_SUCCEEDED(ACI_GetPassphrase(passphrase)))
			Menu_DrawItem(20, 130, 0.45f, "パスワード:", "%s (%s)", display_info? passphrase : NULL, WiFi_GetSecurityMode());

		if ((R_SUCCEEDED(CFG_GetConfigInfoBlk8(CFG_WIFI_SLOT_SIZE, CFG_WIFI_BLKID + 1, (u8*)&slotData))) && (slotData.set))
		{
			if (display_info)
				Menu_DrawItem(20, 146, 0.45f, "Macアドレス:", "%02X:%02X:%02X:%02X:%02X:%02X", slotData.mac_addr[0], slotData.mac_addr[1], slotData.mac_addr[2], 
					slotData.mac_addr[3], slotData.mac_addr[4], slotData.mac_addr[5]);
			else
				Menu_DrawItem(20, 146, 0.45f, "Macアドレス:", NULL);
		}
	}
	
	if (R_SUCCEEDED(ACI_LoadWiFiSlot(2)))
	{
		Draw_Rect(15, 163, 370, 70, MENU_INFO_TITLE_COLOUR);
		Draw_Rect(16, 164, 368, 68, MENU_BAR_COLOUR);
		
		Draw_Text(20, 166, 0.45f, MENU_INFO_DESC_COLOUR, "WiFiスロット 3:");

		if (R_SUCCEEDED(ACI_GetSSID(ssid)))
			Menu_DrawItem(20, 182, 0.45f, "SSID:", ssid);
		
		if (R_SUCCEEDED(ACI_GetPassphrase(passphrase)))
			Menu_DrawItem(20, 198, 0.45f, "パスワード:", "%s (%s)", display_info? passphrase : NULL, WiFi_GetSecurityMode());

		if ((R_SUCCEEDED(CFG_GetConfigInfoBlk8(CFG_WIFI_SLOT_SIZE, CFG_WIFI_BLKID + 2, (u8*)&slotData))) && (slotData.set))
		{
			if (display_info)
				Menu_DrawItem(20, 214, 0.45f, "Macアドレス:", "%02X:%02X:%02X:%02X:%02X:%02X", slotData.mac_addr[0], slotData.mac_addr[1], slotData.mac_addr[2], 
					slotData.mac_addr[3], slotData.mac_addr[4], slotData.mac_addr[5]);
			else
				Menu_DrawItem(20, 214, 0.45f, "Macアドレス:", NULL);
		}
	}
}

static void Menu_Storage(void)
{
	u64 sdUsed = 0, sdTotal = 0, ctrUsed = 0, ctrTotal = 0, twlUsed = 0, twlTotal = 0, twlpUsed = 0, twlpTotal = 0; 

	char sdFreeSize[16], sdUsedSize[16], sdTotalSize[16];
	char ctrFreeSize[16], ctrUsedSize[16], ctrTotalSize[16];
	char twlFreeSize[16], twlUsedSize[16], twlTotalSize[16];
	char twlpFreeSize[16], twlpUsedSize[16], twlpTotalSize[16];

	Utils_GetSizeString(sdFreeSize, Storage_GetFreeStorage(SYSTEM_MEDIATYPE_SD));
	Utils_GetSizeString(sdUsedSize, Storage_GetUsedStorage(SYSTEM_MEDIATYPE_SD));
	Utils_GetSizeString(sdTotalSize, Storage_GetTotalStorage(SYSTEM_MEDIATYPE_SD));
	Utils_GetSizeString(ctrFreeSize, Storage_GetFreeStorage(SYSTEM_MEDIATYPE_CTR_NAND));
	Utils_GetSizeString(ctrUsedSize, Storage_GetUsedStorage(SYSTEM_MEDIATYPE_CTR_NAND));
	Utils_GetSizeString(ctrTotalSize, Storage_GetTotalStorage(SYSTEM_MEDIATYPE_CTR_NAND));
	Utils_GetSizeString(twlFreeSize, Storage_GetFreeStorage(SYSTEM_MEDIATYPE_TWL_NAND));
	Utils_GetSizeString(twlUsedSize, Storage_GetUsedStorage(SYSTEM_MEDIATYPE_TWL_NAND));
	Utils_GetSizeString(twlTotalSize, Storage_GetTotalStorage(SYSTEM_MEDIATYPE_TWL_NAND));
	Utils_GetSizeString(twlpFreeSize, Storage_GetFreeStorage(SYSTEM_MEDIATYPE_TWL_PHOTO));
	Utils_GetSizeString(twlpUsedSize, Storage_GetUsedStorage(SYSTEM_MEDIATYPE_TWL_PHOTO));
	Utils_GetSizeString(twlpTotalSize, Storage_GetTotalStorage(SYSTEM_MEDIATYPE_TWL_PHOTO));

	Draw_Rect(0, 20, 400, 220, BACKGROUND_COLOUR);

	sdUsed = Storage_GetUsedStorage(SYSTEM_MEDIATYPE_SD);
	sdTotal = Storage_GetTotalStorage(SYSTEM_MEDIATYPE_SD);
	Draw_Rect(15, 31, 185, 95, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(16, 32, 183, 93, MENU_BAR_COLOUR);
	Draw_Rect(20, 50, 60, 10, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(21, 51, 58, 8, MENU_BAR_COLOUR);
	Draw_Rect(21, 51, (((double)sdUsed / (double)sdTotal) * 58.00), 8, MENU_SELECTOR_COLOUR);
	Draw_Text(20, 34, 0.45f, MENU_INFO_DESC_COLOUR, "SD");
	Menu_DrawItem(20, 71, 0.45f, "残量:", sdFreeSize);
	Menu_DrawItem(20, 87, 0.45f, "使用中:", sdUsedSize);
	Menu_DrawItem(20, 103, 0.45f, "総容量:", sdTotalSize);

	ctrUsed = Storage_GetUsedStorage(SYSTEM_MEDIATYPE_CTR_NAND);
	ctrTotal = Storage_GetTotalStorage(SYSTEM_MEDIATYPE_CTR_NAND);
	Draw_Rect(200, 31, 185, 95, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(201, 32, 183, 93, MENU_BAR_COLOUR);
	Draw_Rect(205, 50, 60, 10, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(206, 51, 58, 8, MENU_BAR_COLOUR);
	Draw_Rect(206, 51, (((double)ctrUsed / (double)ctrTotal) * 58.00), 8, MENU_SELECTOR_COLOUR);
	Draw_Text(205, 34, 0.45f, MENU_INFO_DESC_COLOUR, "CTR NAND");
	Menu_DrawItem(205, 71, 0.45f, "残量:", ctrFreeSize);
	Menu_DrawItem(205, 87, 0.45f, "使用中:", ctrUsedSize);
	Menu_DrawItem(205, 103, 0.45f, "総容量:", ctrTotalSize);

	twlUsed = Storage_GetUsedStorage(SYSTEM_MEDIATYPE_TWL_NAND);
	twlTotal = Storage_GetTotalStorage(SYSTEM_MEDIATYPE_TWL_NAND);
	Draw_Rect(15, 126, 185, 95, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(16, 127, 183, 93, MENU_BAR_COLOUR);
	Draw_Rect(20, 145, 60, 10, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(21, 146, 58, 8, MENU_BAR_COLOUR);
	Draw_Rect(21, 146, (((double)twlUsed / (double)twlTotal) * 58.00), 8, MENU_SELECTOR_COLOUR);
	Draw_Text(20, 129, 0.45f, MENU_INFO_DESC_COLOUR, "TWL NAND");
	Menu_DrawItem(20, 166, 0.45f, "残量:", twlFreeSize);
	Menu_DrawItem(20, 182, 0.45f, "使用中:", twlUsedSize);
	Menu_DrawItem(20, 198, 0.45f, "総容量:", twlTotalSize);

	twlpUsed = Storage_GetUsedStorage(SYSTEM_MEDIATYPE_TWL_PHOTO);
	twlpTotal = Storage_GetTotalStorage(SYSTEM_MEDIATYPE_TWL_PHOTO);
	Draw_Rect(200, 126, 185, 95, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(201, 127, 183, 93, MENU_BAR_COLOUR);
	Draw_Rect(205, 145, 60, 10, MENU_INFO_TITLE_COLOUR);
	Draw_Rect(206, 146, 58, 8, MENU_BAR_COLOUR);
	Draw_Rect(206, 146, (((double)twlpUsed / (double)twlpTotal) * 58.00), 8, MENU_SELECTOR_COLOUR);
	Draw_Text(205, 129, 0.45f, MENU_INFO_DESC_COLOUR, "TWL Photo");
	Menu_DrawItem(205, 166, 0.45f, "残量:", twlpFreeSize);
	Menu_DrawItem(205, 182, 0.45f, "使用中:", twlpUsedSize);
	Menu_DrawItem(205, 198, 0.45f, "総容量:", twlpTotalSize);
}

static int touchButton(touchPosition *touch, int selection)
{
	if (touch->px >= 16 && touch->px <= 304 && touch->py >= 16 && touch->py <= 35)
		selection = 0;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 36 && touch->py <= 55)
		selection = 1;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 56 && touch->py <= 75)
		selection = 2;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 76 && touch->py <= 95)
		selection = 3;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 96 && touch->py <= 115)
		selection = 4;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 116 && touch->py <= 135)
		selection = 5;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 136 && touch->py <= 155)
		selection = 6;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 156 && touch->py <= 175)
		selection = 7;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 176 && touch->py <= 195)	
		selection = 8;
	else if (touch->px >= 16 && touch->px <= 304 && touch->py >= 196 && touch->py <= 215)	
		selection = 9;
	
	return selection;
}

void Menu_Main(void)
{
	int selection = 0;
	display_info = true;
	touchPosition touch;

	strcpy(kernel_version, Kernel_GetVersion(0));
	strcpy(firm_version, Kernel_GetVersion(1));
	strcpy(initial_version, Kernel_GetVersion(2));
	strcpy(system_version, Kernel_GetVersion(3));
	strcpy(nand_lfcs, System_GetNANDLocalFriendCodeSeed());

	sd_titles = Misc_TitleCount(MEDIATYPE_SD);
	nand_titles = Misc_TitleCount(MEDIATYPE_NAND);
	tickets = Misc_TicketCount();

	while (aptMainLoop()) 
	{
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(RENDER_TOP, BACKGROUND_COLOUR);
		C2D_TargetClear(RENDER_BOTTOM, BACKGROUND_COLOUR);
		C2D_SceneBegin(RENDER_TOP);

		Draw_Rect(0, 0, 400, 20, STATUS_BAR_COLOUR);
		Draw_Textf(5, (20 - Draw_GetTextHeight(0.5f, "3DSident_JPN v0.8.0"))/2, 0.5f, BACKGROUND_COLOUR, "3DSident_JPN v%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_MICRO);

		switch(selection)
		{
			case 0:
				Menu_Kernel();
				break;
			case 1:
				Menu_System();
				break;
			case 2:
				Menu_Battery();
				break;
			case 3:
				Menu_NNID();
				break;
			case 4:
				Menu_Config();
				break;
			case 5:
				Menu_Hardware();
				break;
			case 6:
				Menu_WiFi();
				break;
			case 7:
				Menu_Storage();
				break;
			case 8:
				Menu_Misc();
				break;
		}

		C2D_SceneBegin(RENDER_BOTTOM);

		Draw_Rect(15, 15, 290, 180, MENU_INFO_TITLE_COLOUR);
		Draw_Rect(16, 16, 288, 178, MENU_BAR_COLOUR);
		Draw_Rect(16, 16 + (DISTANCE_Y * selection), 288, 18, MENU_SELECTOR_COLOUR);

		Draw_Text(22, 18, 0.5f, selection == 0? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "カーネル");
		Draw_Text(22, 38, 0.5f, selection == 1? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "システム");
		Draw_Text(22, 58, 0.5f, selection == 2? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "バッテリー");
		Draw_Text(22, 78, 0.5f, selection == 3? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "NNID");
		Draw_Text(22, 98, 0.5f, selection == 4? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "構成情報");
		Draw_Text(22, 118, 0.5f, selection == 5? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "ハードウェア");
		Draw_Text(22, 138, 0.5f, selection == 6? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "WiFi");
		Draw_Text(22, 158, 0.5f, selection == 7? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "ストレージ");
		Draw_Text(22, 178, 0.5f, selection == 8? ITEM_SELECTED_COLOUR : ITEM_COLOUR, "その他");
		
		Draw_Rect(15, 202, 290, 22, MENU_INFO_TITLE_COLOUR);
		Draw_Rect(16, 203, 288, 20, MENU_BAR_COLOUR);
		Draw_Text(22, 205, 0.5f, ITEM_COLOUR, "SELECT:ユーザー固有情報非表示 | START:終了");

		Draw_EndFrame();

		hidScanInput();
		hidTouchRead(&touch);
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();

		selection = touchButton(&touch, selection);

		if (kDown & KEY_DDOWN)
			selection++;
		else if (kDown & KEY_DUP)
			selection--;

		if (selection > MAX_ITEMS) 
			selection = 0;
		if (selection < 0) 
			selection = MAX_ITEMS;

		if (kDown & KEY_SELECT)
			display_info = !display_info;
		
		if (kDown & KEY_START) break;
	}
}