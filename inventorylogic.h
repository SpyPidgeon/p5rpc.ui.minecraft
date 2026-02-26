#pragma once
#include <windows.h>
#include <cstdint>
#include <iostream>
#include <vector>
#include <array>
#include <string>
#include <algorithm>
#include <bitset>
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "moddefs.h"

extern HWND window;

constexpr uint8_t MAXCAPACITY = 36;
extern uint8_t STACK_MAX;
extern bool includeChocolates;

enum CharacterID
{
	JOKER = 1,
	RYUJI,
	MONA,
	ANN,
	YUSUKE,
	MAKOTO,
	HARU,
	FUTABA,
	AKECHI,
	SUMIRE
};

enum ItemCategory
{
	GENERIC,
	HP,
	SP,
	TREASURE,
	INFILTRATION,
	MATERIAL
};

enum ItemRestoreType
{
	EXACTVALUE,
	PERCENTAGE
};

// Don't be like me. (31 is being subtracted because this is in reverse order.)
enum ItemIconBits
{
	NONE = 0,
	ESSENTIAL = 1 << 31 - 2,
	CRAFTING = 1 << 31 - 3,
	TOOLS = 1 << 31 - 4,
	OUTFIT = 1 << 31 - 5,
	CARD = 1 << 31 - 6,
	TREASURES = 1 << 31 - 7,
	KEYITEM = 1 << 31 - 8,
	CONSUMABLES = 1 << 31 - 9
};

enum StatFlags
{
	NO_FLAGS = 0,
	ATTACK_UP = 1 << 0,
	ATTACK_DOWN = 1 << 1,
	SPEED_UP = 1 << 2,
	SPEED_DOWN = 1 << 3,
	DEFENSE_UP = 1 << 4,
	DEFENSE_DOWN = 1 << 5,
	HP_ITEM = 1 << 6,
	SP_ITEM = 1 << 7
};

struct Item
{
	uint16_t type;
	uint8_t quantity;
	uint8_t category = GENERIC;
	uint8_t slot = 0;
	bool selected = false;

	uint16_t skillID = 0;
	int restoringEffect = 0;
	uint8_t restoreType = EXACTVALUE;
	bool isRevivalItem = false;
	bool teamRestore = false;

	uint8_t statFlags = NO_FLAGS;

	std::string name = "";

	Item(int type, int quantity) : type(type), quantity(quantity) {}
	Item(int type, int quantity, std::string name) : type(type), quantity(quantity), name(name) {}

	~Item() {}
};

struct TBLActiveSkill
{
	uint16_t padding;
	BYTE data0[10];
	uint8_t numberOfTargets;
	BYTE alliesOrEnemies;
	uint8_t targetRestriction;
	BYTE data2[8];
	uint8_t damageOrHealType;
	uint16_t healOrDamageChange;
	uint8_t depleteOrRestoreSP;
	uint16_t spChange;
	BYTE data4[6];
	uint8_t buffsAndDebuffs;
	uint8_t data5[10];
};

struct TBLItem
{
	uint32_t icon;
	uint32_t sorting;
	uint16_t flags;
	uint16_t usage;
	uint16_t skill;
	uint16_t RESERVE;
	uint32_t price;
	uint32_t sell;
	uint8_t month;
	uint8_t day;
	uint16_t RESERVE2;
	uint32_t materials[5];
};

void GetCurrentInventory();
void UpdateItem(int type, uint8_t newQuantity, uint8_t category);
uint8_t GetCategory(const uint16_t type);
bool CheckDifferences();
std::string GetNameFromBinary(const uint32_t currentIndex, const uintptr_t nameAddress);
std::string GetDLLPath(const std::string& path);