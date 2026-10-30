# SPDX-License-Identifier: MIT
# Native CMake generator rules for the Intel libraries used by Crocus.
set(_mesa_intel_genxml_dir "${PROJECT_SOURCE_DIR}/src/intel/genxml")
set(_mesa_intel_genxml_out "${PROJECT_BINARY_DIR}/src/intel/genxml")
set(MESA_INTEL_GENXML_FILES gen40 gen45 gen50 gen60 gen70 gen75 gen80 gen90 gen110 gen120 gen125 xe2 xe3 xe3p)
set(MESA_INTEL_GENXML_RT_FILES gen125_rt xe2_rt xe3_rt xe3p_rt)
set(_mesa_intel_genxml_inputs)
foreach(_xml IN LISTS MESA_INTEL_GENXML_FILES MESA_INTEL_GENXML_RT_FILES)
    list(APPEND _mesa_intel_genxml_inputs "${_mesa_intel_genxml_dir}/${_xml}.xml")
endforeach()
set(_mesa_intel_genxml_inputs_main)
foreach(_xml IN LISTS MESA_INTEL_GENXML_FILES)
    list(APPEND _mesa_intel_genxml_inputs_main "${_mesa_intel_genxml_dir}/${_xml}.xml")
endforeach()
set(_mesa_intel_pack_deps
    "${_mesa_intel_genxml_dir}/gen_pack_header.py"
    "${_mesa_intel_genxml_dir}/intel_genxml.py"
    "${_mesa_intel_genxml_dir}/util.py"
    "${Python3_EXECUTABLE}")

mesa_generate(
    OUTPUT "${_mesa_intel_genxml_out}/genX_xml.h"
    CAPTURE "${_mesa_intel_genxml_out}/genX_xml.h"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/src/util/gen_zipped_xml_file.py"
        ${_mesa_intel_genxml_inputs_main}
    DEPENDS "${PROJECT_SOURCE_DIR}/src/util/gen_zipped_xml_file.py" ${_mesa_intel_genxml_inputs_main}
        "${Python3_EXECUTABLE}")

