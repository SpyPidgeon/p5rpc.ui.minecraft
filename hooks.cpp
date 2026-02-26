#include "hooks.h"

OriginalMainMenuInit originalMenu;
LoadData oLoadItemData;
GetDatUnitByID GetDatUnit;
ApplySkillEffect applyEffect;
GetMaxHPSP GetMaxHP;
GetMaxHPSP GetMaxSP;
OriginalMenuFunction originalItemMenu;
OriginalItemFunction originalSetGeneralItemNum;
OriginalItemFunction oSetTreasureItemNum;
NewGame oNewGameFunc;
GetName GetProtagFirstName = nullptr;

std::string protagName = "Protagonist";

DWORD_PTR generalItemBaseAddress;
DWORD_PTR treasureBaseAddress;
uint64_t genericItemTBLAddress;
DWORD_PTR genericItemNameAddress;
DWORD_PTR treasureItemNameAddress;
DWORD_PTR confidantNamesAddress;
DWORD_PTR activeSkillsAddress;
std::vector<std::pair<uint16_t, uint8_t>> savedItemNumParams;

DWORD_PTR partyAvailableBits;
DWORD_PTR sumiName;

MouseStateControls MouseState;

bool menuCheck = false;
bool overLimit = false;
bool loadedSave = false;

int64_t LoadItemDataHook(uint32_t param1, uint32_t param2, int64_t param3)
{
	if (!loadedSave)
	{
		printf("Menu Loaded!\n");
	}

	loadedSave = true;

	while (genericItemTBLAddress == NULL)
		Sleep(50);

	int64_t returnVal = oLoadItemData(param1, param2, param3);
	if (CheckDifferences())
		GetCurrentInventory();

	return returnVal;
}

void GenericItemNumHook(uint16_t itemID, uint8_t newQuantity, uint8_t maxQuantity)
{
	maxQuantity = UINT8_MAX;
	uint8_t* currentQuantity = (uint8_t*)(generalItemBaseAddress + itemID);

	// If additional items cause the value to wrap around to near 0, cap it
	if (UINT8_MAX < *currentQuantity + 10 && newQuantity < STACK_MAX)
	{
		newQuantity = UINT8_MAX;
	}

	uint32_t* icon = (uint32_t*)(genericItemTBLAddress + itemID * 0x30);

	if (newQuantity <= *currentQuantity || !(*icon & (TOOLS | CONSUMABLES)))
	{
		UpdateItem(itemID, newQuantity, GENERIC);
		return;
	}

	if (!savedItemNumParams.empty())
	{
		for (auto &params : savedItemNumParams)
		{
			if (params.first == itemID)
			{
				params.second++;
				return;
			}
		}
	}

	savedItemNumParams.push_back({ itemID,newQuantity });
}

void TreasureNumHook(uint16_t itemID, uint8_t newQuantity, uint8_t maxQuantity)
{
	maxQuantity = UINT8_MAX;
	uint8_t* currentQuantity = (uint8_t*)(generalItemBaseAddress + itemID);

	// If additional items cause the value to wrap around to near 0, cap it
	if (UINT8_MAX < *currentQuantity + 10 && newQuantity < STACK_MAX)
	{
		newQuantity = UINT8_MAX;
	}

	UpdateItem(itemID, newQuantity, TREASURE);
}

int64_t ItemMenuLogicHook(uint32_t* param1, int8_t* param2, int64_t param3, int64_t param4)
{
	if (*param1 == 0)
	{
		menuCheck = true;
	}

	if (*param1 == 0x1a)
	{
		CURSORINFO ci = { sizeof(CURSORINFO) };
		
		if (GetCursorInfo(&ci))
		{
			if (ci.flags == 1)
			{
				ShowCursor(false);
			}
		}
		menuCheck = false;
	}

	return originalItemMenu(param1, param2, param3, param4);
}

void OnTopMenuStart(int64_t param1, int64_t param2)
{
	menuCheck = false;

	CURSORINFO ci = { sizeof(CURSORINFO) };

	if (GetCursorInfo(&ci))
	{
		if (ci.flags == 1)
			ShowCursor(false);
	}
	originalMenu(param1, param2);
}

int64_t NewGameHook(int64_t param1)
{
	loadedSave = true;

	if (!inventoryItems.empty())
		inventoryItems.clear();

	return oNewGameFunc(param1);
}

/*
char* GetFirstNameHook()
{
	char* name = GetProtagFirstName();
	protagName = name;
	return name;
}*/

void MouseStateControlHook()
{
	if (menuCheck)
		return;

	MouseState();
}