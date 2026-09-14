#include "inventory.h"
#include "hooks.h"
#include <iomanip>

const char* name = "Inventory";

ImColor backgroundQuadColor[8] =
{
	ImColor(255, 0, 0,140),ImColor(149, 0, 0,140),
	ImColor(39, 255, 0,140),ImColor(22, 140, 0,140),
	ImColor(0, 69, 255,140),ImColor(0, 41, 151,140),
	ImColor(105, 209, 255, 140), ImColor(219, 105, 255, 140)
};

bool AABBCheck(const ImVec2 boxStart, const ImVec2 boxEnd,const POINT cursorPosition)
{
	if (cursorPosition.x > boxStart.x &&
		cursorPosition.y > boxStart.y &&
		cursorPosition.x < boxEnd.x &&
		cursorPosition.y < boxEnd.y)
	{
		return true;
	}

	return false;
}

int16_t selectedItem = -1;
bool displaySelectionWindow = false;
int16_t displayItem = -1;
double clickDelay = 0;

void UseItem(const uint16_t charaID, Item& item)
{
	uintptr_t joker = (uintptr_t)(GetDatUnit(JOKER));
	uintptr_t character = (uintptr_t)(GetDatUnit(charaID));

	uint16_t characterHP = *(uint16_t*)(character + 0xC);
	uint16_t characterSP = *(uint16_t*)(character + 0x10);
	
	bool usedItem = false;

	if (item.teamRestore)
	{
		for (uint8_t i = 1; i <= 10; i++)
		{
			uintptr_t unit = (uintptr_t)(GetDatUnit(i));
			uint16_t unitHP = *(uint16_t*)(unit + 0xC);
			uint16_t unitSP = *(uint16_t*)(unit + 0x10);

			applyEffect(item.skillID, (uintptr_t*)joker,(uintptr_t*)unit);

			if (unitHP != *(uint16_t*)(unit + 0xC) || unitSP != *(uint16_t*)(unit + 0x10))
				usedItem = true;
		}
	}
	else
	{
		applyEffect(item.skillID, (uintptr_t*)joker, (uintptr_t*)character);

		if (characterHP != *(uint16_t*)(character + 0xC) || characterSP != *(uint16_t*)(character + 0x10))
			usedItem = true;
	}

	if (usedItem)
	{
		item.quantity--;
		*reinterpret_cast<uint8_t*>(generalItemBaseAddress + item.type) -= 1;
	}

	if (item.quantity <= 0)
	{
		inventoryItems.erase(inventoryItems.begin() + displayItem);
		displaySelectionWindow = false;
	}
}

float padding = 0;
ImDrawList* windowDraw = nullptr;

