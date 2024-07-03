#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d9.h>
#include <d3d9types.h>
#include <d3d9caps.h>
#include <stdexcept>

#pragma comment(lib, "d3d9.lib")

#include "..\..\factories\dx9\renderer_dx9.hpp"

using namespace CheatRenderFramework;
RendererPtr g_Renderer;
FontHandle g_FontTahoma;

static LPDIRECT3D9EX g_pD3D = nullptr;
static LPDIRECT3DDEVICE9EX g_pd3dDevice = nullptr;
static bool g_DeviceLost = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS g_d3dpp = {};

void ResetDevice();

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
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
    }
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void InitD3D(HWND hWnd)
{
    if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &g_pD3D)))
    {
        throw std::runtime_error("Direct3DCreate9Ex failed");
    }

    g_d3dpp.Windowed = TRUE;
    g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    if (FAILED(g_pD3D->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                      &g_d3dpp, nullptr, &g_pd3dDevice)))
    {
        throw std::runtime_error("CreateDeviceEx failed");
    }
}

void CleanUp()
{
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }

    if (g_pD3D)
    {
        g_pD3D->Release();
        g_pD3D = nullptr;
    }
}

void MainLoop()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            if (g_DeviceLost)
            {
                HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
                if (hr == D3DERR_DEVICELOST)
                {
                    ::Sleep(10);
                    continue;
                }

                if (hr == D3DERR_DEVICENOTRESET)
                {
                    ResetDevice();
                }

                g_DeviceLost = false;
            }

            if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
            {
                g_d3dpp.BackBufferWidth = g_ResizeWidth;
                g_d3dpp.BackBufferHeight = g_ResizeHeight;
                g_ResizeWidth = g_ResizeHeight = 0;
                ResetDevice();
            }

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 40, 100), 1.0f, 0);

            if (SUCCEEDED(g_pd3dDevice->BeginScene()))
            {
                g_Renderer->BeginFrame();

                g_Renderer->AddRectFilled(Vec2{10.f, 10.f}, Vec2{10.f + 50.f, 10.f + 50.f}, Color(255, 0, 0));
                g_Renderer->AddRect(Vec2{100.f, 10.f}, Vec2{100.f + 50.f, 10.f + 50.f}, Color(0, 0, 0), 2.f);
                g_Renderer->AddCircle(Vec2{250.f, 40.f}, 32.f, Color(0, 255, 0));
                g_Renderer->AddLine(Vec2{300.f, 40.f}, Vec2{450.f, 45.f}, Color(255, 255, 255));

                g_Renderer->AddText(g_FontTahoma, L"This is a normal test text!", Vec2(5.f, 100.f),
                                    Color(255, 255, 255));

                g_Renderer->AddText(g_FontTahoma, L"This is a drop shadow test text!", Vec2(5.f, 120.f),
                                    Color(255, 255, 255), TEXT_FLAG_DROPSHADOW);

                g_Renderer->AddText(g_FontTahoma, L"This is a outline test text!", Vec2(5.f, 140.f),
                                    Color(255, 255, 255), TEXT_FLAG_OUTLINE);

                g_Renderer->AddText(g_FontTahoma, L"This is a {#FF0000FF}color {#66FF0096}tags {#FFFFFFFF}test text!",
                                    Vec2(5.f, 160.f), Color(255, 255, 255), TEXT_FLAG_COLORTAGS);

                g_Renderer->Render();
                g_Renderer->EndFrame();

                g_pd3dDevice->EndScene();
            }

            HRESULT hr = g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
            if (hr == D3DERR_DEVICELOST)
            {
                g_DeviceLost = true;
            }
        }
    }
}

void ResetDevice()
{
    if (g_Renderer)
    {
        g_Renderer->OnLostDevice();
    }

    HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
    if (SUCCEEDED(hr))
    {
        if (g_Renderer)
        {
            g_Renderer->OnResetDevice();
        }
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

    HWND hWnd = CreateWindow("D3D Window", "Direct3D9Ex Window", WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, NULL, NULL,
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

    MainLoop();

    CleanUp();
    UnregisterClass("D3D Window", wc.hInstance);

    return 0;
}