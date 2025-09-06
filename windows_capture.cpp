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
    BITMAPFILEHEADER bmfHeader = { 0 };
    BITMAPINFOHEADER bi = { 0 };

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

    HANDLE hFile = CreateFileW(filename.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written;
    WriteFile(hFile, &bmfHeader, sizeof(bmfHeader), &written, NULL);
    WriteFile(hFile, &bi, sizeof(bi), &written, NULL);
    WriteFile(hFile, pixels.data(), dwSizeImage, &written, NULL);
    CloseHandle(hFile);
    return true;
}

int wmain() {
    // 控制台支持中文输出
    _setmode(_fileno(stdout), _O_U16TEXT);

    // 创建 D3D11 设备
    D3D_FEATURE_LEVEL featureLevel;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context))) {
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

    // 获取桌面帧
    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    IDXGIResource* desktopResource = nullptr;
    if (FAILED(duplication->AcquireNextFrame(1000, &frameInfo, &desktopResource))) {
        std::wcerr << L"AcquireNextFrame 失败！\n";
        return -1;
    }

    ID3D11Texture2D* desktopImage = nullptr;
    desktopResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&desktopImage);

    // 把 GPU 图像复制到 CPU 可访问纹理
    D3D11_TEXTURE2D_DESC desc;
    desktopImage->GetDesc(&desc);
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* cpuTex = nullptr;
    device->CreateTexture2D(&desc, nullptr, &cpuTex);
    context->CopyResource(cpuTex, desktopImage);

    // Map CPU 可访问纹理
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (FAILED(context->Map(cpuTex, 0, D3D11_MAP_READ, 0, &mapped))) {
        std::wcerr << L"Map 失败！\n";
        return -1;
    }

    // 保存整张桌面截图
    std::vector<BYTE> fullPixels(desc.Width * desc.Height * 4);
    BYTE* src = (BYTE*)mapped.pData;
    for (int y = 0; y < (int)desc.Height; y++) {
        memcpy(
            &fullPixels[y * desc.Width * 4],
            src + y * mapped.RowPitch,
            desc.Width * 4
        );
    }
    context->Unmap(cpuTex, 0);

    // 保存到桌面
    wchar_t desktopPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath);
    std::wstring filename = std::wstring(desktopPath) + L"\\desktop_full.bmp";

    if (SaveBitmapToFile(desc.Width, desc.Height, filename, fullPixels)) {
        std::wcout << L"整张桌面截图成功: " << filename << L"\n";
    } else {
        std::wcerr << L"保存失败！\n";
    }

    // 释放资源
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
