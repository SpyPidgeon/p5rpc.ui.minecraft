#pragma once
#include "inventorylogic.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

bool LoadTextureFromFile(std::string filename, ID3D11ShaderResourceView** shaderResource);
extern ImVec2 size;

void ShowInventoryWindow();

extern ImFont* inventoryFont;
extern ImFont* selectFont;

extern ImColor backgroundQuadColor[8];

extern ID3D11ShaderResourceView* genericItemIcon;
extern ID3D11ShaderResourceView* treasureItemIcon;
extern ID3D11ShaderResourceView* hpItemIcon;
extern ID3D11ShaderResourceView* spItemIcon;
extern ID3D11ShaderResourceView* materialItemIcon;
extern ID3D11ShaderResourceView* toolItemIcon;