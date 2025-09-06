#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <iostream>
#include <vector>
#include <string>
#include <shlobj.h>
#include <fcntl.h>
#include <io.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

// 保存 CPU 内存的 RGBA 图像为 BMP
bool SaveBitmapToFile(int width, int height, const std::wstring& filename, const std::vector<BYTE>& pixels) {
    BITMAPFILEHEADER bmfHeader = {0};
    BITMAPINFOHEADER bi = {0};

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // 负数避免倒置
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    DWORD dwSizeImage = width * height * 4;

    bmfHeader.bfType = 0x4D42;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = bmfHeader.bfOffBits + dwSizeImage;

    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written;
    WriteFile(hFile, &bmfHeader, sizeof(bmfHeader), &written, nullptr);
    WriteFile(hFile, &bi, sizeof(bi), &written, nullptr);
    WriteFile(hFile, pixels.data(), dwSizeImage, &written, nullptr);
    CloseHandle(hFile);
    return true;
}

// 用于存储搜索结果
struct SearchParam {
    std::wstring keyword;
    HWND hwnd;
    RECT rect;
};

// 查找窗口回调
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (!IsWindowVisible(hwnd)) return TRUE;

    wchar_t title[256] = {0};
    GetWindowTextW(hwnd, title, 256);
    std::wstring str(title);

    SearchParam* p = reinterpret_cast<SearchParam*>(lParam);
    if (str.find(p->keyword) != std::wstring::npos) {
        p->hwnd = hwnd;
        GetWindowRect(hwnd, &p->rect);
        return FALSE; // 找到窗口，停止枚举
    }
    return TRUE;
}

int wmain() {
    // 支持中文输入输出
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);

    std::wcout << L"请输入要截取的窗口名称关键字：";
    std::wstring keyword;
    std::getline(std::wcin, keyword);

    SearchParam param;
    param.keyword = keyword;
    param.hwnd = nullptr;
    param.rect = {0};

    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&param));

    if (!param.hwnd) {
        std::wcerr << L"未找到匹配窗口！\n";
        return -1;
    }

    int winLeft = param.rect.left;
    int winTop = param.rect.top;
    int winWidth = param.rect.right - param.rect.left;
    int winHeight = param.rect.bottom - param.rect.top;

    std::wcout << L"窗口 HWND: " << param.hwnd 
               << L" 位置: (" << winLeft << L"," << winTop
               << L") 大小: " << winWidth << L"x" << winHeight << L"\n";

    // ---- D3D11 截屏 ----
    D3D_FEATURE_LEVEL featureLevel;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        &device, &featureLevel, &context))) {
        std::wcerr << L"D3D11CreateDevice 失败！\n";
        return -1;
    }

    IDXGIDevice* dxgiDevice = nullptr;
    device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);

    IDXGIAdapter* adapter = nullptr;
    dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);

    IDXGIOutput* output = nullptr;
    adapter->EnumOutputs(0, &output);

    IDXGIOutput1* output1 = nullptr;
    output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);

    IDXGIOutputDuplication* duplication = nullptr;
    if (FAILED(output1->DuplicateOutput(device, &duplication))) {
        std::wcerr << L"DuplicateOutput 失败！\n";
        return -1;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* desktopResource = nullptr;
    if (FAILED(duplication->AcquireNextFrame(1000, &frameInfo, &desktopResource))) {
        std::wcerr << L"AcquireNextFrame 失败！\n";
        return -1;
    }

    ID3D11Texture2D* desktopImage = nullptr;
    desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktopImage);

    D3D11_TEXTURE2D_DESC desc;
    desktopImage->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* cpuTex = nullptr;
    device->CreateTexture2D(&desc, nullptr, &cpuTex);
    context->CopyResource(cpuTex, desktopImage);

    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(cpuTex, 0, D3D11_MAP_READ, 0, &mapped))) {
        std::wcerr << L"Map 失败！\n";
        return -1;
    }

    // 裁剪窗口区域
    std::vector<BYTE> windowPixels(winWidth * winHeight * 4);
    for (int y = 0; y < winHeight; y++) {
        memcpy(
            &windowPixels[y * winWidth * 4],
            (BYTE*)mapped.pData + (y + winTop) * mapped.RowPitch + winLeft * 4,
            winWidth * 4
        );
    }

    context->Unmap(cpuTex, 0);

    // 保存到桌面
    wchar_t desktopPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_DESKTOP, nullptr, 0, desktopPath);
    std::wstring filename = std::wstring(desktopPath) + L"\\window_capture.bmp";

    if (SaveBitmapToFile(winWidth, winHeight, filename, windowPixels)) {
        std::wcout << L"窗口截图成功: " << filename << L"\n";
    } else {
        std::wcerr << L"保存失败！\n";
    }

    cpuTex->Release();
    desktopImage->Release();
    desktopResource->Release();
    duplication->Release();
    output1->Release();
    output->Release();
    adapter->Release();
    dxgiDevice->Release();
    context->Release();
    device->Release();

    return 0;
}

