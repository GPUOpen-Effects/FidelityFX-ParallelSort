// sample.cpp
// 
// Copyright(c) 2021 Advanced Micro Devices, Inc.All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include "Sample.h"

#include <shellapi.h>
#include <cassert>

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void Sample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight)
{
    // set some default values
    *pWidth = 1920;
    *pHeight = 1080;
    m_VsyncEnabled = false;
    m_bIsBenchmarking = false;
    m_fontSize = 13.f; // default value overridden by a json file if available
    m_isCpuValidationLayerEnabled = false;
    m_isGpuValidationLayerEnabled = false;
    m_stablePowerState = false;

    // Read globals
    auto process = [&](json jData)
    {
        *pWidth = jData.value("width", *pWidth);
        *pHeight = jData.value("height", *pHeight);
        m_fullscreenMode = jData.value("presentationMode", m_fullscreenMode);
        m_isCpuValidationLayerEnabled = jData.value("CpuValidationLayerEnabled", m_isCpuValidationLayerEnabled);
        m_isGpuValidationLayerEnabled = jData.value("GpuValidationLayerEnabled", m_isGpuValidationLayerEnabled);
        m_VsyncEnabled = jData.value("vsync", m_VsyncEnabled);
        m_bIsBenchmarking = jData.value("benchmark", m_bIsBenchmarking);
        m_stablePowerState = jData.value("stablePowerState", m_stablePowerState);
        m_fontSize = jData.value("fontsize", m_fontSize);
    };

    // Read config file (and override values from commandline if so)
    {
        std::ifstream f("FFXParallelSort.json");
        if (!f)
        {
            MessageBox(nullptr, "Config file not found!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }

        try
        {
            f >> m_jsonConfigFile;
        }
        catch (json::parse_error)
        {
            MessageBox(nullptr, "Error parsing FFXParallelSort.json!\n", "Cauldron Panic!", MB_ICONERROR);
            exit(0);
        }
    }

    json globals = m_jsonConfigFile["globals"];
    process(globals);

    // Process the command line to see if we need to do anything for the sample (i.e. benchmarking, setup certain settings, etc.)
    std::string charString = lpCmdLine;
    if (!charString.compare(""))
        return;     // No parameters

    // Need to first convert the char string to a wide character set 
    std::wstring wideString;
    wideString.assign(charString.begin(), charString.end());

    LPWSTR* ArgList;
    int ArgCount, CurrentArg(0);
    ArgList = CommandLineToArgvW(wideString.c_str(), &ArgCount);
    while (CurrentArg < ArgCount)
    {
        wideString = ArgList[CurrentArg];

        // Enable benchmarking
        if (!wideString.compare(L"-benchmark"))
        {
            m_bIsBenchmarking = true;
            ++CurrentArg;
        }

        // Set num keys to sort
        else if (!wideString.compare(L"-keyset"))
        {
            assert(ArgCount > CurrentArg + 1 && "Incorrect usage of -keyset <0-2>");
            // Get the parameter
            int keySet = std::stoi(ArgList[CurrentArg + 1]);
            assert(keySet >= 0 && keySet < 3 && "Incorrect usage of -keyset <0-2>");
            FFXParallelSort::OverrideKeySet(keySet);
            CurrentArg += 2;
        }

        // Set payload sort
        else if (!wideString.compare(L"-payload"))
        {
            FFXParallelSort::OverridePayload();
            ++CurrentArg;
        }

        else
        {
            assert(false && "Unsupported command line parameter");
            exit(0);
        }
    }
}

//--------------------------------------------------------------------------------------
//
// OnCreate
//
//--------------------------------------------------------------------------------------
void Sample::OnCreate()
{
    // Init the shader compiler
    InitDirectXCompiler();
    CreateShaderCache();

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    m_pRenderer = new Renderer();
    m_pRenderer->OnCreate(&m_device, &m_swapChain, m_fontSize);

    // set benchmarking state if enabled 
    if (m_bIsBenchmarking)
    {
        std::string deviceName;
        std::string driverVersion;
        m_device.GetDeviceInfo(&deviceName, &driverVersion);
        BenchmarkConfig(m_jsonConfigFile["BenchmarkSettings"], -1, nullptr, deviceName, driverVersion);
    }

    // Init GUI (non gfx stuff)
    ImGUI_Init((void*)m_windowHwnd);
    m_UIState.Initialize();

    OnResize();
    OnUpdateDisplay();
}

//--------------------------------------------------------------------------------------
//
// OnDestroy
//
//--------------------------------------------------------------------------------------
void Sample::OnDestroy()
{
    ImGUI_Shutdown();

    m_device.GPUFlush();

    m_pRenderer->OnDestroyWindowSizeDependentResources();
    m_pRenderer->OnDestroy();

    delete m_pRenderer;

    //shut down the shader compiler 
    DestroyShaderCache(&m_device);
}

//--------------------------------------------------------------------------------------
//
// OnEvent, win32 sends us events and we forward them to ImGUI
//
//--------------------------------------------------------------------------------------
static void ToggleBool(bool& b) { b = !b; }
bool Sample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    // handle function keys (F1, F2...) here, rest of the input is handled
    // by imGUI later in HandleInput() function
    const WPARAM& KeyPressed = msg.wParam;
    switch (msg.message)
    {
    case WM_KEYUP:
    case WM_SYSKEYUP:
        /* WINDOW TOGGLES */
        if (KeyPressed == VK_F1) m_UIState.bShowControlsWindow ^= 1;
        if (KeyPressed == VK_F2) m_UIState.bShowProfilerWindow ^= 1;
        break;
    }

    return true;
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void Sample::OnResize()
{
    // Destroy resources (if we are not minimized)
    if (m_Width && m_Height && m_pRenderer)
    {
        m_pRenderer->OnDestroyWindowSizeDependentResources();
        m_pRenderer->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
    }
}

//--------------------------------------------------------------------------------------
//
// UpdateDisplay
//
//--------------------------------------------------------------------------------------
void Sample::OnUpdateDisplay()
{
    // Destroy resources (if we are not minimized)
    if (m_pRenderer)
    {
        m_pRenderer->OnUpdateDisplayDependentResources(&m_swapChain);
    }
}

//--------------------------------------------------------------------------------------
//
// OnUpdate
//
//--------------------------------------------------------------------------------------
void Sample::OnUpdate()
{
    ImGuiIO& io = ImGui::GetIO();

    //If the mouse was not used by the GUI then it's for the camera
    if (io.WantCaptureMouse)
    {
        io.MouseDelta.x = 0;
        io.MouseDelta.y = 0;
        io.MouseWheel = 0;
    }

    // Keyboard & Mouse
    HandleInput(io);

    // Increase time
    m_time += (float)m_deltaTime / 1000.0f; // time in seconds
}

void Sample::HandleInput(const ImGuiIO& io)
{
    auto fnIsKeyTriggered = [&io](char key) { return io.KeysDown[key] && io.KeysDownDuration[key] == 0.0f; };

    // Handle Keyboard/Mouse input here
}

//--------------------------------------------------------------------------------------
//
// OnRender, updates the state from the UI, animates, transforms and renders the scene
//
//--------------------------------------------------------------------------------------
void Sample::OnRender()
{
    // Do any start of frame necessities
    BeginFrame();

    ImGUI_UpdateIO();
    ImGui::NewFrame();

    if (m_bIsBenchmarking)
    {
        // Benchmarking takes control of the time, and exits the app when the animation is done
        std::vector<TimeStamp> timeStamps = m_pRenderer->GetTimingValues();
        std::string Filename;
        m_time = BenchmarkLoop(timeStamps, nullptr, Filename);
    }
    else
    {
        // Build the UI. Note that the rendering of the UI happens later.
        BuildUI();
        OnUpdate();
    }

    // Do Render frame using AFR
    m_pRenderer->OnRender(&m_UIState, &m_swapChain, m_time, m_bIsBenchmarking);

    // Framework will handle Present and some other end of frame logic
    EndFrame();
}


//--------------------------------------------------------------------------------------
//
// WinMain
//
//--------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow)
{
    LPCSTR Name = "FidelityFX Parallel Sort VK v1.1";

    // create new DX sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new Sample(Name));
}
