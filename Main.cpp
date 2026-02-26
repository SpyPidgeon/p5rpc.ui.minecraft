#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fstream>

#include "MinHook/MinHook.h"
#if _WIN64 
#pragma comment(lib, "MinHook/libMinHook.x64.lib")
#else
#pragma comment(lib, "MinHook/libMinHook.x86.lib")
#endif

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include "moddefs.h"
#include "signaturescan.h"
#include "hooks.h"
#include <sstream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;


// Globals
HINSTANCE dll_handle;

ImFont* inventoryFont;
ImFont* selectFont;

ID3D11ShaderResourceView* genericItemIcon;
ID3D11ShaderResourceView* treasureItemIcon;
ID3D11ShaderResourceView* hpItemIcon;
ID3D11ShaderResourceView* spItemIcon;
ID3D11ShaderResourceView* materialItemIcon;
ID3D11ShaderResourceView* toolItemIcon;

typedef long(__stdcall* present)(IDXGISwapChain*, UINT, UINT);
present p_present;
present p_present_target;
bool get_present_pointer()
{
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	IDXGISwapChain* swap_chain;
	ID3D11Device* device;

	const D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	if (D3D11CreateDeviceAndSwapChain(
		NULL, 
		D3D_DRIVER_TYPE_HARDWARE, 
		NULL, 
		0, 
		feature_levels, 
		2, 
		D3D11_SDK_VERSION, 
		&sd, 
		&swap_chain, 
		&device, 
		nullptr, 
		nullptr) == S_OK)
	{
		void** p_vtable = *reinterpret_cast<void***>(swap_chain);
		swap_chain->Release();
		device->Release();
		//context->Release();
		p_present_target = (present)p_vtable[8];
		return true;
	}
	return false;
}

WNDPROC oWndProc;
bool endLoop = false;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (window != GetForegroundWindow())
		return false;

	if (menuCheck) 
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
		if (uMsg == WM_INPUT || (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST))
			return FALSE;

		if (uMsg == WM_MOUSEACTIVATE || (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST))
			return FALSE;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HWND window = NULL;
ID3D11Device* p_device = NULL;
ID3D11DeviceContext* p_context = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;

bool LoadTextureFromFile(std::string filename, ID3D11ShaderResourceView** outSRV) 
{
	int width, height, channels;
	filename = GetDLLPath(filename);

	unsigned char* data = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!data) return false;

	D3D11_TEXTURE2D_DESC texDesc = {};
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.SampleDesc.Count = 1;
	texDesc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = data;
	initData.SysMemPitch = width * 4;

	ID3D11Texture2D* pTexture = nullptr;
	p_device->CreateTexture2D(&texDesc, &initData, &pTexture);

	if (pTexture == nullptr)
		return false;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	HRESULT hr = p_device->CreateShaderResourceView(pTexture, &srvDesc, outSRV);
	if (FAILED(hr))
		return false;

	pTexture->Release();
	stbi_image_free(data);
	return true;
}

ImVec2 size;

