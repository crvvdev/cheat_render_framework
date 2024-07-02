#include <windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <stdexcept>

#pragma comment(lib, "d3d11.lib")

#include "..\..\factories\dx11\renderer_dx11.hpp"

using namespace CheatRenderFramework;
RendererPtr g_Renderer;
FontHandle g_FontTahoma;

ID3D11Device *g_pd3dDevice = nullptr;
ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
IDXGISwapChain *g_pSwapChain = nullptr;
ID3D11RenderTargetView *g_pRenderTargetView = nullptr;
UINT g_ResizeWidth = 0, g_ResizeHeight = 0;

void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
}

void InitD3D(HWND hWnd)
{
    RECT rc;
    GetClientRect(hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_DEBUG,
                                               featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &sd,
                                               &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pd3dDeviceContext);
    if (FAILED(hr))
    {
        throw std::runtime_error("D3D11CreateDeviceAndSwapChain failed");
    }

    CreateRenderTarget();

    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);

    D3D11_VIEWPORT vp;
    vp.Width = static_cast<FLOAT>(width);
    vp.Height = static_cast<FLOAT>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_pd3dDeviceContext->RSSetViewports(1, &vp);
}

void CleanupDeviceD3D()
{
    if (g_pRenderTargetView)
    {
        g_pRenderTargetView->Release();
    }

    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
    }

    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
    }

    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
    }
}

void CreateRenderTarget()
{
    ID3D11Texture2D *pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));

    HRESULT hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_pRenderTargetView);
    if (FAILED(hr))
    {
        throw std::runtime_error("g_pd3dDevice->CreateRenderTargetView failed");
    }

    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_pRenderTargetView)
    {
        g_pRenderTargetView->Release();
        g_pRenderTargetView = nullptr;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                     "D3D Window",       NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(wc.lpszClassName, "DirectX11 Window", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL,
                             wc.hInstance, NULL);

    try
    {
        InitD3D(hWnd);
    }
    catch (const std::runtime_error &e)
    {
        MessageBox(NULL, e.what(), "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    try
    {
        g_Renderer = std::make_shared<Renderer>(g_pd3dDevice, 4096);
        g_FontTahoma = g_Renderer->AddFont(L"Tahoma", 15, FONT_FLAG_CLEAR_TYPE);
    }
    catch (const std::runtime_error &e)
    {
        MessageBox(NULL, e.what(), "Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
            {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
                g_ResizeWidth = g_ResizeHeight = 0;
                CreateRenderTarget();
            }

            // Rendering code goes here
            g_Renderer->BeginFrame();

            g_Renderer->AddRectFilled(Vec2{10.f, 10.f}, Vec2{10.f + 50.f, 10.f + 50.f}, Color(255, 0, 0));
            g_Renderer->AddRect(Vec2{100.f, 10.f}, Vec2{100.f + 50.f, 10.f + 50.f}, Color(0, 0, 0), 2.f);
            g_Renderer->AddCircle(Vec2{250.f, 40.f}, 32.f, Color(0, 255, 0));
            g_Renderer->AddLine(Vec2{300.f, 40.f}, Vec2{450.f, 45.f}, Color(255, 255, 255));

            g_Renderer->AddText(g_FontTahoma, L"This is a normal test text!", 5.f, 100.f, Color(255, 255, 255));

            g_Renderer->AddText(g_FontTahoma, L"This is a drop shadow test text!", 5.f, 120.f, Color(255, 255, 255),
                                TEXT_FLAG_DROPSHADOW);

            g_Renderer->AddText(g_FontTahoma, L"This is a outline test text!", 5.f, 140.f, Color(255, 255, 255),
                                TEXT_FLAG_OUTLINE);

            g_Renderer->AddText(g_FontTahoma, L"This is a {#FF0000FF}color {#66FF0096}tags {#FFFFFFFF}test text!", 5.f,
                                160.f, Color(255, 255, 255), TEXT_FLAG_COLORTAGS);

            g_pd3dDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, nullptr);
            float clearColor[4] = {0.0f, 0.2f, 0.4f, 1.0f};
            g_pd3dDeviceContext->ClearRenderTargetView(g_pRenderTargetView, clearColor);

            g_Renderer->Render();

            g_Renderer->EndFrame();

            // Present the frame
            g_pSwapChain->Present(1, 0);
        }
    }

    CleanupDeviceD3D();
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return 0;
}