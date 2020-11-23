// sample.cpp
// 
// Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "stdafx.h"
#include "Sample.h"

#include <shellapi.h>
#include <cassert>

// Uncomment to generate a pix capture on first frame
//#define GENERATE_PIXCAPTURE

#ifdef GENERATE_PIXCAPTURE
// Uncomment to enable PIX capture of first frame
#include <DXProgrammableCapture.h>
#include <dxgi1_3.h>
#include <wrl.h>

Microsoft::WRL::ComPtr<IDXGraphicsAnalysis> ga;
#endif // GENERATE_PIXCAPTURE

Sample::Sample(LPCSTR name) : FrameworkWindows(name)
{
    m_lastFrameTime = MillisecondsNow();
    m_time = 0;
    m_bPlay = true;
}

//--------------------------------------------------------------------------------------
//
// OnParseCommandLine
//
//--------------------------------------------------------------------------------------
void Sample::OnParseCommandLine(LPSTR lpCmdLine, uint32_t* pWidth, uint32_t* pHeight, bool *pbFullScreen)
{
    // set some default values
    *pWidth = 1920; 
    *pHeight = 1080; 
    *pbFullScreen = false;
    m_state.m_isBenchmarking = false;
    m_isCpuValidationLayerEnabled = false;
    m_isGpuValidationLayerEnabled = false;
    m_stablePowerState = false;

    //read globals
    auto process = [&](json jData)
    {
        *pWidth = jData.value("width", *pWidth);
        *pHeight = jData.value("height", *pHeight);
        *pbFullScreen = jData.value("fullScreen", *pbFullScreen);
        m_isCpuValidationLayerEnabled = jData.value("CpuValidationLayerEnabled", m_isCpuValidationLayerEnabled);
        m_isGpuValidationLayerEnabled = jData.value("GpuValidationLayerEnabled", m_isGpuValidationLayerEnabled);
        m_state.m_isBenchmarking = jData.value("benchmark", m_state.m_isBenchmarking);
        m_stablePowerState = jData.value("stablePowerState", m_stablePowerState);
    };

	// read config file (and override values from commandline if so)
	//
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

    // Need to first convert the char string to a wide character set (Note: Why aren't all strings wide in the framework)?
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
            m_state.m_isBenchmarking = true;
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
void Sample::OnCreate(HWND hWnd)
{
	// Create Device
    m_device.OnCreate("myapp", "myEngine", m_isCpuValidationLayerEnabled, m_isGpuValidationLayerEnabled, hWnd);
    m_device.CreatePipelineCache();

    // set stable power state
    if (m_stablePowerState)
        m_device.GetDevice()->SetStablePowerState(TRUE);

    //init the shader compiler
    InitDirectXCompiler();
    CreateShaderCache();

    // Create Swapchain
    uint32_t dwNumberOfBackBuffers = 2;
    m_swapChain.OnCreate(&m_device, dwNumberOfBackBuffers, hWnd);

    // Create a instance of the renderer and initialize it, we need to do that for each GPU
    m_Node = new SampleRenderer();
    m_Node->OnCreate(&m_device, &m_swapChain);

    // init GUI (non gfx stuff)
    ImGUI_Init((void *)hWnd);

	if (m_state.m_isBenchmarking)
	{
		std::string deviceName;
		std::string driverVersion;
		m_device.GetDeviceInfo(&deviceName, &driverVersion);
		BenchmarkConfig(m_jsonConfigFile["BenchmarkSettings"], -1, nullptr, deviceName, driverVersion);
	}

#ifdef GENERATE_PIXCAPTURE
    // Uncomment to enable PIX capture of first frame
	HRESULT hr = DXGIGetDebugInterface1(0, IID_PPV_ARGS(&ga));
	// hr will be E_NOINTERFACE if not attached for GPU capture
	if (hr == E_NOINTERFACE)
		ga = nullptr;
#endif // GENERATE_PIXCAPTURE
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

    // Fullscreen state should always be false before exiting the app.
    m_swapChain.SetFullScreen(false);

    m_Node->OnDestroyWindowSizeDependentResources();
    m_Node->OnDestroy();

    delete m_Node;

    m_swapChain.OnDestroyWindowSizeDependentResources();
    m_swapChain.OnDestroy();

    //shut down the shader compiler 
    DestroyShaderCache(&m_device);

    m_device.OnDestroy();
}

//--------------------------------------------------------------------------------------
//
// OnEvent, win32 sends us events and we forward them to ImGUI
//
//--------------------------------------------------------------------------------------
bool Sample::OnEvent(MSG msg)
{
    if (ImGUI_WndProcHandler(msg.hwnd, msg.message, msg.wParam, msg.lParam))
        return true;

    return true;
}

//--------------------------------------------------------------------------------------
//
// SetFullScreen
//
//--------------------------------------------------------------------------------------
void Sample::SetFullScreen(bool fullscreen)
{
    m_device.GPUFlush();
    m_swapChain.SetFullScreen(fullscreen);
}

//--------------------------------------------------------------------------------------
//
// OnResize
//
//--------------------------------------------------------------------------------------
void Sample::OnResize(uint32_t width, uint32_t height, DisplayModes displayMode)
{
    if (m_Width != width || m_Height != height)
    {
        // Flush GPU
        m_device.GPUFlush();

        // destroy resources (if were not minimized)
        if (m_Width > 0 && m_Height > 0)
        {
            if (m_Node!=nullptr)
            {
                m_Node->OnDestroyWindowSizeDependentResources();
            }
            m_swapChain.OnDestroyWindowSizeDependentResources();
        }

        m_Width = width;
        m_Height = height;

        // if resizing but not minimizing the recreate it with the new size
        if (m_Width > 0 && m_Height > 0)
        {
            m_swapChain.OnCreateWindowSizeDependentResources(m_Width, m_Height, false, displayMode);
            if (m_Node != nullptr)
            {
                m_Node->OnCreateWindowSizeDependentResources(&m_swapChain, m_Width, m_Height);
            }
        }
    }
}

//--------------------------------------------------------------------------------------
//
// BuildUI, all UI code should be here
//
//--------------------------------------------------------------------------------------
void Sample::BuildUI()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameBorderSize = 1.0f;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 700), ImGuiCond_FirstUseEver);

    bool opened = true;
    ImGui::Begin("Stats", &opened);

    if (ImGui::CollapsingHeader("Info", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Resolution       : %ix%i", m_Width, m_Height);
    }

    // Render UI for Radix Sort
    this->m_Node->RenderParallelSortUI();

    if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen))
    {
        std::vector<TimeStamp> timeStamps = m_Node->GetTimingValues();
        if (timeStamps.size() > 0)
        {
            for (uint32_t i = 0; i < timeStamps.size(); i++)
            {
                ImGui::Text("%-22s: %7.1f", timeStamps[i].m_label.c_str(), timeStamps[i].m_microseconds);
            }

            //scrolling data and average computing
            static float values[128];
            values[127] = timeStamps.back().m_microseconds;
            for (uint32_t i = 0; i < 128 - 1; i++) { values[i] = values[i + 1]; }
            ImGui::PlotLines("", values, 128, 0, "GPU frame time (us)", 0.0f, 30000.0f, ImVec2(0, 80));
        }
    }

    ImGui::End();

    // Process I/O
    ImGuiIO& io = ImGui::GetIO();
}

