# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

function(x3pio_external_example name)
    cmake_parse_arguments(ARG "" "SOURCE" "LIBS" ${ARGN})
    add_executable(example_${name} ${ARG_SOURCE})
    target_link_libraries(example_${name} PRIVATE x3pio ${ARG_LIBS})
    target_compile_features(example_${name} PRIVATE cxx_std_17)
    set_target_properties(example_${name} PROPERTIES CXX_EXTENSIONS OFF)
endfunction()

find_package(OpenCV REQUIRED COMPONENTS core imgproc)
x3pio_external_example(opencv SOURCE opencv.cpp LIBS opencv_core opencv_imgproc)

set(vtk_components CommonCore CommonDataModel FiltersCore FiltersGeneral
    FiltersGeometry InteractionStyle RenderingCore RenderingOpenGL2)
find_package(VTK REQUIRED COMPONENTS ${vtk_components})
set(vtk_modules ${vtk_components})
list(TRANSFORM vtk_modules PREPEND VTK::)
x3pio_external_example(vtk SOURCE vtk.cpp LIBS ${vtk_modules})
vtk_module_autoinit(TARGETS example_vtk MODULES ${vtk_modules})

find_package(Eigen3 3.3 REQUIRED NO_MODULE)
x3pio_external_example(eigen SOURCE eigen.cpp LIBS Eigen3::Eigen)

# PCL reports its libraries, headers and definitions in variables.
find_package(PCL 1.11 REQUIRED COMPONENTS common filters)
x3pio_external_example(pcl SOURCE pcl.cpp LIBS ${PCL_LIBRARIES})
target_include_directories(example_pcl SYSTEM PRIVATE ${PCL_INCLUDE_DIRS})
target_compile_definitions(example_pcl PRIVATE ${PCL_DEFINITIONS})
target_link_directories(example_pcl PRIVATE ${PCL_LIBRARY_DIRS})