static long __stdcall detour_present(IDXGISwapChain* p_swap_chain, UINT sync_interval, UINT flags) {
	if (!init) {
		if (SUCCEEDED(p_swap_chain->GetDevice(__uuidof(ID3D11Device), (void**)&p_device)))
		{
			p_device->GetImmediateContext(&p_context);
			DXGI_SWAP_CHAIN_DESC sd;
			p_swap_chain->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			p_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			p_device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(p_device, p_context);

			if (!LoadTextureFromFile("icons\\genericitem.png", &genericItemIcon))
				printf("Failed to create generic item texture!\n");
			if (!LoadTextureFromFile("icons\\hpitem.png", &hpItemIcon))
				printf("Failed to create hp item texture!\n");
			if (!LoadTextureFromFile("icons\\spitem.png", &spItemIcon))
				printf("Failed to create sp item texture!\n");
			if (!LoadTextureFromFile("icons\\treasure.png", &treasureItemIcon))
				printf("Failed to create treasure item texture!\n");
			if (!LoadTextureFromFile("icons\\material.png", &materialItemIcon))
				printf("Failed to create crafting material item texture!\n");
			if (!LoadTextureFromFile("icons\\tool.png", &toolItemIcon))
				printf("Failed to create tool item texture!\n");

			RECT windRect;
			if (GetClientRect(window, &windRect))
			{
				size.x = windRect.right - windRect.left;
				size.y = windRect.bottom - windRect.top;
			}
			
			inventoryFont = io.Fonts->AddFontFromFileTTF(GetDLLPath("font\\seguibl.ttf").c_str(),32.0f);
			selectFont = io.Fonts->AddFontFromFileTTF(GetDLLPath("font\\seguibl.ttf").c_str(), size.y * 0.02f);
			init = true;
		}
		else
			return p_present(p_swap_chain, sync_interval, flags);
	}

	if (!overLimit && !savedItemNumParams.empty())
	{
		uint16_t id = savedItemNumParams.back().first;
		uint8_t quantity = savedItemNumParams.back().second;

		UpdateItem(id, quantity, GENERIC);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ShowInventoryWindow();

	ImGui::EndFrame();
	ImGui::Render();

	p_context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return p_present(p_swap_chain, sync_interval, flags);
}

DWORD __stdcall EjectThread(LPVOID lpParameter) {
	Sleep(100);
	FreeLibraryAndExitThread(dll_handle, 0);
	Sleep(100);
	return 0;
}

std::string GetDLLPath(const std::string& path)
{
	char dllPath[MAX_PATH] = { 0 };
	GetModuleFileNameA(dll_handle, dllPath, MAX_PATH);

	std::string fullPath(dllPath);

	size_t lastSlash = fullPath.find_last_of("\\/");
	std::string baseDirectory = (lastSlash != std::string::npos) ? fullPath.substr(0, lastSlash + 1) : "";

	std::ostringstream ss;
	ss << baseDirectory << path;

	return ss.str();
}

void ParseConfig()
{
	std::ifstream f(GetDLLPath("config.json"));
	json j;

	try
	{
		j = json::parse(f);
	}
	catch (const json::parse_error& e)
	{
		printf("config failed to parse, using default values instead.\n");
		return;
	}

	int stack_max = j["stack_maximum"];

	if (stack_max < 1)
		stack_max = 1;
	else if (stack_max > 255)
		stack_max = 255;

	STACK_MAX = stack_max;
	includeChocolates = j["include_chocolates"];

	auto colorArray = j["hp_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[6] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["sp_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[7] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["attackup_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[0] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["attackdown_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[1] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["speedup_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[2] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["speeddown_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[3] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["defenseup_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[4] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
	colorArray = j["defensedown_color"].get<std::vector<uint8_t>>();
	backgroundQuadColor[5] = ImColor(colorArray[0], colorArray[1], colorArray[2], colorArray[3]);
}

//"main" loop
int WINAPI main()
{
	if (dll_handle == NULL)
		return 1;

	ParseConfig();

	// Prepare signatures
	std::vector<std::pair<std::string, DWORD_PTR*>> patterns;
	std::string pattern;

	DWORD_PTR itemMenuHook;
	pattern = "4C 89 4C 24 20 55 53 56 57 41 54 41 55 41 56 41 57 48 8D 6C 24 E1 48 81 EC E8 00 00 00 C6 02 01 49 8B F9";
	patterns.push_back({ pattern, &itemMenuHook });

	DWORD_PTR setGeneralCall;
	pattern = "E8 ?? ?? ?? ?? B9 0C 30 00 00 E8 ?? ?? ?? ?? 3C 05 73 ?? B9 0C 30 00 00 E8 ?? ?? ?? ?? B9 0C 00 00 00 41 B0 63";
	patterns.push_back({ pattern,&setGeneralCall });

	DWORD_PTR setTreasureNum;
	pattern = "8D 50 ?? 48 8D 05 ?? ?? ?? ??";
	patterns.push_back({ pattern,&setTreasureNum });

	DWORD_PTR initTopMenu;
	pattern = "48 89 54 24 10 48 89 4C 24 08 55 53 56 57 41 55 41 57 48 8D 6C 24 D1 48 81 EC D8 00 00 00 48 8B D9 C6 45 93 01 48 8B 49 08 48 8D 05 ? ? ? ? 33 F6 48 89 45 87 89 75 8F 48 8B FA E8 ? ? ? ? 4C 8B C0 48 85 C0 74 ? 40 38 70 10 74 ? B8 01 00 00 00 F0 41 0F C1 40 14 48 8B 4B 10 4C 89 43 10 48 85 C9 74 ? E8 ? ? ? ? 4C 8B 43 10 48 8D 55 87 49 8B C8 E8 ? ? ? ? 4C 8B 2D";
	patterns.push_back({ pattern,&initTopMenu });

	DWORD_PTR genericItemsTBLPTR;
	pattern = "48 8B 0D ?? ?? ?? ?? 41 FF C0 48 03 CA 48 83 C2 30 8B 01 0F C8 89 01 8B 41 ?? 0F C8 89 41 ?? 0F B7 41 ??";
	patterns.push_back({ pattern,&genericItemsTBLPTR });

	DWORD_PTR itemNamePTR;
	pattern = "48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 49 8B C8 48 89 05 ?? ?? ?? ?? 48 D1 E9 66 89 0D ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 66 3B 1D ?? ?? ?? ?? 44 8B C3 73 ?? 66 66 0F 1F 84 ?? 00 00 00 00 48 8B 05 ?? ?? ?? ?? 41 8B C8 41 FF C0 48 8D 14 ?? 0F B7 04 ?? 66 C1 C0 08 66 89 02 0F B7 05 ?? ?? ?? ?? 44 3B C0 72 ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 48 8B C8 48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 49 8B C8 48 89 05 ?? ?? ?? ?? 48 D1 E9 66 89 0D ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 66 3B 1D ?? ?? ?? ?? 44 8B C3 73 ?? 48 8B 05 ?? ?? ?? ?? 41 8B C8 41 FF C0 48 8D 14 ?? 0F B7 04 ?? 66 C1 C0 08 66 89 02 0F B7 05 ?? ?? ?? ?? 44 3B C0 72 ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 48 8B C8 48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 49 8B C8 48 89 05 ?? ?? ?? ?? 48 D1 E9 66 89 0D ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 66 3B 1D ?? ?? ?? ?? 44 8B C3 73 ?? 66 66 0F 1F 84 ?? 00 00 00 00 48 8B 05 ?? ?? ?? ?? 41 8B C8 41 FF C0 48 8D 14 ?? 0F B7 04 ?? 66 C1 C0 08 66 89 02 0F B7 05 ?? ?? ?? ?? 44 3B C0 72 ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 48 8B C8 48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 8B C2";
	patterns.push_back({ pattern,&itemNamePTR });

	DWORD_PTR confidantNamesPTR;
	pattern = "48 03 1D ?? ?? ?? ?? E9 ?? ?? ?? ?? 48 8B 05 ?? ?? ?? ??";
	patterns.push_back({ pattern,&confidantNamesPTR });

	DWORD_PTR skillsAddress;
	pattern = "48 8D 0D ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 0F 1F 00 8B 41 ??";
	patterns.push_back({ pattern,&skillsAddress });

	DWORD_PTR treasureNamePtr;
	pattern = "48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 49 8B C8 48 89 05 ?? ?? ?? ?? 48 D1 E9 66 89 0D ?? ?? ?? ?? 48 8B C8 E8 ?? ?? ?? ?? 66 3B 1D ?? ?? ?? ?? 44 8B C3 73 ?? 66 66 0F 1F 84 ?? 00 00 00 00 48 8B 05 ?? ?? ?? ?? 41 8B C8 41 FF C0 48 8D 14 ?? 0F B7 04 ?? 66 C1 C0 08 66 89 02 0F B7 05 ?? ?? ?? ?? 44 3B C0 72 ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 41 B8 04 00 00 00 8B C2 1B C9 83 E0 F0 83 E1 10 03 C8 8B C3 2B CA 85 C9 0F 4F C1 48 8D 4D ?? 48 03 45 ?? 48 03 F8 48 8B D7 E8 ?? ?? ?? ?? 8B 45 ?? 48 83 C7 04 0F C8 89 45 ?? 48 8B 4D ?? E8 ?? ?? ?? ?? 4C 8B 45 ?? 48 8B D7 48 8B C8 48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 55 ?? 83 C2 04 8B C2 25 0F 00 00 80 7D ?? FF C8 83 C8 F0 FF C0 F7 D8 8B C2";
	patterns.push_back({ pattern,&treasureNamePtr });
	
	DWORD_PTR bitflagsPtr;
	pattern = "48 8D 05 ?? ?? ?? ?? 48 C1 EA 1C 44 8B C1 48 03 D2 49 C1 E8 05 83 E1 1F 41 81 E0 FF FF 7F 00 48 8B 14 ?? B8 01 00 00 00 48 D3 E0 42 85 04 ?? 0F 95 C0 C3";
	patterns.push_back({ pattern,&bitflagsPtr });

	DWORD_PTR loadItemDataAddress;
	pattern = "40 53 41 57 48 83 EC 28 49 8B D8 44 8B F9 83 F9 0F";
	patterns.push_back({ pattern,&loadItemDataAddress });
	
	DWORD_PTR newGameAddress;
	pattern = "4C 8B DC 49 89 4B 08 48 81 EC E8 00 00 00 49 89 73 F0";
	patterns.push_back({ pattern,&newGameAddress });

	DWORD_PTR mouseStateFunction;
	pattern = "48 8B C4 48 81 EC 98 00 00 00 80 3D ? ? ? ? 00";
	patterns.push_back({ pattern,&mouseStateFunction });

	pattern = "0F B7 C1 48 8D 0D ? ? ? ? 48 69 C0 A0 02 00 00 48 01 C8";
	GetDatUnit = (GetDatUnitByID)PatternScan(GetModuleHandle(NULL), pattern);

	pattern = "40 56 48 83 EC 30 0F B7 41 04";
	GetMaxHP = (GetMaxHPSP)PatternScan(GetModuleHandle(NULL), pattern);
	pattern = "40 53 48 83 EC 30 0F B7 41 04";
	GetMaxSP = (GetMaxHPSP)PatternScan(GetModuleHandle(NULL), pattern);

	pattern = "48 8B C4 48 89 58 10 48 89 68 18 56 57 41 54 41 56 41 57 48 83 EC 40 41 8B D9";
	applyEffect = (ApplySkillEffect)PatternScan(GetModuleHandle(NULL), pattern);

//	pattern = "E8 ? ? ? ? 4C 8B C8 48 89 7C 24 20 4C 8D 05 ? ? ? ? BA 00 01 00 00 48 8B CB E8 ? ? ? ? 48 8B 7C 24 40 48 8B C3";
//	DWORD_PTR getProtagNameHook = PatternScan(GetModuleHandle(NULL), pattern);
//	getProtagNameHook = GetAddressFromFuncCall(getProtagNameHook);

	if (!BulkScan(patterns))
		return 1;

	if (!get_present_pointer()) 
	{
		return 1;
	}

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK)
	{
		return 1;
	}

	if (MH_CreateHook((LPVOID*)loadItemDataAddress, &LoadItemDataHook, (LPVOID*)&oLoadItemData) != MH_OK)
	{
		return 1;
	}

	if (MH_EnableHook((LPVOID*)loadItemDataAddress) != MH_OK)
	{
		return 1;
	}

	/*
	if (MH_CreateHook((LPVOID*)getProtagNameHook, &GetFirstNameHook, (LPVOID*)&GetProtagFirstName) != MH_OK)
		return 1;
	if (MH_EnableHook((LPVOID*)getProtagNameHook) != MH_OK)
		return 1;
		*/
	if (MH_CreateHook((LPVOID*)itemMenuHook, &ItemMenuLogicHook, (LPVOID*)&originalItemMenu) != MH_OK)
		return 1;

	if (MH_EnableHook((LPVOID*)itemMenuHook) != MH_OK)
		return 1;

	if (MH_CreateHook((LPVOID*)initTopMenu, &OnTopMenuStart, (LPVOID*)&originalMenu) != MH_OK)
		return 1;

	if (MH_EnableHook((LPVOID*)initTopMenu) != MH_OK)
		return 1;

	if (MH_CreateHook((LPVOID*)newGameAddress, &NewGameHook, (LPVOID*)&oNewGameFunc) != MH_OK)
		return 1;

	if (MH_EnableHook((LPVOID*)newGameAddress) != MH_OK)
		return 1;

	while (!loadedSave)
	{
		Sleep(50);
	}

	setGeneralCall = GetAddressFromFuncCall(setGeneralCall);
	if (MH_CreateHook((LPVOID*)setGeneralCall, &GenericItemNumHook, (LPVOID*)&originalSetGeneralItemNum) != MH_OK)
	{
		printf("Failed to create general hook.\n");
		return 1;
	}

	if (MH_EnableHook((LPVOID*)setGeneralCall) != MH_OK)
	{
		return 1;
	}

	generalItemBaseAddress = GetAddressFromGlobalRef(setGeneralCall + 0x11);
	genericItemsTBLPTR = GetAddressFromGlobalRef(genericItemsTBLPTR);
	genericItemNameAddress = *(DWORD_PTR*)GetAddressFromGlobalRef(itemNamePTR);

	setTreasureNum += 0x3f;
	setTreasureNum = GetAddressFromGlobalRef(setTreasureNum);

	if (MH_CreateHook((LPVOID*)setTreasureNum, &TreasureNumHook, (LPVOID*)&oSetTreasureItemNum) != MH_OK)
	{
		printf("Failed to create treasure hook.\n");
		return 1;
	}

	if (MH_EnableHook((LPVOID*)setTreasureNum) != MH_OK)
		return 1;

	if (MH_CreateHook((LPVOID*)mouseStateFunction, &MouseStateControlHook, (LPVOID*)&MouseState) != MH_OK)
		return 1;

	if (MH_EnableHook((LPVOID*)mouseStateFunction) != MH_OK)
		return 1;

	treasureBaseAddress = GetAddressFromGlobalRef(setTreasureNum + 0x11);
	treasureNamePtr = GetAddressFromGlobalRef(treasureNamePtr);
	treasureItemNameAddress = *(DWORD_PTR*)treasureNamePtr;

	confidantNamesPTR = GetAddressFromGlobalRef(confidantNamesPTR);

	confidantNamesAddress = *(DWORD_PTR*)confidantNamesPTR;
	while (*(BYTE*)confidantNamesAddress == 0x00)
		confidantNamesAddress++;

	activeSkillsAddress = GetAddressFromGlobalRef(skillsAddress);

	bitflagsPtr = GetAddressFromGlobalRef(bitflagsPtr);
	partyAvailableBits = *(DWORD_PTR*)(bitflagsPtr);
	partyAvailableBits += 0x5c6;
	sumiName = *(DWORD_PTR*)(bitflagsPtr)+0x109;

	genericItemTBLAddress = *(DWORD_PTR*)genericItemsTBLPTR;

	if (MH_CreateHook(reinterpret_cast<void**>(p_present_target), &detour_present, reinterpret_cast<void**>(&p_present)) != MH_OK) {
		return 1;
	}

	if (MH_EnableHook(p_present_target) != MH_OK) {
		return 1;
	}

	printf("Mod loaded successfully!\n");

	return 0;
}

extern "C" _declspec(dllexport) void __stdcall Init()
{
	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
}

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		dll_handle = hModule;

		FILE* newOut;
		freopen_s(&newOut, "CONOUT$", "w", stdout);
	}

	return true;
}