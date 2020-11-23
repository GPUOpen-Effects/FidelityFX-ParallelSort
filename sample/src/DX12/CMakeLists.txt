project (${PROJECT_NAME})

include(${CMAKE_CURRENT_SOURCE_DIR}/../../common.cmake)

add_compile_options(/MP)
add_compile_definitions(FFX_CPP)

set(sources
    sample.cpp
    sample.h
    stdafx.cpp
    stdafx.h
	samplerenderer.cpp
	samplerenderer.h
	ParallelSort.cpp
	ParallelSort.h)

set(shader_sources
	${CMAKE_CURRENT_SOURCE_DIR}/shaders/ParallelSortCS.hlsl
	${CMAKE_CURRENT_SOURCE_DIR}/shaders/ParallelSortVerify.hlsl)

set(fidelityfx_sources
	${CMAKE_CURRENT_SOURCE_DIR}/../../../FFX-ParallelSort/FFX_ParallelSort.h)

set(common_sources
	${CMAKE_CURRENT_SOURCE_DIR}/../Common/FFXParallelSort.json
	${CMAKE_CURRENT_SOURCE_DIR}/../Common/Validate4K.png
	${CMAKE_CURRENT_SOURCE_DIR}/../Common/Validate2K.png
	${CMAKE_CURRENT_SOURCE_DIR}/../Common/Validate1080p.png)

copyCommand("${shader_sources}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX)
copyCommand("${fidelityfx_sources}" ${CMAKE_HOME_DIRECTORY}/bin/ShaderLibDX/FFX-ParallelSort)
copyCommand("${common_sources}" ${CMAKE_HOME_DIRECTORY}/bin)

source_group("Common" FILES ${common_sources})
source_group("Shaders" FILES ${shader_sources})
source_group("FidelityFX" FILES ${fidelityfx_sources})
source_group("Sources" FILES ${sources})

# prevent VS from processing/compiling these files
set_source_files_properties(${Shaders_src} PROPERTIES VS_TOOL_OVERRIDE "Text")

add_executable(${PROJECT_NAME} WIN32 ${common_sources} ${shader_sources} ${sources} ${fidelityfx_sources})
target_link_libraries(${PROJECT_NAME} LINK_PUBLIC Cauldron_DX12 ImGUI amd_ags DXC d3dcompiler D3D12 DXGI)
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_HOME_DIRECTORY}/bin")

addManifest(${PROJECT_NAME})
