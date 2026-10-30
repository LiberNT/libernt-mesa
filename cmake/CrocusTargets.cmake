# SPDX-License-Identifier: MIT
# Native CMake target graph for the Crocus Gallium driver (Intel Gfx4-8) over
# the LiberNT hdgfx D3DKMT transport.

function(mesa_intel_target_defaults target)
    mesa_target_defaults(${target})
    target_include_directories(${target} PRIVATE
        "${PROJECT_BINARY_DIR}/include"
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_BINARY_DIR}/src"
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}/src/util"
        "${PROJECT_SOURCE_DIR}/src/util"
        "${PROJECT_BINARY_DIR}/src/util/format"
        "${PROJECT_SOURCE_DIR}/src/util/format"
        "${PROJECT_BINARY_DIR}/src/compiler"
        "${PROJECT_SOURCE_DIR}/src/compiler"
        "${PROJECT_BINARY_DIR}/src/compiler/nir"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir"
        "${PROJECT_BINARY_DIR}/src/compiler/spirv"
        "${PROJECT_SOURCE_DIR}/src/compiler/spirv"
        "${PROJECT_BINARY_DIR}/src/intel"
        "${PROJECT_SOURCE_DIR}/src/intel"
        "${PROJECT_BINARY_DIR}/src/intel/dev"
        "${PROJECT_SOURCE_DIR}/src/intel/dev"
        "${PROJECT_BINARY_DIR}/src/intel/genxml"
        "${PROJECT_SOURCE_DIR}/src/intel/genxml"
        "${PROJECT_BINARY_DIR}/src/intel/perf"
        "${PROJECT_SOURCE_DIR}/src/intel/perf"
        "${PROJECT_SOURCE_DIR}/src/intel/common"
        "${PROJECT_SOURCE_DIR}/cmake/intel"
        "${PROJECT_SOURCE_DIR}/cmake/intel/compat")
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C,CXX>:SHELL:-include ${PROJECT_SOURCE_DIR}/cmake/intel/compat/intel_win32_compat.h>")
    target_compile_definitions(${target} PRIVATE HAVE_INTEL_VIRTIO INTEL_USE_ELK)
    target_compile_options(${target} PRIVATE
        "$<$<COMPILE_LANGUAGE:C>:-fvisibility=hidden>"
        "$<$<COMPILE_LANGUAGE:CXX>:-fvisibility=hidden>"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-override-init>"
        "$<$<COMPILE_LANGUAGE:C>:-Wno-initializer-overrides>")
endfunction()

function(mesa_gallium_intel_target_defaults target)
    mesa_intel_target_defaults(${target})
    target_include_directories(${target} PRIVATE
        "${PROJECT_BINARY_DIR}/src/mesa"
        "${PROJECT_SOURCE_DIR}/src/mesa"
        "${PROJECT_BINARY_DIR}/src/gallium/auxiliary"
        "${PROJECT_SOURCE_DIR}/src/gallium/auxiliary"
        "${PROJECT_SOURCE_DIR}/src/gallium/include"
        "${PROJECT_BINARY_DIR}/src/gallium/drivers"
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers"
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus"
        "${PROJECT_SOURCE_DIR}/src/gallium/winsys"
        "${MESA_REACTOS_SOURCE_DIR}/sdk/include/reactos")
endfunction()

