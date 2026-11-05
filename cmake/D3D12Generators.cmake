# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Ahmed ARIF <arif.ing@outlook.com>
# Generated sources of the DXIL compiler behind the D3D12 Gallium driver.

mesa_generate(
    OUTPUT
        "${PROJECT_BINARY_DIR}/src/microsoft/compiler/dxil_nir_algebraic.c"
    CAPTURE "${PROJECT_BINARY_DIR}/src/microsoft/compiler/dxil_nir_algebraic.c"
    COMMAND
        "${Python3_EXECUTABLE}"
        "${PROJECT_SOURCE_DIR}/src/microsoft/compiler/dxil_nir_algebraic.py"
        "-p"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir/"
    DEPENDS
        "${PROJECT_SOURCE_DIR}/src/microsoft/compiler/dxil_nir_algebraic.py"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir/nir_algebraic.py"
        "${PROJECT_SOURCE_DIR}/src/compiler/nir/nir_opcodes.py"
        "${Python3_EXECUTABLE}"
)