void RenderItem(ImVec2 drawCalc1,ImVec2 drawCalc2,const uint8_t itemToDraw)
{
	ImColor textColor = ImColor(255, 255, 255);
	Item* currentItem = &inventoryItems[itemToDraw];

	if (currentItem->statFlags != NO_FLAGS)
	{

		std::vector<uint8_t> backgroundIndexes;
		const uint8_t maxIndex = 8;
		const ImVec2 rectSize = ImVec2(drawCalc2.x - drawCalc1.x, drawCalc2.y - drawCalc1.y);

		for (uint8_t i = 0; i <= maxIndex; i++)
		{
			if (currentItem->statFlags & (1 << i))
			{
				backgroundIndexes.push_back(i);
			}
		}

		float quadSizes = rectSize.y / backgroundIndexes.size();

		for (uint8_t i = 0; i < backgroundIndexes.size() && backgroundIndexes.size() > 1; i++)
		{
			uint8_t backgroundIndex = backgroundIndexes[i];
			ImColor quadColor = backgroundQuadColor[backgroundIndex];
			bool isHalfway = i > backgroundIndexes.size();

			ImVec2 p1,p2,p3,p4;

			p2 = isHalfway ? ImVec2(rectSize.x + (quadSizes * i), 0) : ImVec2(quadSizes * (i + 2), 0);

			if (p2.x > rectSize.x)
			{
				float difference = p2.x - rectSize.x;
				p2.x = rectSize.x;
				p2.y = difference;
			}

			p1 = p2.y == 0 ? ImVec2(p2.x - quadSizes,0) : ImVec2(rectSize.x,p2.y - quadSizes);

			if (p1.y < 0)
			{
				p1.x = rectSize.x + p1.y;
				p1.y = 0;
			}

			p4 = isHalfway ? ImVec2(0, quadSizes * (2 + i)) : ImVec2(0,rectSize.y + (quadSizes * i));

			if (p4.y > rectSize.y)
			{
				float difference = p4.y - rectSize.y;
				p4.y = rectSize.y;
				p4.x = difference;
			}

			p3 = p4.x == 0 ? ImVec2(0, p4.y - quadSizes) : ImVec2(p4.x - quadSizes, rectSize.y);

			if (p3.x < 0)
			{
				p3.y = rectSize.y + p3.x;
				p3.x = 0;
			}

			if (i == 0)
			{
				p1.x = 0;
				p1.y = 0;
			}

			if (i == backgroundIndexes.size() - 1)
			{
				p4.x = rectSize.x;
				p4.y = rectSize.y;
			}

			p1.x += drawCalc1.x;
			p1.y += drawCalc1.y;

			p2.x += drawCalc1.x;
			p2.y += drawCalc1.y;

			p3.x += drawCalc1.x;
			p3.y += drawCalc1.y;

			p4.x += drawCalc1.x;
			p4.y += drawCalc1.y;

			windowDraw->AddQuadFilled(p1, p2, p4, p3, quadColor);
		}

		if (backgroundIndexes.size() == 1)
		{
			ImColor color = backgroundQuadColor[backgroundIndexes[0]];
			windowDraw->AddRectFilled(drawCalc1, drawCalc2, color);
		}
	}

	switch (currentItem->category)
	{
	case GENERIC:
		windowDraw->AddImage((ImTextureID)(intptr_t)genericItemIcon, drawCalc1, drawCalc2);
		break;
	case HP:
//		windowDraw->AddRectFilled(drawCalc1, drawCalc2, ImColor(105, 209, 255, 128));
		windowDraw->AddImage((ImTextureID)(intptr_t)hpItemIcon, drawCalc1, drawCalc2);
		break;
	case SP:
//		windowDraw->AddRectFilled(drawCalc1, drawCalc2, ImColor(219, 105, 255, 128));
		windowDraw->AddImage((ImTextureID)(intptr_t)spItemIcon, drawCalc1, drawCalc2);
		break;
	case TREASURE:
		windowDraw->AddImage((ImTextureID)(intptr_t)treasureItemIcon, drawCalc1, drawCalc2);
		break;
	case INFILTRATION:
		windowDraw->AddImage((ImTextureID)(intptr_t)toolItemIcon, drawCalc1, drawCalc2);
		break;
	case MATERIAL:
		windowDraw->AddImage((ImTextureID)(intptr_t)materialItemIcon, drawCalc1, drawCalc2);
		break;
	default:
		break;
	}

	ImVec2 position = ImVec2(drawCalc1.x + padding, drawCalc1.y + (padding));

	std::string displayText = currentItem->name;
	windowDraw->AddText(inventoryFont, size.y * 0.02f, position, textColor, displayText.c_str(), 0, (drawCalc2.x - (padding * 2)) - drawCalc1.x);

	bool isRestorative = currentItem->category == HP || currentItem->category == SP;

	if (isRestorative)
	{
		displayText = std::to_string(currentItem->restoringEffect);

		if (currentItem->restoreType == PERCENTAGE)
			displayText.push_back('%');
		else
			displayText.insert(displayText.begin(), '+');

		position = ImVec2(drawCalc1.x + padding, drawCalc2.y - (size.y * 0.04f));

		for (int first = -3; first <= 3; first++)
			for (int second = -3; second <= 3; second++)
				if (first != 0 || second != 0)
					windowDraw->AddText(inventoryFont, size.y * 0.02f, ImVec2(position.x + first, position.y + second), ImColor(0, 0, 0), displayText.c_str());

		windowDraw->AddText(inventoryFont, size.y * 0.02f, position, ImColor(255, 255, 255), displayText.c_str());
	}

	displayText = std::to_string(currentItem->quantity);
	uint8_t stringlength = displayText.length() - 1;

	position = ImVec2(drawCalc1.x + padding, drawCalc2.y - (size.y * 0.025f));

	for (int first = -3; first <= 3; first++)
		for (int second = -3; second <= 3; second++)
			if (first != 0 || second != 0)
				windowDraw->AddText(inventoryFont, size.y * 0.02f, ImVec2(position.x + first, position.y + second), ImColor(0, 0, 0), displayText.c_str());

	windowDraw->AddText(inventoryFont, size.y * 0.02f, position, ImColor(255, 255, 255), displayText.c_str());
}

