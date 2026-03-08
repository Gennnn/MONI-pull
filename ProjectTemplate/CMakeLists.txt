cmake_minimum_required(VERSION 3.16)

set(PROJECT_NAME PPV)
project(${PROJECT_NAME})
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()
# Great way to reduce compile times for large files like Gateware.h 
# https://edgarluque.com/blog/cmake-precompiled-headers/
set(PRE_COMPILED
	./Source/precompiled.h 
)

# Create list of source files.
# CMake generally discourages the use of GLOB. (it can make doing more complex operations fail)
# For students the conveinece of not editing the CMake constantly is probably more important.  
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS
	./Source/*.h
	./Source/*.hpp
	./Source/*.cpp
	./Source/*.inl
	./Shaders/*.hlsl
	./gateware-main/Gateware.h
	./entt-3.13.1/single_include/entt/entt.hpp
	./defaults.ini
	./inifile-cpp-master/include/inicpp.h
	./TinyGLTF/*.h
	./TinyGLTF/*.hpp
	./TrueType/*.h
)

set(SOLOUD_STATIC ON CACHE BOOL "" FORCE)
set(SOLOUD_DYNAMIC OFF CACHE BOOL "" FORCE)
set(SOLOUD_C_API OFF CACHE BOOL "" FORCE)
set(SOLOUD_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(SOLOUD_GENERATE_GLUE OFF CACHE BOOL "" FORCE)
set(SOLOUD_BUILD_FILTERS ON CACHE BOOL "" FORCE)

set(SOLOUD_BACKEND_NULL OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_SDL2 OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_ALSA OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_COREAUDIO OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_OPENSLES OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_XAUDIO2 OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_WINMM OFF CACHE BOOL "" FORCE)
set(SOLOUD_BACKEND_WASAPI OFF CACHE BOOL "" FORCE)

if (WIN32)
	set(SOLOUD_BACKEND_XAUDIO2 ON CACHE BOOL "" FORCE)
	set(SOLOUD_BACKEND_WASAPI ON CACHE BOOL "" FORCE)
	set(SOLOUD_BACKEND_WINMM ON CACHE BOOL "" FORCE)
endif()
if(APPLE)
	set(SOLOUD_BACKEND_COREAUDIO ON CACHE BOOL "" FORCE)
endif()
if (UNIX AND NOT APPLE)
	set(SOLOUD_BACKEND_ALSA ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(SoLoud/contrib EXCLUDE_FROM_ALL)

# force unicode in some libraries on win32
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

if (WIN32)
	# by default CMake selects "ALL_BUILD" as the startup project 
	set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
	add_executable (${PROJECT_NAME} ${SOURCE_FILES})

	if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
      $<$<CONFIG:Debug>:/Ob1>
      $<$<CONFIG:Debug>:/RTC->
      $<$<CONFIG:Debug>:/Oy->
    )
	endif()

	if (MSVC)
    set(SHADERC_REL "$ENV{VULKAN_SDK}/Lib/shaderc_combined.lib")
    set(SHADERC_DBG "$ENV{VULKAN_SDK}/Lib/shaderc_combinedd.lib") # if it exists

    if (EXISTS "${SHADERC_DBG}")
      target_link_libraries(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:${SHADERC_DBG}>
        $<$<NOT:$<CONFIG:Debug>>:${SHADERC_REL}>
      )
    else()
      # Vulkan SDK doesn't ship a debug shaderc lib -> you can't link it into a true Debug build
      target_link_libraries(${PROJECT_NAME} PRIVATE ${SHADERC_REL})
    endif()
	endif()

	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SoLoud/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE soloud)
	source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})
	target_include_directories(${PROJECT_NAME} PUBLIC $ENV{VULKAN_SDK}/Include/)
	target_link_directories(${PROJECT_NAME} PUBLIC $ENV{VULKAN_SDK}/Lib/)
	#target_compile_options(${PROJECT_NAME} PRIVATE "/MD")
	# shaderc_combined.lib in Vulkan requires this for debug & release (runtime shader compiling)
endif(WIN32)

if(UNIX AND NOT APPLE)
	# libshaderc_combined.a is required for runtime shader compiling
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -lX11 -lshaderc_combined")
    find_package(X11)
	find_package(Vulkan REQUIRED)
    link_libraries(${X11_LIBRARIES})
    include_directories(${X11_INCLUDE_DIR})
    include_directories(${Vulkan_INCLUDE_DIR}) 
	#link_directories(${Vulkan_LIBRARY}) this is currently not working
	link_libraries(${Vulkan_LIBRARIES})
	# the path is (properly)hardcoded because "${Vulkan_LIBRARY}" currently does not 
	# return a proper path on MacOS (it has the .dynlib appended)
    link_libraries(/usr/lib/x86_64-linux-gnu/libshaderc_combined.a)
    add_executable (${PROJECT_NAME} ${SOURCE_FILES})
	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SoLoud/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE soloud)
    source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCE_FILES})
endif(UNIX AND NOT APPLE)

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fmodules -fcxx-modules")
	set(Architecture ${CMAKE_OSX_ARCHITECTURES})
	find_package(Vulkan REQUIRED)
	include_directories(${Vulkan_INCLUDE_DIR}) 
	#link_directories(${Vulkan_LIBRARY}) this is currently not working
	link_libraries(${Vulkan_LIBRARIES})
	# libshaderc_combined.a is required for runtime shader compiling
	# the path is (properly)hardcoded because "${Vulkan_LIBRARY}" currently does not 
	# return a proper path on MacOS (it has the .dynlib appended)
	link_libraries(/usr/local/lib/libshaderc_combined.a)
	add_executable (${PROJECT_NAME} source/main.mm)
	target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/SoLoud/include)
	target_link_libraries(${PROJECT_NAME} PRIVATE soloud)
endif(APPLE)

# EnTT requires c++17 support
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

# adding gateware.h and other librairies as a precompiled headers to reduce compile times
target_precompile_headers(${PROJECT_NAME} PRIVATE ${PRE_COMPILED})

# Disable all shaders from compiling since they may have Vulkan specifics
file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
	./Shaders/*.hlsl
)
set_source_files_properties( ${SHADER_FILES} PROPERTIES
	VS_SHADER_TYPE Vertex # not all are vertex shaders, but that's ok here
    VS_SHADER_MODEL 5.1
    VS_SHADER_ENTRYPOINT main
    VS_TOOL_OVERRIDE "None" # stop VS from compiling, we will do it
)


