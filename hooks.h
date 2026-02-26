#pragma once
#include "inventory.h"

typedef int(__stdcall* OriginalMenuFunction)(unsigned int* param1, int8_t* param2, long long param3, long long param4);
typedef void(__stdcall* OriginalMainMenuInit)(int64_t param1, int64_t param2);
typedef void(__fastcall* OriginalItemFunction)(unsigned short itemID, unsigned char newItemCount, unsigned char maxItemCount);

extern OriginalMainMenuInit originalMenu;

typedef int64_t(__stdcall* LoadData)(uint32_t param1, uint32_t param2, int64_t param_3);
extern LoadData oLoadItemData;

typedef void* (__stdcall* GetDatUnitByID)(uint16_t ID);
extern GetDatUnitByID GetDatUnit;

typedef int(__fastcall* ApplySkillEffect)(uint16_t skillID, uintptr_t* sendingUnit, uintptr_t* receivingUnit);
extern ApplySkillEffect applyEffect;

typedef uint32_t(__stdcall* GetMaxHPSP)(uintptr_t entityAddress);
extern GetMaxHPSP GetMaxHP;
extern GetMaxHPSP GetMaxSP;

typedef void(__stdcall* MouseStateControls)();
extern MouseStateControls MouseState;

extern OriginalMenuFunction originalItemMenu;
extern OriginalItemFunction originalSetGeneralItemNum;
extern OriginalItemFunction oSetTreasureItemNum;

typedef int64_t(__stdcall* NewGame)(int64_t param1);
extern NewGame oNewGameFunc;

typedef char* (__stdcall* GetName)();
extern GetName GetProtagFirstName;

extern std::string protagName;

extern DWORD_PTR generalItemBaseAddress;
extern DWORD_PTR treasureBaseAddress;
extern std::vector<Item> inventoryItems;
extern uint64_t genericItemTBLAddress;
extern DWORD_PTR genericItemNameAddress;
extern DWORD_PTR treasureItemNameAddress;
extern DWORD_PTR confidantNamesAddress;
extern DWORD_PTR activeSkillsAddress;
extern std::vector<std::pair<uint16_t, uint8_t>> savedItemNumParams;

extern DWORD_PTR partyAvailableBits;
extern DWORD_PTR sumiName;

extern bool menuCheck;
extern bool overLimit;
extern bool loadedSave;

int64_t LoadItemDataHook(uint32_t param1,uint32_t param2,int64_t param3);
void GenericItemNumHook(uint16_t itemID, uint8_t newQuantity, uint8_t maxQuantity);
void TreasureNumHook(uint16_t itemID, uint8_t newQuantity, uint8_t maxQuantity);
int64_t ItemMenuLogicHook(uint32_t* param1, int8_t* param2, int64_t param3, int64_t param4);
void OnTopMenuStart(int64_t param1, int64_t param2);
int64_t NewGameHook(int64_t param1);
//char* GetFirstNameHook();
void MouseStateControlHook();