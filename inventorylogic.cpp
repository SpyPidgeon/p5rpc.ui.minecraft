#include "inventorylogic.h"
#include "hooks.h"

std::vector<Item> inventoryItems;

constexpr int GENERICITEMINDEX = 695;
constexpr int TREASUREITEMINDEX = 255;
uint8_t STACK_MAX = 64;
bool includeChocolates = false;

const std::array<uint8_t, 6> spEffects = { 2,3,5,7,9,11 };
const std::array<uint8_t, 6> hpEffects = { 2,3,5,7,9,11 };

std::string GetNameFromBinary(const uint32_t currentIndex, const uintptr_t nameAddress)
{
	unsigned int j = 0;
	unsigned int nameOffset = 0;

	while (j < currentIndex)
	{
		if (*reinterpret_cast<BYTE*>(nameAddress + nameOffset) == 0x00)
		{
			j++;
		}
		nameOffset++;
	}

	std::string binaryName;

	while (*reinterpret_cast<BYTE*>(nameAddress + nameOffset) != 0x00)
	{
		binaryName.push_back((char)*reinterpret_cast<char*>(nameAddress + nameOffset));
		nameOffset++;
	}

	return binaryName;
}

bool SortBySlot(const Item& first, const Item& second)
{
	return first.slot < second.slot;
}

uint8_t GetFirstAvailableSlot()
{
	int slot = 0;

	std::sort(inventoryItems.begin(), inventoryItems.end(), SortBySlot);

	for (auto& item : inventoryItems)
	{
		if (slot != item.slot)
		{
			return slot;
		}

		slot++;
	}

	return slot;
}

uint8_t GetCategory(const uint16_t type)
{
	uintptr_t itemAddress = (genericItemTBLAddress + (type * 0x30)); // Find index for the item type
	uint32_t icon = *reinterpret_cast<uint32_t*>(itemAddress); // Icon is the first four bytes

	uint16_t skillCheck = *reinterpret_cast<uint16_t*>(itemAddress + 0xC); // Checks what skill the item uses
	uint8_t* sphpCheck = reinterpret_cast<uint8_t*>(activeSkillsAddress + (skillCheck * 0x30)); // Check the skill for HP/SP regen
	uint8_t sp = *(sphpCheck - 0x6); // For some reason offset 0 is after the start of the struct, so I get to do this.
	uint8_t hp = *(sphpCheck - 0x9);

	bool isSPRestoring = false;

	for (int j = 0; j < spEffects.size(); j++)
	{
		if (sp == spEffects[j])
		{
			return SP;
		}
	}

	if (!isSPRestoring)
	{
		for (int j = 0; j < hpEffects.size(); j++)
		{
			if (hp == hpEffects[j])
			{
				return HP;
			}
		}
	}

	return GENERIC;
}

void SetCategoryAndRestore(Item& item)
{
	TBLItem* tblItem = (TBLItem*)(genericItemTBLAddress + (item.type * sizeof(TBLItem)));

	switch (tblItem->icon)
	{
	case CRAFTING:
		item.category = MATERIAL;
		return;
		break;
	case TOOLS:
		item.category = INFILTRATION;
		return;
		break;
	}

	TBLActiveSkill* skill = (TBLActiveSkill*)(activeSkillsAddress + (tblItem->skill * sizeof(TBLActiveSkill)) - 0x20);
	item.statFlags = skill->buffsAndDebuffs;

	// These occupy the positions of Accuracy Down and Up, which is not considered for this mod.
	item.statFlags &= ~(HP_ITEM);
	item.statFlags &= ~(SP_ITEM);

	if (skill->numberOfTargets == 1 || skill->numberOfTargets == 2)
		item.teamRestore = true;

	if (skill->targetRestriction == 64)
		item.isRevivalItem = true;

	bool isSPRestoring = false;

	for (int j = 0; j < spEffects.size(); j++)
	{
		if (skill->depleteOrRestoreSP == spEffects[j])
		{
			item.category = SP;
			item.statFlags |= SP_ITEM;
			isSPRestoring = true;
			break;
		}
	}

	for (int j = 0; j < hpEffects.size(); j++)
	{
		if (skill->damageOrHealType == hpEffects[j])
		{
			if (!isSPRestoring)
				item.category = HP;

			item.statFlags |= HP_ITEM;
			break;
		}
	}

	if (item.category == SP)
	{
		item.skillID = tblItem->skill;
		item.restoringEffect = skill->spChange;
		item.restoreType = skill->depleteOrRestoreSP == 9 || skill->depleteOrRestoreSP == 11 ? PERCENTAGE : EXACTVALUE;
	}
	else if (item.category == HP)
	{
		item.skillID = tblItem->skill;
		item.restoringEffect = skill->healOrDamageChange;
		item.restoreType = skill->damageOrHealType == 9 || skill->damageOrHealType == 11 ? PERCENTAGE : EXACTVALUE;
	}
}