add_library(intel_dev STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(intel_dev)
target_sources(intel_dev PRIVATE
    "${PROJECT_BINARY_DIR}/src/intel/dev/intel_wa.c"
    "${PROJECT_SOURCE_DIR}/src/intel/dev/i915/intel_device_info.c"
    "${PROJECT_SOURCE_DIR}/src/intel/dev/xe/intel_device_info.c"
    "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_debug.c"
    "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_device_info.c"
    "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_hwconfig.c"
    "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_kmd.c"
)

set(_mesa_isl_per_hw_ver_sources
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_emit_cpb.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_emit_depth_stencil.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_surface_state.c")
set(_mesa_isl_per_hw_vers)
foreach(_entry 40:gfx4 50: 60:gfx6 70:gfx7 75: 80:gfx8 90:gfx9 110: 120:gfx12 125:gfx12 200:gfx20 300:gfx20 350:gfx20)
    string(REPLACE ":" ";" _entry "${_entry}")
    list(GET _entry 0 _ver)
    list(LENGTH _entry _entry_len)
    add_library(isl_per_hw_ver${_ver} STATIC EXCLUDE_FROM_ALL)
    mesa_intel_target_defaults(isl_per_hw_ver${_ver})
    target_sources(isl_per_hw_ver${_ver} PRIVATE ${_mesa_isl_per_hw_ver_sources})
    if(_entry_len GREATER 1)
        list(GET _entry 1 _gfx)
        if(_gfx)
            target_sources(isl_per_hw_ver${_ver} PRIVATE "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_${_gfx}.c")
        endif()
    endif()
    target_compile_definitions(isl_per_hw_ver${_ver} PRIVATE GFX_VERx10=${_ver})
    list(APPEND _mesa_isl_per_hw_vers isl_per_hw_ver${_ver})
endforeach()

add_library(isl STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(isl)
target_sources(isl PRIVATE
    "${PROJECT_BINARY_DIR}/src/intel/isl/isl_format_layout.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_aux_info.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_drm.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_format.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_storage_image.c"
    "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_tiled_memcpy_normal.c"
)
if(MESA_ARCH MATCHES "^(amd64|i386)$")
    add_library(isl_tiled_memcpy_sse41 STATIC EXCLUDE_FROM_ALL)
    mesa_intel_target_defaults(isl_tiled_memcpy_sse41)
    target_sources(isl_tiled_memcpy_sse41 PRIVATE
        "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_tiled_memcpy_sse41.c")
    target_compile_options(isl_tiled_memcpy_sse41 PRIVATE "$<$<COMPILE_LANGUAGE:C>:-msse4.1>")
    list(APPEND _mesa_isl_per_hw_vers isl_tiled_memcpy_sse41)
endif()
target_link_libraries(isl PUBLIC ${_mesa_isl_per_hw_vers})

add_library(intel_common STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(intel_common)
target_sources(intel_common PRIVATE
    "${PROJECT_SOURCE_DIR}/src/intel/common/i915/intel_engine.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/i915/intel_gem.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/xe/intel_device_query.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/xe/intel_engine.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/xe/intel_gem.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/xe/intel_queue.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_aux_map.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_bind_timeline.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_common.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_compute_slm.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_debug_identifier.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_engine.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_gem.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_l3_config.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_urb_config.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_sample_positions.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_uuid.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_measure.c"
    "${PROJECT_SOURCE_DIR}/src/intel/common/intel_pagefault.c"
)

add_library(intel_compiler_elk STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(intel_compiler_elk)
target_sources(intel_compiler_elk PRIVATE
    "${PROJECT_BINARY_DIR}/src/intel/compiler/elk/elk_nir_trig_workarounds.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_cfg.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_clip_line.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_clip_point.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_clip_tri.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_clip_unfilled.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_clip_util.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_compile_clip.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_compile_ff_gs.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_compile_sf.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_compiler.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_dead_control_flow.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_debug_recompile.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_disasm.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_disasm_info.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_eu.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_eu_compact.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_eu_emit.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_eu_util.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_eu_validate.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_bank_conflicts.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_cmod_propagation.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_combine_constants.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_copy_propagation.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_cse.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_dead_code_eliminate.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_generator.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_live_variables.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_lower_pack.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_lower_regioning.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_nir.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_reg_allocate.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_register_coalesce.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_saturate_propagation.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_sel_peephole.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_thread_payload.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_validate.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_fs_visitor.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_interpolation_map.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_ir_performance.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_lower_logical_sends.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_analyze_boolean_resolves.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_analyze_ubo_ranges.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_attribute_workarounds.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_lower_alpha_to_coverage.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_lower_cs_intrinsics.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_lower_storage_image.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_options.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_packed_float.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_predicated_break.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_reg_type.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_schedule_instructions.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_shader.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_simd_selection.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_cmod_propagation.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_copy_propagation.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_cse.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_dead_code_eliminate.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_generator.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_gs_visitor.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_live_variables.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_nir.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_gs_nir.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_reg_allocate.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_surface_builder.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_tcs.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_tes.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_visitor.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vec4_vs_visitor.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_vue_map.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_gfx6_gs_visitor.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_blockify_uniform_loads.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_clamp_image_1d_2d_array_sizes.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_clamp_per_vertex_loads.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_lower_non_uniform_barycentric_at_sample.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_lower_non_uniform_resource_intel.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_lower_printf.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_lower_shading_rate_output.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_lower_sparse.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_opt_peephole_ffma.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_opt_peephole_imul32x16.c"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/intel_nir_tcs_workarounds.c"
)
target_include_directories(intel_compiler_elk PRIVATE
    "${PROJECT_SOURCE_DIR}/src/intel/compiler"
    "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk")
target_compile_options(intel_compiler_elk PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-Werror=vla>")

add_library(blorp_elk STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(blorp_elk)
target_sources(blorp_elk PRIVATE
    "${PROJECT_SOURCE_DIR}/cmake/intel/blorp_shaders.cpp"
    "${PROJECT_SOURCE_DIR}/src/intel/blorp/blorp.c"
    "${PROJECT_SOURCE_DIR}/src/intel/blorp/blorp_blit.c"
    "${PROJECT_SOURCE_DIR}/src/intel/blorp/blorp_clear.c"
    "${PROJECT_SOURCE_DIR}/src/intel/blorp/blorp_elk.c"
)
target_include_directories(blorp_elk PRIVATE "${PROJECT_SOURCE_DIR}/src/intel/blorp")

add_library(intel_decoder_elk STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(intel_decoder_elk)
target_sources(intel_decoder_elk PRIVATE
    "${PROJECT_SOURCE_DIR}/cmake/intel/compat/intel_decoder_win32.c")

add_library(intel_perf STATIC EXCLUDE_FROM_ALL)
mesa_intel_target_defaults(intel_perf)
target_sources(intel_perf PRIVATE
    "${PROJECT_BINARY_DIR}/src/intel/perf/intel_perf_metrics.c"
    "${PROJECT_SOURCE_DIR}/src/intel/perf/i915/intel_perf.c"
    "${PROJECT_SOURCE_DIR}/src/intel/perf/xe/intel_perf.c"
    "${PROJECT_SOURCE_DIR}/src/intel/perf/intel_perf_common.c"
    "${PROJECT_SOURCE_DIR}/src/intel/perf/intel_perf.c"
    "${PROJECT_SOURCE_DIR}/src/intel/perf/intel_perf_query.c"
    "${PROJECT_SOURCE_DIR}/src/intel/perf/intel_perf_mdapi.c"
)

set(_mesa_crocus_per_hw_vers)
foreach(_ver 40 45 50 60 70 75 80)
    add_library(crocus_per_hw_ver${_ver} STATIC EXCLUDE_FROM_ALL)
    mesa_gallium_intel_target_defaults(crocus_per_hw_ver${_ver})
    target_sources(crocus_per_hw_ver${_ver} PRIVATE
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_blorp.c"
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_query.c"
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_state.c"
        "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_blt.c"
    )
    target_compile_definitions(crocus_per_hw_ver${_ver} PRIVATE GFX_VERx10=${_ver})
    list(APPEND _mesa_crocus_per_hw_vers crocus_per_hw_ver${_ver})
endforeach()

add_library(crocus STATIC EXCLUDE_FROM_ALL)
mesa_gallium_intel_target_defaults(crocus)
target_sources(crocus PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_batch.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_blit.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_bufmgr.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_clear.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_context.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_draw.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_fence.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_fine_fence.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_formats.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_monitor.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_perf.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_performance_query.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_pipe_control.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_program.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_program_cache.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_resolve.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_resource.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_screen.c"
    "${PROJECT_SOURCE_DIR}/src/gallium/drivers/crocus/crocus_disk_cache.c"
)

add_library(crocuswinsys STATIC EXCLUDE_FROM_ALL)
mesa_gallium_intel_target_defaults(crocuswinsys)
target_sources(crocuswinsys PRIVATE
    "${PROJECT_SOURCE_DIR}/src/gallium/winsys/crocus/d3dkmt/crocus_d3dkmt_winsys.c")

target_compile_definitions(mesa_gallium PRIVATE GALLIUM_CROCUS)
target_link_libraries(mesa_gallium PRIVATE
    crocuswinsys
    crocus
    ${_mesa_crocus_per_hw_vers}
    blorp_elk
    intel_compiler_elk
    intel_decoder_elk
    intel_perf
    intel_common
    isl
    ${_mesa_isl_per_hw_vers}
    intel_dev)