set(_mesa_intel_bits_symbols
    COMPUTE_WALKER GPGPU_WALKER MEDIA_CURBE_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_STATE_FLUSH
    MEDIA_VFE_STATE MI_ARB_CHECK 3DMESH_3D 3DPRIMITIVE 3DPRIMITIVE_EXTENDED 3DSTATE_CLEAR_PARAMS
    3DSTATE_CLIP 3DSTATE_CLIP_MESH 3DSTATE_CONSTANT_ALL 3DSTATE_CONSTANT_VS 3DSTATE_DS 3DSTATE_GS
    3DSTATE_HS 3DSTATE_INDEX_BUFFER 3DSTATE_MESH_CONTROL 3DSTATE_MESH_DISTRIB 3DSTATE_MESH_SHADER
    3DSTATE_MESH_SHADER_DATA 3DSTATE_PRIMITIVE_REPLICATION 3DSTATE_PS 3DSTATE_PS_BLEND 3DSTATE_PS_EXTRA
    3DSTATE_PUSH_CONSTANT_ALLOC_VS 3DSTATE_RASTER 3DSTATE_SBE 3DSTATE_SBE_MESH 3DSTATE_SBE_SWIZ
    3DSTATE_SF 3DSTATE_STREAMOUT 3DSTATE_TASK_CONTROL 3DSTATE_TASK_REDISTRIB 3DSTATE_TASK_SHADER
    3DSTATE_TASK_SHADER_DATA 3DSTATE_TE 3DSTATE_URB_ALLOC_MESH 3DSTATE_URB_ALLOC_TASK
    3DSTATE_URB_ALLOC_VS 3DSTATE_URB_VS 3DSTATE_VF_INSTANCING 3DSTATE_VF_SGVS 3DSTATE_VF_SGVS_2
    3DSTATE_VF_TOPOLOGY 3DSTATE_VFG 3DSTATE_WM 3DSTATE_VS PIPE_CONTROL RESOURCE_BARRIER
    "3DSTATE_CPSIZE_CONTROL_BUFFER::Surface Base Address"
    "3DSTATE_CPSIZE_CONTROL_BUFFER::Surface Pitch"
    "3DSTATE_DEPTH_BUFFER::Surface Base Address"
    "3DSTATE_DEPTH_BUFFER::Surface Pitch"
    "3DSTATE_HIER_DEPTH_BUFFER::Surface Base Address"
    "3DSTATE_HIER_DEPTH_BUFFER::Surface Pitch"
    "3DSTATE_STENCIL_BUFFER::Surface Base Address"
    "3DSTATE_STENCIL_BUFFER::Surface Pitch"
    "3DSTATE_SO_BUFFER::Surface Base Address"
    "3DSTATE_SO_BUFFER::Stream Offset"
    "MI_BATCH_BUFFER_START::Batch Buffer Start Address"
    "MI_REPORT_PERF_COUNT::Memory Address"
    "MI_STORE_DATA_IMM::Address"
    "MI_STORE_DATA_IMM::Immediate Data"
    "MI_STORE_REGISTER_MEM::Memory Address"
    "3DSTATE_DEPTH_BUFFER::Surface Base Address"
    "3DSTATE_DEPTH_BUFFER::Surface Pitch"
    "3DSTATE_STENCIL_BUFFER::Surface Base Address"
    "3DSTATE_STENCIL_BUFFER::Surface Pitch"
    "3DSTATE_HIER_DEPTH_BUFFER::Surface Base Address"
    "3DSTATE_HIER_DEPTH_BUFFER::Surface Pitch"
    3DSTATE_DS 3DSTATE_HS 3DSTATE_CLEAR_PARAMS
    "3DSTATE_SO_BUFFER::Surface Base Address"
    "3DSTATE_SO_BUFFER::Stream Offset"
    "3DSTATE_CPSIZE_CONTROL_BUFFER::Surface Base Address"
    "3DSTATE_CPSIZE_CONTROL_BUFFER::Surface Pitch"
    "COMPUTE_WALKER::body"
    "EXECUTE_INDIRECT_DISPATCH::body"
    3DSTATE_CONSTANT_ALL_DATA
    "RENDER_SURFACE_STATE::Surface Base Address"
    "RENDER_SURFACE_STATE::Surface Pitch"
    "RENDER_SURFACE_STATE::Surface QPitch"
    "RENDER_SURFACE_STATE::Auxiliary Surface Base Address"
    "RENDER_SURFACE_STATE::Auxiliary Surface Pitch"
    "RENDER_SURFACE_STATE::Clear Value Address"
    "RENDER_SURFACE_STATE::Red Clear Color"
    "RENDER_SURFACE_STATE::Green Clear Color"
    "RENDER_SURFACE_STATE::Blue Clear Color"
    "RENDER_SURFACE_STATE::Alpha Clear Color"
    "RENDER_SURFACE_STATE::Width"
    "RENDER_SURFACE_STATE::Height"
    "RENDER_SURFACE_STATE::Depth"
    "RENDER_SURFACE_STATE::Surface Format"
    "RENDER_SURFACE_STATE::Surface Type"
    "RENDER_SURFACE_STATE::Render Target View Extent"
    "RENDER_SURFACE_STATE::Tile Mode"
    "RENDER_SURFACE_STATE::Minimum Array Element"
    CLEAR_COLOR
    "VERTEX_BUFFER_STATE::Buffer Starting Address"
    CPS_STATE INTERFACE_DESCRIPTOR_DATA
    "RT_DISPATCH_GLOBALS::Hit Group Table"
    "RT_DISPATCH_GLOBALS::Miss Group Table"
    "RT_DISPATCH_GLOBALS::Callable Group Table"
    "RT_DISPATCH_GLOBALS::Hit Group Stride"
    "RT_DISPATCH_GLOBALS::Miss Group Stride"
    "RT_DISPATCH_GLOBALS::Callable Group Stride"
    "RT_DISPATCH_GLOBALS::Launch Width"
    "RT_DISPATCH_GLOBALS::Launch Height"
    "RT_DISPATCH_GLOBALS::Launch Depth"
    SAMPLER_STATE VERTEX_ELEMENT_STATE)
string(JOIN "," _mesa_intel_bits_symbols ${_mesa_intel_bits_symbols})
mesa_generate(
    OUTPUT "${_mesa_intel_genxml_out}/genX_bits.h"
    COMMAND "${Python3_EXECUTABLE}" "${_mesa_intel_genxml_dir}/gen_bits_header.py"
        ${_mesa_intel_genxml_inputs}
        -o "${_mesa_intel_genxml_out}/genX_bits.h"
        --include-symbols "${_mesa_intel_bits_symbols}"
    DEPENDS "${_mesa_intel_genxml_dir}/gen_bits_header.py" ${_mesa_intel_genxml_inputs} ${_mesa_intel_pack_deps})

foreach(_xml IN LISTS MESA_INTEL_GENXML_FILES MESA_INTEL_GENXML_RT_FILES)
    mesa_generate(
        OUTPUT "${_mesa_intel_genxml_out}/${_xml}_pack.h"
        CAPTURE "${_mesa_intel_genxml_out}/${_xml}_pack.h"
        COMMAND "${Python3_EXECUTABLE}" "${_mesa_intel_genxml_dir}/gen_pack_header.py"
            "${_mesa_intel_genxml_dir}/${_xml}.xml" --engines=render,blitter,compute
        DEPENDS "${_mesa_intel_genxml_dir}/${_xml}.xml" ${_mesa_intel_pack_deps})
