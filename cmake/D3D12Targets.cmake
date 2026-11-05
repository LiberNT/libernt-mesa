# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Ahmed ARIF <arif.ing@outlook.com>
# Native CMake target graph for the D3D12 Gallium driver in the WGL ICD.

set(_mesa_d3d12 "${PROJECT_SOURCE_DIR}/src/gallium/drivers/d3d12")
set(_mesa_dxil "${PROJECT_SOURCE_DIR}/src/microsoft/compiler")
set(_mesa_d3d12_wgl "${PROJECT_SOURCE_DIR}/src/gallium/winsys/d3d12/wgl")

function(mesa_d3d12_target_defaults target)
    mesa_target_defaults(${target})
    target_include_directories(${target} PRIVATE
        "${_mesa_d3d12}"
        "${_mesa_dxil}"
        "${PROJECT_SOURCE_DIR}/cmake/wrl-compat"
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers"
        "${PROJECT_BINARY_DIR}/src/microsoft/compiler"
        "${MESA_DIRECTX_HEADERS_ROOT}/include"
        "${PROJECT_BINARY_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}/src/util"
        "${PROJECT_SOURCE_DIR}/src/util"
        "${PROJECT_BINARY_DIR}/src/util/format"
        "${PROJECT_SOURCE_DIR}/src/gallium/include"
        "${PROJECT_BINARY_DIR}/src/gallium/auxiliary"
        "${PROJECT_SOURCE_DIR}/src/gallium/auxiliary"
        "${PROJECT_SOURCE_DIR}/src/gallium/frontends/wgl"
        "${PROJECT_SOURCE_DIR}/src/gallium/winsys"
        "${PROJECT_BINARY_DIR}/src/compiler"
        "${PROJECT_SOURCE_DIR}/src/compiler"
        "${PROJECT_BINARY_DIR}/src/compiler/nir"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir")
    target_compile_definitions(${target} PRIVATE
        HAVE_GALLIUM_D3D12_GRAPHICS
        XXH_FORCE_ALIGN_CHECK=0
        XXH_FORCE_MEMORY_ACCESS=0)
endfunction()

add_library(dxil_compiler STATIC EXCLUDE_FROM_ALL)
mesa_d3d12_target_defaults(dxil_compiler)
target_sources(dxil_compiler PRIVATE
    "${_mesa_dxil}/dxil_buffer.c"
    "${_mesa_dxil}/dxil_container.c"
    "${_mesa_dxil}/dxil_dump.c"
    "${_mesa_dxil}/dxil_enums.c"
    "${_mesa_dxil}/dxil_function.c"
    "${_mesa_dxil}/dxil_module.c"
    "${_mesa_dxil}/dxil_nir.c"
    "${_mesa_dxil}/dxil_nir_lower_int_cubemaps.c"
    "${_mesa_dxil}/dxil_nir_lower_int_samplers.c"
    "${_mesa_dxil}/dxil_nir_lower_vs_vertex_conversion.c"
    "${_mesa_dxil}/dxil_signature.c"
    "${_mesa_dxil}/dxil_nir_tess.c"
    "${_mesa_dxil}/nir_to_dxil.c"
    "${_mesa_dxil}/dxil_validator.cpp"
    "${PROJECT_BINARY_DIR}/src/microsoft/compiler/dxil_nir_algebraic.c")

add_library(d3d12 STATIC EXCLUDE_FROM_ALL)
mesa_d3d12_target_defaults(d3d12)
target_sources(d3d12 PRIVATE
    "${_mesa_d3d12}/d3d12_batch.cpp"
    "${_mesa_d3d12}/d3d12_copy.cpp"
    "${_mesa_d3d12}/d3d12_bufmgr.cpp"
    "${_mesa_d3d12}/d3d12_context_common.cpp"
    "${_mesa_d3d12}/d3d12_fence.cpp"
    "${_mesa_d3d12}/d3d12_format.c"
    "${_mesa_d3d12}/d3d12_residency.cpp"
    "${_mesa_d3d12}/d3d12_resource.cpp"
    "${_mesa_d3d12}/d3d12_resource_state.cpp"
    "${_mesa_d3d12}/d3d12_screen.cpp"
    "${_mesa_d3d12}/d3d12_blit.cpp"
    "${_mesa_d3d12}/d3d12_draw.cpp"
    "${_mesa_d3d12}/d3d12_surface.cpp"
    "${_mesa_d3d12}/d3d12_context_graphics.cpp"
    "${_mesa_d3d12}/d3d12_cmd_signature.cpp"
    "${_mesa_d3d12}/d3d12_compiler.cpp"
    "${_mesa_d3d12}/d3d12_compute_transforms.cpp"
    "${_mesa_d3d12}/d3d12_descriptor_pool.cpp"
    "${_mesa_d3d12}/d3d12_nir_passes.c"
    "${_mesa_d3d12}/d3d12_pipeline_state.cpp"
    "${_mesa_d3d12}/d3d12_query.cpp"
    "${_mesa_d3d12}/d3d12_root_signature.cpp"
    "${_mesa_d3d12}/d3d12_gs_variant.cpp"
    "${_mesa_d3d12}/d3d12_lower_image_casts.c"
    "${_mesa_d3d12}/d3d12_lower_point_sprite.c"
    "${_mesa_d3d12}/d3d12_tcs_variant.cpp"
    "${_mesa_d3d12}/d3d12_dxgi_screen.cpp"
    "${_mesa_d3d12}/d3d12_dxcore_screen.cpp"
    "${PROJECT_SOURCE_DIR}/cmake/wrl-compat/d3d12_guids.cpp")

add_library(d3d12winsys STATIC EXCLUDE_FROM_ALL)
mesa_d3d12_target_defaults(d3d12winsys)
target_sources(d3d12winsys PRIVATE
    "${_mesa_d3d12_wgl}/d3d12_wgl_winsys.c"
    "${_mesa_d3d12_wgl}/d3d12_wgl_framebuffer.cpp")

target_include_directories(mesa_gallium PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gallium/winsys")
target_compile_definitions(mesa_gallium PRIVATE GALLIUM_D3D12)
target_link_libraries(mesa_gallium PRIVATE
    d3d12winsys
    d3d12
    dxil_compiler
    "-lversion")