void SeparateStacks(Item& item, const int index)
{
	while (item.quantity > STACK_MAX && inventoryItems.size() < MAXCAPACITY)
	{
		Item nextItem = item;
		nextItem.quantity = STACK_MAX;
		item.quantity -= STACK_MAX;
		nextItem.slot = GetFirstAvailableSlot();
		inventoryItems.push_back(nextItem);
	}
}

bool CheckDifferences()
{
	if (inventoryItems.empty())
		return true;

	for (int i = 0; i < inventoryItems.size(); i++)
	{
		uint8_t* item = nullptr;

		if (inventoryItems[i].category == TREASURE)
			item = (uint8_t*)(treasureBaseAddress + inventoryItems[i].type);
		else
			item = (uint8_t*)(generalItemBaseAddress + inventoryItems[i].type);

		if (*item != inventoryItems[i].quantity)
			return true;
	}

	return false;
}

bool SortByPriority(const std::pair<uint16_t, uint16_t>& first, const std::pair<uint16_t, uint16_t>& second)
{
	TBLItem* firstItem = (TBLItem*)(genericItemTBLAddress + (first.first * sizeof(TBLItem)));
	TBLActiveSkill* firstSkill = (TBLActiveSkill*)(activeSkillsAddress + (firstItem->skill * sizeof(TBLActiveSkill) - 0x20));
	TBLItem* secondItem = (TBLItem*)(genericItemTBLAddress + (second.first * sizeof(TBLItem)));
	TBLActiveSkill* secondSkill = (TBLActiveSkill*)(activeSkillsAddress + (secondItem->skill * sizeof(TBLActiveSkill) - 0x20));

	int firstPriority = 0, secondPriority = 0;

	bool isFirstConsumable = firstItem->icon & CONSUMABLES;
	bool isSecondConsumable = secondItem->icon & CONSUMABLES;

	if (isFirstConsumable || isSecondConsumable)
	{
		for (int i = 0; i < spEffects.size(); i++)
		{
			if (firstSkill->depleteOrRestoreSP == spEffects[i])
				firstPriority += 50;
			if (secondSkill->depleteOrRestoreSP == spEffects[i])
				secondPriority += 50;

			if (firstSkill->damageOrHealType == hpEffects[i])
				firstPriority += 20;
			if (secondSkill->damageOrHealType == hpEffects[i])
				secondPriority += 20;
		}
	}

	bool firstIsTool = firstItem->icon & TOOLS;
	bool secondIsTool = secondItem->icon & TOOLS;

	if (firstIsTool)
		firstPriority += 10;
	if (secondIsTool)
		secondPriority += 10;

	for (int i = 0; i < 6; i++)
	{
		bool isEven = i % 2 == 0;

		if (firstSkill->buffsAndDebuffs & (1 << i))
		{
			if ((isEven && firstSkill->alliesOrEnemies & 1 << 0) || (!isEven && firstSkill->alliesOrEnemies & 1 << 1))
				firstPriority += 5;
		}
		if (secondSkill->buffsAndDebuffs & (1 << i))
		{
			if ((isEven && secondSkill->alliesOrEnemies & 1 << 0) || (!isEven && secondSkill->alliesOrEnemies & 1 << 1))
				secondPriority += 5;
		}
	}

	return firstPriority > secondPriority;
}

void GetCurrentInventory()
{
	if (!inventoryItems.empty())
		inventoryItems.clear();

	std::vector<std::pair<uint16_t, uint16_t>> itemIDs;

	for (int i = 0; i <= GENERICITEMINDEX; i++)
	{
		itemIDs.push_back({ i,i });
	}

	std::sort(itemIDs.begin(), itemIDs.end(), SortByPriority);

	// General Items
	for (auto currentItem : itemIDs)
	{
		uint32_t* icon = (uint32_t*)(genericItemTBLAddress + currentItem.second * 0x30);
		uint8_t* indexQuantity = reinterpret_cast<uint8_t*>(generalItemBaseAddress + currentItem.second);

		if (*indexQuantity == 0)
			continue;

		if (!includeChocolates)
		{
			if ((currentItem.second >= 466 && currentItem.second <= 476) || (currentItem.second >= 596 && currentItem.second <= 617))
				continue;
		}

		if (inventoryItems.size() >= MAXCAPACITY)
		{
			*indexQuantity = 0;
			continue;
		}

		uintptr_t itemAddress = (genericItemTBLAddress + (currentItem.second * 0x30));

		if (!(*icon & (CONSUMABLES | CRAFTING | TOOLS)))
		{
			continue;
		}

		Item thisItem = Item(currentItem.second, *indexQuantity, GetNameFromBinary(currentItem.second, genericItemNameAddress));
		SetCategoryAndRestore(thisItem);

		if (thisItem.quantity > STACK_MAX)
		{
			SeparateStacks(thisItem, currentItem.second);
		}

		thisItem.slot = inventoryItems.size();
		inventoryItems.push_back(thisItem);
	}


	// Sellable treasures
	for (int i = 0; i <= TREASUREITEMINDEX; i++)
	{
		uint8_t* indexQuantity = (uint8_t*)(treasureBaseAddress + i);

		if (*indexQuantity == 0)
		{
			continue;
		}

		if (inventoryItems.size() >= MAXCAPACITY)
		{
			*indexQuantity = 0;
			continue;
		}

		Item thisItem = Item(i, *indexQuantity, GetNameFromBinary(i, treasureItemNameAddress));
		thisItem.category = TREASURE;

		if (thisItem.quantity > STACK_MAX)
		{
			SeparateStacks(thisItem, i);
		}

		thisItem.slot = inventoryItems.size();
		inventoryItems.push_back(thisItem);
	}
}