endforeach()
foreach(_xml IN LISTS MESA_INTEL_GENXML_FILES)
    mesa_generate(
        OUTPUT "${_mesa_intel_genxml_out}/${_xml}_video_pack.h"
        CAPTURE "${_mesa_intel_genxml_out}/${_xml}_video_pack.h"
        COMMAND "${Python3_EXECUTABLE}" "${_mesa_intel_genxml_dir}/gen_pack_header.py"
            "${_mesa_intel_genxml_dir}/${_xml}.xml" --engines=video
        DEPENDS "${_mesa_intel_genxml_dir}/${_xml}.xml" ${_mesa_intel_pack_deps})
endforeach()

mesa_generate(
    OUTPUT
        "${PROJECT_BINARY_DIR}/src/intel/dev/intel_wa.h"
        "${PROJECT_BINARY_DIR}/src/intel/dev/intel_wa.c"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/src/intel/dev/gen_wa_helpers.py"
        "${PROJECT_SOURCE_DIR}/src/intel/dev/mesa_defs.json"
        "${PROJECT_BINARY_DIR}/src/intel/dev/intel_wa.h"
        "${PROJECT_BINARY_DIR}/src/intel/dev/intel_wa.c"
    DEPENDS "${PROJECT_SOURCE_DIR}/src/intel/dev/gen_wa_helpers.py"
        "${PROJECT_SOURCE_DIR}/src/intel/dev/mesa_defs.json" "${Python3_EXECUTABLE}")
mesa_generate(
    OUTPUT "${PROJECT_BINARY_DIR}/src/intel/dev/intel_device_info_gen.h"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_device_info_gen_h.py"
        "${PROJECT_BINARY_DIR}/src/intel/dev/intel_device_info_gen.h"
    DEPENDS "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_device_info_gen_h.py"
        "${PROJECT_SOURCE_DIR}/src/intel/dev/intel_device_info.py" "${Python3_EXECUTABLE}")
mesa_generate(
    OUTPUT "${PROJECT_BINARY_DIR}/src/intel/isl/isl_format_layout.c"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/src/intel/isl/gen_format_layout.py"
        --csv "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_format_layout.csv"
        --out "${PROJECT_BINARY_DIR}/src/intel/isl/isl_format_layout.c"
    DEPENDS "${PROJECT_SOURCE_DIR}/src/intel/isl/gen_format_layout.py"
        "${PROJECT_SOURCE_DIR}/src/intel/isl/isl_format_layout.csv" "${Python3_EXECUTABLE}")
mesa_generate(
    OUTPUT "${PROJECT_BINARY_DIR}/src/intel/compiler/elk/elk_nir_trig_workarounds.c"
    CAPTURE "${PROJECT_BINARY_DIR}/src/intel/compiler/elk/elk_nir_trig_workarounds.c"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_trig_workarounds.py"
        -p "${PROJECT_SOURCE_DIR}/src/compiler/nir"
    DEPENDS "${PROJECT_SOURCE_DIR}/src/intel/compiler/elk/elk_nir_trig_workarounds.py"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir/nir_algebraic.py"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir/nir_opcodes.py" "${Python3_EXECUTABLE}")

set(_mesa_intel_oa_files
    hsw bdw chv sklgt2 sklgt3 sklgt4 kblgt2 kblgt3 cflgt2 cflgt3 bxt glk icl ehl tglgt1 tglgt2 rkl
    dg1 adl acmgt1 acmgt2 acmgt3 mtlgt2 mtlgt3 arlgt1 arlgt2 lnl bmg ptl)
set(_mesa_intel_oa_inputs)
foreach(_oa IN LISTS _mesa_intel_oa_files)
    list(APPEND _mesa_intel_oa_inputs "${PROJECT_SOURCE_DIR}/src/intel/perf/oa-${_oa}.xml")
endforeach()
mesa_generate(
    OUTPUT
        "${PROJECT_BINARY_DIR}/src/intel/perf/intel_perf_metrics.c"
        "${PROJECT_BINARY_DIR}/src/intel/perf/intel_perf_metrics.h"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/src/intel/perf/gen_perf.py"
        --code "${PROJECT_BINARY_DIR}/src/intel/perf/intel_perf_metrics.c"
        --header "${PROJECT_BINARY_DIR}/src/intel/perf/intel_perf_metrics.h"
        ${_mesa_intel_oa_inputs}
    DEPENDS "${PROJECT_SOURCE_DIR}/src/intel/perf/gen_perf.py" ${_mesa_intel_oa_inputs}
        "${Python3_EXECUTABLE}")