//--------------------------------------------------------------------------------------
//
// OnRender, updates the state from the UI, animates, transforms and renders the scene
//
//--------------------------------------------------------------------------------------
void Sample::OnRender()
{
    // Get timings
    //
    double timeNow = MillisecondsNow();
    float deltaTime = (float)(timeNow - m_lastFrameTime);
    m_lastFrameTime = timeNow;

    ImGUI_UpdateIO();
    ImGui::NewFrame();

    if (m_state.m_isBenchmarking)
    {
        // benchmarking takes control of the time, and exits the app when the animation is done
        std::vector<TimeStamp> timeStamps = m_Node->GetTimingValues();
        m_time = BenchmarkLoop(timeStamps, nullptr, &m_state.m_pScreenShotName);
    }
    else
    {
        // Build the UI. Note that the rendering of the UI happens later.
        BuildUI();

        // Set animation time
        //
        if (m_bPlay)
        {
            m_time += (float)deltaTime / 1000.0f;
        }
    }

    // Update time
    m_state.time = m_time;
    
#ifdef GENERATE_PIXCAPTURE
    // Uncomment to enable PIX capture of first frame
    static uint32_t frameID = 0;
    if (!frameID)
    {
        // Use renderdoc or PIX to take a capture of the first frame if enabled/attached
        if (ga)
            ga->BeginCapture();

    }
#endif // GENERATE_PIXCAPTURE

    // Do Render frame using AFR 
    m_Node->OnRender(&m_state, &m_swapChain);
    m_swapChain.Present();

#ifdef GENERATE_PIXCAPTURE
    // Uncomment to enable PIX capture of first frame
	if (!frameID)
	{
        if (ga)
		    ga->EndCapture();

		frameID++;
	}
#endif // GENERATE_PIXCAPTURE
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
    LPCSTR Name = "FidelityFX Parallel Sort v1.0";

    // create new DX sample
    return RunFramework(hInstance, lpCmdLine, nCmdShow, new Sample(Name));
}