void DrawSelectionWindow(const ImVec2 rectMax)
{
	ImVec2 newWindowPos = rectMax;
	ImVec2 newWindowSize = ImVec2(size.y * 0.25f, size.y * 0.25f);
	ImGui::SetNextWindowPos(newWindowPos);
	ImGui::SetNextWindowSize(newWindowSize);
	ImGui::PushFont(selectFont);
	if (ImGui::Begin("Select",nullptr,ImGuiWindowFlags_NoCollapse))
	{
		uint8_t* joker = (uint8_t*)GetDatUnit(JOKER);
		uint16_t HP = *(uint16_t*)(joker + 0xC);
		uint16_t SP = *(uint16_t*)(joker + 0x10);

		std::string unitButton = protagName;
		unitButton += " HP: " + std::to_string(HP) + " SP: " + std::to_string(SP);
		ImVec2 buttonSize = ImVec2(0, selectFont->FontSize);
		if (ImGui::Button(unitButton.c_str(),buttonSize))
		{
			UseItem(JOKER, inventoryItems[displayItem]);
		}

		for (uint8_t o = 2; o <= 10; o++)
		{
			uint8_t bitPosition = o - 2;
			if (!(*(uint16_t*)partyAvailableBits & (1 << bitPosition)))
				continue;

			uint8_t* unit = (uint8_t*)GetDatUnit(o);
			uint16_t unitHP = *(uint16_t*)(unit + 0xC);
			uint16_t unitSP = *(uint16_t*)(unit + 0x10);

			if (o == 10 && *(BYTE*)sumiName & 1 << 1)
				unitButton = GetNameFromBinary(32, confidantNamesAddress) + " HP: " + std::to_string(unitHP) + " SP: " + std::to_string(unitSP);
			else
				unitButton = GetNameFromBinary(o, confidantNamesAddress) + " HP: " + std::to_string(unitHP) + " SP: " + std::to_string(unitSP);

			if (ImGui::Button(unitButton.c_str(),buttonSize))
			{
				UseItem(o, inventoryItems[displayItem]);
			}
		}
		ImGui::End();
	}

	ImGui::PopFont();
}

uint8_t hoveredSlot = 0;