void PlaceNewItem(Item& newItem)
{
	for (int i = 0; i < MAXCAPACITY; i++)
	{
		for (auto item : inventoryItems)
		{
			if (i == item.slot)
				continue;

			newItem.slot = i;
			break;
		}
	}
}

int CheckStacks(const Item& itemCheck)
{
	int stacks = 0;

	for (int i = 0; i < inventoryItems.size(); i++)
	{
		if (inventoryItems[i].type == itemCheck.type)
		{
			stacks++;
		}
	}

	return stacks;
}

void UpdateItem(int type, uint8_t newQuantity, uint8_t category)
{
	uint32_t icon = *(uint32_t*)(genericItemTBLAddress + (type * 0x30));

	if (category != TREASURE)
	{
		if (!(icon & (CONSUMABLES | TOOLS | CRAFTING)))
		{
			originalSetGeneralItemNum(type, newQuantity, 99);
			return;
		}
	}

	uint8_t* currentQuantity = category == TREASURE ? (uint8_t*)(treasureBaseAddress + type) : (uint8_t*)(generalItemBaseAddress + type);
	int16_t quantityDifference = newQuantity - *currentQuantity;

	if (!savedItemNumParams.empty() && icon & (CONSUMABLES | TOOLS) && newQuantity > *currentQuantity)
	{
		savedItemNumParams.pop_back();
	}

	if (quantityDifference == 0)
		return;

	for (Item& item : inventoryItems)
	{
		if (quantityDifference == 0)
			return;

		if (item.type == type)
		{
			if (quantityDifference < 0)
			{
				int16_t distFromMin = item.quantity + quantityDifference;
				int16_t subValue = item.quantity - distFromMin;

				if (subValue < 0)
				{
					*currentQuantity -= item.quantity;
					quantityDifference += item.quantity;
					item.quantity = 0;
					continue;
				}

				quantityDifference += subValue;
				item.quantity -= subValue;
				if (item.quantity == 0)
				{
					std::swap(item, inventoryItems.back());
					inventoryItems.pop_back();
				}
				*currentQuantity -= subValue;
				continue;
			}

			uint8_t distFromMax = STACK_MAX - item.quantity;

			if (distFromMax > quantityDifference)
				distFromMax -= distFromMax - quantityDifference;

			quantityDifference -= distFromMax;
			item.quantity += distFromMax;
			*currentQuantity += distFromMax;
		}
	}

	if (quantityDifference > 0)
	{
		Item newStack = Item(type, quantityDifference);
		newStack.name = category == TREASURE ? GetNameFromBinary(type, treasureItemNameAddress) : GetNameFromBinary(type, genericItemNameAddress);
		newStack.category = category;
		*currentQuantity += newStack.quantity;

		if (category != TREASURE)
		{
			SetCategoryAndRestore(newStack);
		}

		if (inventoryItems.size() < MAXCAPACITY)
		{
			if (newStack.quantity > STACK_MAX)
			{
				SeparateStacks(newStack, type);
			}

			if (newStack.quantity > STACK_MAX)
			{
				// There should never be a time during gameplay in which this happens, this is just in case.
				uint8_t distFromMax = newStack.quantity - STACK_MAX;
				newStack.quantity = STACK_MAX;
				*currentQuantity -= distFromMax;
			}

			if (inventoryItems.size() < MAXCAPACITY)
			{
				newStack.slot = GetFirstAvailableSlot();
				inventoryItems.push_back(newStack);
			}
			else if (icon & (CONSUMABLES | TOOLS))
			{
				overLimit = true;
				menuCheck = true;
				newStack.slot = MAXCAPACITY + 1;
				inventoryItems.push_back(newStack);
				return;
			}
		}
		else if (icon & (CONSUMABLES | TOOLS))
		{
			overLimit = true;
			menuCheck = true;
			newStack.slot = MAXCAPACITY + 1;
			inventoryItems.push_back(newStack);
			return;
		}
	}
}