// stdafx.h
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

#pragma once

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <windowsx.h>

// C RunTime Header Files
#include <malloc.h>
#include <map>
#include <mutex>
#include <vector>
#include <fstream>

#include "../../libs/d3d12x/d3dx12.h"

// we are using DirectXMath
#include <DirectXMath.h>
using namespace DirectX;

// TODO: reference additional headers your program requires here
#include "Base/Imgui.h"
#include "Base/ImguiHelper.h"
#include "Base/Fence.h"
#include "Base/Helper.h"
#include "Base/Device.h"
#include "Base/Texture.h"
#include "Base/SwapChain.h"
#include "Base/UploadHeap.h"
#include "Base/UserMarkers.h"
#include "Base/GPUTimestamps.h"
#include "Base/CommandListRing.h"
#include "Base/StaticBufferPool.h"
#include "Base/DynamicBufferRing.h"
#include "Base/ResourceViewHeaps.h"
#include "Base/ShaderCompilerHelper.h"
#include "Base/StaticConstantBufferPool.h"

#include "GLTF/GltfPbrPass.h"
#include "GLTF/GltfBBoxPass.h"
#include "GLTF/GltfDepthPass.h"
#include "GLTF/GltfMotionVectorsPass.h"

#include "Misc/Misc.h"
#include "Misc/Error.h"
#include "Misc/Camera.h"
#include "Misc/FrameworkWindows.h"

#include "PostProc/TAA.h"
#include "PostProc/Bloom.h"
#include "PostProc/BlurPS.h"
#include "PostProc/SkyDome.h"
#include "PostProc/SkyDomeProc.h"
#include "PostProc/PostProcCS.h"
#include "PostProc/ToneMapping.h"
#include "PostProc/ToneMappingCS.h"
#include "PostProc/ColorConversionPS.h"
#include "PostProc/DownSamplePS.h"
#include "PostProc/ShadowResolvePass.h"

#include "ParallelSort.h"

#include "Widgets/wireframe.h"


using namespace CAULDRON_DX12;