void ShowInventoryWindow()
{
	if (!menuCheck)
	{
		ImGuiIO& io = ImGui::GetIO();
		// Idk why these are retained when the inventory is closed
		io.MouseDown[ImGuiMouseButton_Left] = false;
		io.MouseDown[ImGuiMouseButton_Right] = false;
		return;
	}

	CURSORINFO ci = { sizeof(CURSORINFO) };

	if (GetCursorInfo(&ci))
	{
		if (ci.flags == 0)
		{
			ShowCursor(true);
		}
	}

	double now = ImGui::GetTime();
	ImGui::SetNextWindowSize(ImVec2(200, 100));
	ImGui::SetNextWindowPos(ImVec2(0, 0));

	if (!ImGui::Begin(name, nullptr,
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBackground))
	{
		ImGui::End();
		return;
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f,1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 5.0f);

	if (!overLimit && ImGui::Button("Original Menu", ImVec2(-1, -1)))
	{
		menuCheck = false;
		ShowCursor(false);
	}

	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar();

	const ImVec2 corners[] =
	{
		ImVec2(size.x * 0.15f,size.y * 0.2f),
		ImVec2(size.x * 0.95f,size.y * 0.2f),
		ImVec2(size.x * 0.85f,size.y * 0.85f),
		ImVec2(size.x * 0.05f,size.y * 0.85f)
	};

	windowDraw = ImGui::GetBackgroundDrawList();
	windowDraw->AddQuadFilled(corners[0], corners[1], corners[2], corners[3], ImColor(0, 0, 0));
	windowDraw->AddPolyline(corners, 4, ImColor(255, 255, 255), ImDrawFlags_Closed, size.y * 0.01f);

	ImVec2 overLimitRectMin = corners[0];
	overLimitRectMin.y += size.y * 0.05f;
	ImVec2 overLimitRectMax;
	overLimitRectMax.x = (overLimitRectMin.x + (size.y * 0.1f));
	overLimitRectMax.y = (overLimitRectMin.y + (size.y * 0.1f));

//	windowDraw->AddText(ImGui::GetFont(), size.y * 0.1f, ImVec2(size.x * 0.45f, size.y * 0.2f), ImColor(255, 255, 255), "Inventory");

	ImVec2 rectMin = corners[0];
	rectMin.y = size.y * 0.2f;
	ImVec2 rectMax = rectMin;
	rectMax.x += size.y * 0.1f;
	rectMax.y += size.y * 0.1f;
	padding = size.y * 0.005f;

	uint8_t itemIndex = 0;

	POINT cursorPosition;
	GetCursorPos(&cursorPosition);
	ScreenToClient(window, &cursorPosition);

	bool overLimitMouseCheck = (overLimit && AABBCheck(overLimitRectMin, overLimitRectMax, cursorPosition));

	if (overLimit && overLimitMouseCheck && ImGui::IsMouseDown(ImGuiMouseButton_Left) && selectedItem == -1 && clickDelay < now)
	{
		selectedItem = MAXCAPACITY;
		inventoryItems[MAXCAPACITY].selected = true;
	}

	if (overLimit)
	{
		if (selectedItem != MAXCAPACITY)
			RenderItem(overLimitRectMin, overLimitRectMax, MAXCAPACITY);

		windowDraw->AddRect(overLimitRectMin, overLimitRectMax, ImColor(1.0f, 0.0f, 0.0f), 0, 0, padding);

		if (AABBCheck(overLimitRectMin, overLimitRectMax, cursorPosition))
		{
			windowDraw->AddRectFilled(overLimitRectMin, overLimitRectMax, ImColor(1.0f, 1.0f, 1.0f, 0.5f));
		}
	}

	for (int i = 0; i < 4; i++)
	{
		rectMin.x = (size.x * 0.15f) - (i * (size.y * 0.05f));
		rectMin.y += i + 1 * (size.y * 0.11f);

		for (int j = 0; j < 9; j++)
		{
			rectMin.x += j * 1 + (size.x * 0.07f);

			rectMax = rectMin;
			rectMax.x += size.y * 0.1f;
			rectMax.y += size.y * 0.1f;

			bool mouseCheck = AABBCheck(rectMin, rectMax, cursorPosition);
			bool shouldDrawItem = false;
			int itemToDraw = -1;

			if (!inventoryItems.empty())
			{
				for (int check = 0; check < inventoryItems.size(); check++)
				{
					if (itemIndex == inventoryItems[check].slot)
					{
						shouldDrawItem = true;
						itemToDraw = check;
						break;
					}
				}
			}

			if (mouseCheck && ImGui::IsMouseDown(ImGuiMouseButton_Left) && !displaySelectionWindow && clickDelay < now)
			{
				
				if (shouldDrawItem && !inventoryItems.at(itemToDraw).selected && selectedItem == -1)
				{
					selectedItem = itemToDraw;
					inventoryItems.at(selectedItem).selected = true;
				}
				else if (selectedItem != -1)
				{
					bool canPlace = true;
					uint8_t indexToSwap = 0xFF;

					for (int oi = 0; oi < inventoryItems.size(); oi++)
					{
						if (oi == selectedItem)
							continue;

						if (inventoryItems.at(oi).slot == itemIndex)
						{
							canPlace = false;
							indexToSwap = oi;
							break;
						}
					}

					if (canPlace)
					{
						inventoryItems.at(selectedItem).slot = itemIndex;
						inventoryItems.at(selectedItem).selected = false;
						selectedItem = -1;
					}
					else
					{
						if (inventoryItems[indexToSwap].type == inventoryItems[selectedItem].type)
						{
							int distanceFromMax = STACK_MAX - inventoryItems[indexToSwap].quantity;
							
							if (distanceFromMax > 0)
							{
								int newQuantity = inventoryItems[selectedItem].quantity - distanceFromMax;

								if (newQuantity <= 0)
								{
									inventoryItems[indexToSwap].quantity += inventoryItems[selectedItem].quantity;
									inventoryItems.erase(inventoryItems.begin() + selectedItem);
									selectedItem = -1;
								}
								else
								{
									inventoryItems[indexToSwap].quantity = STACK_MAX;
									inventoryItems[selectedItem].quantity = newQuantity;
								}
							}
						}
						else
						{
							inventoryItems[indexToSwap].slot = inventoryItems[selectedItem].slot;
							inventoryItems[selectedItem].slot = itemIndex;
							inventoryItems[selectedItem].selected = false;

							if (!overLimit)
							{
								selectedItem = indexToSwap;
								inventoryItems[indexToSwap].selected = true;
							}
							else
							{
								overLimit = false;
								menuCheck = false;
								ShowCursor(false);
								selectedItem = -1;
								uint8_t* itemQuantity = inventoryItems[indexToSwap].category == TREASURE ?
									(uint8_t*)(treasureBaseAddress + inventoryItems[indexToSwap].type) : (uint8_t*)(generalItemBaseAddress + inventoryItems[indexToSwap].type);
								*itemQuantity -= inventoryItems[indexToSwap].quantity;
								inventoryItems.erase(inventoryItems.begin() + indexToSwap);
							}
						}
					}
				}
			}

			if (clickDelay < now && itemToDraw != -1)
			{
				if (mouseCheck && (inventoryItems[itemToDraw].category == HP || inventoryItems[itemToDraw].category == SP) &&
					ImGui::IsMouseDown(ImGuiMouseButton_Right) && !inventoryItems[itemToDraw].isRevivalItem)
				{
					displaySelectionWindow = displaySelectionWindow ? false : true;
					if (displaySelectionWindow)
						displayItem = itemToDraw;
					else
						displayItem = -1;
				}
			}

			if (displaySelectionWindow && displayItem != -1 && itemIndex == inventoryItems[displayItem].slot)
			{
				uint8_t category = inventoryItems[displayItem].category;
				DrawSelectionWindow(rectMax);
			}
			
			itemIndex++;

			if (!inventoryItems.empty() && shouldDrawItem && itemToDraw != selectedItem)
			{
				RenderItem(rectMin, rectMax, itemToDraw);
			}

			if (mouseCheck)
			{
				windowDraw->AddRectFilled(rectMin, rectMax, ImColor(1.0f, 1.0f, 1.0f, 0.5f));
			}

			//windowDraw->AddRect(rectMin, rectMax, ImColor(255, 255, 255), NULL, NULL, padding);

			int sizeX = rectMax.x - rectMin.x;
			int sizeY = rectMax.y - rectMin.y;

			ImVec2 vectors[] =
			{
				ImVec2(rectMin.x + sizeX / 2,rectMin.y),
				ImVec2(rectMax.x,rectMin.y + sizeY / 2),
				ImVec2(rectMin.x + sizeX / 2,rectMax.y),
				ImVec2(rectMin.x, rectMin.y + sizeY / 2)
			};

			windowDraw->AddQuad(vectors[0], vectors[1], vectors[2], vectors[3], ImColor(255, 255, 255),padding);
		}
	}

	if (selectedItem != -1)
	{
		float differenceX = rectMax.x - rectMin.x;
		float differenceY = rectMax.y - rectMin.y;

		rectMin.x = cursorPosition.x;
		rectMin.y = cursorPosition.y;
		rectMax.x = cursorPosition.x + differenceX;
		rectMax.y = cursorPosition.y + differenceY;

		RenderItem(rectMin, rectMax, selectedItem);
	}

	ImVec2 firstCorner = corners[0];
	firstCorner.x = corners[3].x;
	ImVec2 secondCorner = corners[2];
	secondCorner.x = corners[1].x;

	if (clickDelay < now && !AABBCheck(firstCorner, secondCorner, cursorPosition) && selectedItem != -1)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			uint16_t type = inventoryItems[selectedItem].type;
			uint8_t* item = inventoryItems[selectedItem].category == TREASURE ? (uint8_t*)(treasureBaseAddress + type) :
				(uint8_t*)(generalItemBaseAddress + type);
			*item -= inventoryItems[selectedItem].quantity;
			inventoryItems.erase(inventoryItems.begin() + selectedItem);

			if (overLimit && selectedItem == MAXCAPACITY)
			{
				overLimit = false;
				menuCheck = false;
				ShowCursor(false);
			}

			selectedItem = -1;
		}

		if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
		{
			uint16_t type = inventoryItems[selectedItem].type;
			uint8_t* item = inventoryItems[selectedItem].category == TREASURE ? (uint8_t*)(treasureBaseAddress + type) :
				(uint8_t*)(generalItemBaseAddress + type);

			inventoryItems[selectedItem].quantity--;
			*item -= 1;

			if (inventoryItems[selectedItem].quantity <= 0)
			{
				inventoryItems.erase(inventoryItems.begin() + selectedItem);

				if (overLimit && selectedItem == MAXCAPACITY)
				{
					overLimit = false;
					menuCheck = false;
					ShowCursor(false);
				}

				selectedItem = -1;
			}
		}
	}

	bool isButtonDown = ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

	if (clickDelay < now && isButtonDown)
	{
		clickDelay = now + 0.25f;
	}

	ImGui::End();
}