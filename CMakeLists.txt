cmake_minimum_required(VERSION 3.25)
project(SvenCoopSDK LANGUAGES C CXX)

option(SVENCOOP_BUILD_CLIENT "Build the Sven Co-op client with FMOD Ex 4.44.64" ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "SvenCoopSDK requires x86. Use -A Win32 with Visual Studio or the Scout i386 build script.")
endif()
if(NOT (WIN32 AND MSVC AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "X86")
	AND NOT (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86|x86_64|amd64)$"
		AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
	message(FATAL_ERROR "SvenCoopSDK supports Win32 with MSVC and Linux i386 with GCC.")
endif()

set(SVENCOOP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(SVENCOOP_ANGELSCRIPT_DIR "${SVENCOOP_ROOT}/external/angelscript/sdk")
if(NOT EXISTS "${SVENCOOP_ANGELSCRIPT_DIR}/angelscript/include/angelscript.h")
	message(FATAL_ERROR "AngelScript is missing. Run git submodule update --init --recursive.")
endif()
include(svencoop_sources.cmake)

if(SVENCOOP_BUILD_CLIENT)
	set(fmodex_include_dir "${CMAKE_CURRENT_BINARY_DIR}/fmodex-include")
	if(WIN32)
		set(fmodex_api "${SVENCOOP_ROOT}/external/fmodapi44464win/api")
		set(fmodex_link_library "${fmodex_api}/lib/fmodex_vc.lib")
		set(fmodex_runtime "${fmodex_api}/fmodex.dll")
		set(fmodex_runtime_name fmodex.dll)
	else()
		set(fmodex_archive "${SVENCOOP_ROOT}/external/fmodapi44464linux.tar.gz")
		if(NOT EXISTS "${fmodex_archive}")
			message(FATAL_ERROR "FMOD Ex 4.44.64 archive missing: ${fmodex_archive}. See README.md.")
		endif()
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${fmodex_archive}")
		set(fmodex_extract_dir "${CMAKE_CURRENT_BINARY_DIR}/fmodex-sdk")
		file(ARCHIVE_EXTRACT INPUT "${fmodex_archive}" DESTINATION "${fmodex_extract_dir}"
			PATTERNS "fmodapi44464linux/api/inc/*" "fmodapi44464linux/api/lib/libfmodex-4.44.64.so")
		set(fmodex_api "${fmodex_extract_dir}/fmodapi44464linux/api")
		set(fmodex_runtime "${fmodex_api}/lib/libfmodex-4.44.64.so")
		set(fmodex_runtime_name libfmodex.so)
		set(fmodex_link_library "${fmodex_api}/lib/${fmodex_runtime_name}")
	endif()

	foreach(required_file IN ITEMS "${fmodex_api}/inc/fmod.h" "${fmodex_api}/inc/fmod.hpp" "${fmodex_runtime}")
		if(NOT EXISTS "${required_file}")
			message(FATAL_ERROR "FMOD Ex 4.44.64 file missing: ${required_file}. See README.md.")
		endif()
	endforeach()
	file(STRINGS "${fmodex_api}/inc/fmod.h" fmodex_version
		REGEX "^#define[ \t]+FMOD_VERSION[ \t]+0x00044464([ \t]|$)")
	if(NOT fmodex_version)
		message(FATAL_ERROR "Expected FMOD Ex 4.44.64 headers in ${fmodex_api}/inc. See README.md.")
	endif()
	if(NOT WIN32)
		# The upstream ELF has no SONAME. Link using the name installed beside the game.
		configure_file("${fmodex_runtime}" "${fmodex_link_library}" COPYONLY)
	elseif(NOT EXISTS "${fmodex_link_library}")
		message(FATAL_ERROR "FMOD Ex 4.44.64 import library missing: ${fmodex_link_library}. See README.md.")
	endif()
	file(GLOB fmodex_headers "${fmodex_api}/inc/*.h" "${fmodex_api}/inc/*.hpp")
	foreach(header IN LISTS fmodex_headers)
		get_filename_component(header_name "${header}" NAME)
		configure_file("${header}" "${fmodex_include_dir}/fmodex/${header_name}" COPYONLY)
	endforeach()
endif()

function(svencoop_compile_settings target)
	set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
	if(MSVC)
		set_target_properties(${target} PROPERTIES
			MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
			MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase
			INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
		target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS _ITERATOR_DEBUG_LEVEL=0)
		target_compile_options(${target} PRIVATE
			"$<$<COMPILE_LANGUAGE:CXX>:/Zc:threadSafeInit->" "$<$<CONFIG:Release>:/Gy;/Oi>")
	else()
		set_target_properties(${target} PROPERTIES
			C_STANDARD 99 C_STANDARD_REQUIRED ON
			CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
		target_compile_definitions(${target} PRIVATE
			_GLIBCXX_USE_CXX11_ABI=0 POSIX _POSIX linux LINUX _LINUX GNUC HL1
			stricmp=strcasecmp _strnicmp=strncasecmp strnicmp=strncasecmp
			_snprintf=snprintf _vsnprintf=vsnprintf _alloca=alloca
			QUIVER QUAKE2 VALVE_DLL NO_MALLOC_OVERRIDE)
		target_compile_options(${target} PRIVATE
			-march=pentium-m -mfpmath=387 -gdwarf-2 -fno-strict-aliasing
			-fasynchronous-unwind-tables -fexpensive-optimizations -Werror=return-type -w -UNDEBUG
			"$<$<CONFIG:Release>:-g0>" "$<$<CONFIG:Debug>:-g2>"
			"$<$<COMPILE_LANGUAGE:CXX>:-fpermissive;-fexceptions;-fno-gnu-unique>")
	endif()
endfunction()

function(svencoop_install_module target output_name destination)
	set_target_properties(${target} PROPERTIES PREFIX "" OUTPUT_NAME "${output_name}")
	if(MSVC)
		target_link_options(${target} PRIVATE
			/SUBSYSTEM:WINDOWS /DEBUG:FULL "/PDBALTPATH:${output_name}.pdb"
			"$<$<CONFIG:Release>:/OPT:REF;/OPT:ICF>")
		install(FILES "$<TARGET_PDB_FILE:${target}>" DESTINATION "${destination}" OPTIONAL)
	else()
		target_link_options(${target} PRIVATE -static-libgcc -static-libstdc++ LINKER:--no-undefined)
	endif()
	install(TARGETS ${target} RUNTIME DESTINATION "${destination}" LIBRARY DESTINATION "${destination}")
endfunction()

set(server_include_dirs dlls engine common pm_shared game_shared public
	"${SVENCOOP_ANGELSCRIPT_DIR}/angelscript/include")
set(server_definitions AS_PROCESS_METADATA=0 AS_USE_STLNAMES=1 AS_USE_ACCESSORS=0 CLIENT_WEAPONS)
set(server_sources ${SVENCOOP_SERVER_SOURCES} ${SVENCOOP_ANGELSCRIPT_SOURCES})
set(script_sources ${SVENCOOP_SERVER_SCRIPT_RUNTIME_SOURCES})
set_property(SOURCE "${SVENCOOP_ANGELSCRIPT_DIR}/add_on/scriptmath/scriptmath.cpp"
	APPEND PROPERTY COMPILE_DEFINITIONS AS_USE_FLOAT=1)
if(WIN32)
	list(APPEND server_definitions QUIVER VOXEL QUAKE2 VALVE_DLL)
	list(APPEND server_sources dlls/hl.def)
	set_property(SOURCE ${SVENCOOP_MSVC_NO_RTTI_SOURCES} APPEND PROPERTY COMPILE_OPTIONS /GR-)
else()
	list(APPEND server_sources ${SVENCOOP_LINUX_DIRECT_SCRIPT_SOURCES})
	list(REMOVE_ITEM script_sources ${SVENCOOP_LINUX_DIRECT_SCRIPT_SOURCES})
	set_property(SOURCE ${SVENCOOP_ANGELSCRIPT_SOURCES}
		"${SVENCOOP_ANGELSCRIPT_DIR}/add_on/serializer/serializer.cpp"
		APPEND PROPERTY COMPILE_DEFINITIONS AS_NO_EXCEPTIONS)
	set_source_files_properties(external/cryptopp/gcm_simd.cpp PROPERTIES COMPILE_OPTIONS "-mssse3;-mpclmul")
	set_source_files_properties(external/cryptopp/rijndael_simd.cpp PROPERTIES COMPILE_OPTIONS "-msse4.1;-maes")
	set_source_files_properties(external/cryptopp/sha_simd.cpp PROPERTIES COMPILE_OPTIONS "-msse4.2;-msha")
	set_source_files_properties(external/cryptopp/sse_simd.cpp PROPERTIES COMPILE_OPTIONS "-msse2")
endif()

add_library(svencoop_script_runtime STATIC ${script_sources})
set_target_properties(svencoop_script_runtime PROPERTIES OUTPUT_NAME SvenScriptRuntime)
target_include_directories(svencoop_script_runtime PRIVATE ${server_include_dirs})
target_compile_definitions(svencoop_script_runtime PRIVATE ${server_definitions})
svencoop_compile_settings(svencoop_script_runtime)

add_library(svencoopdll SHARED ${server_sources})
target_include_directories(svencoopdll PRIVATE ${server_include_dirs})
target_compile_definitions(svencoopdll PRIVATE ${server_definitions})
target_link_libraries(svencoopdll PRIVATE svencoop_script_runtime)
svencoop_compile_settings(svencoopdll)
svencoop_install_module(svencoopdll server svencoop/dlls)

if(WIN32)
	set(curl_def "${SVENCOOP_ROOT}/external/curl-win32/libcurl_sven.def")
	set(curl_import_lib "${CMAKE_CURRENT_BINARY_DIR}/curl/$<CONFIG>/libcurl_sven.lib")
	add_custom_command(OUTPUT "${curl_import_lib}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/curl/$<CONFIG>"
		COMMAND "${CMAKE_AR}" /NOLOGO /MACHINE:X86 "/DEF:${curl_def}" "/OUT:${curl_import_lib}"
		DEPENDS "${curl_def}"
		VERBATIM)
	add_custom_target(svencoop_curl_import DEPENDS "${curl_import_lib}")
	add_dependencies(svencoopdll svencoop_curl_import)
	target_link_libraries(svencoopdll PRIVATE "${curl_import_lib}" wsock32)
	target_link_options(svencoopdll PRIVATE legacy_stdio_float_rounding.obj)
else()
	find_package(CURL REQUIRED)
	find_package(Threads REQUIRED)
	target_link_libraries(svencoopdll PRIVATE CURL::libcurl m ${CMAKE_DL_LIBS} Threads::Threads)
	set_target_properties(svencoopdll PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

if(SVENCOOP_BUILD_CLIENT)
	add_library(svencoop_cdll SHARED ${SVENCOOP_CLIENT_SOURCES})
	target_include_directories(svencoop_cdll PRIVATE "${fmodex_include_dir}" dlls cl_dll public common pm_shared
		engine utils/vgui/include game_shared external external/discord-rpc/include)
	target_compile_definitions(svencoop_cdll PRIVATE CLIENT_DLL CLIENT_WEAPONS HL_DLL)
	svencoop_compile_settings(svencoop_cdll)
	svencoop_install_module(svencoop_cdll client svencoop/cl_dlls)
	install(FILES "${fmodex_runtime}" DESTINATION svencoop RENAME "${fmodex_runtime_name}")
	if(WIN32)
		target_link_libraries(svencoop_cdll PRIVATE
			"${SVENCOOP_ROOT}/utils/vgui/lib/win32_vc6/vgui.lib"
			"${SVENCOOP_ROOT}/lib/public/SDL2.lib"
			"${SVENCOOP_ROOT}/lib/public/discord-rpc.lib"
			"${fmodex_link_library}" wsock32 opengl32 delayimp)
		target_link_options(svencoop_cdll PRIVATE "/DELAYLOAD:${fmodex_runtime_name}")
	else()
		find_package(OpenGL REQUIRED)
		set(svencoop_linux_lib_dir "${SVENCOOP_ROOT}/linux")
		get_filename_component(fmodex_link_dir "${fmodex_link_library}" DIRECTORY)
		target_compile_definitions(svencoop_cdll PRIVATE DISABLE_VEC_ORIGIN _snwprintf=swprintf)
		target_link_directories(svencoop_cdll PRIVATE "${fmodex_link_dir}" "${svencoop_linux_lib_dir}")
		target_link_libraries(svencoop_cdll PRIVATE
			-l:vgui.so -l:libSDL2-2.0.so.0 -l:libdiscord-rpc.so -l:libfmodex.so
			OpenGL::GL m ${CMAKE_DL_LIBS} Threads::Threads)
		set_target_properties(svencoop_cdll PROPERTIES
			LINK_DEPENDS "${svencoop_linux_lib_dir}/vgui.so;${svencoop_linux_lib_dir}/libSDL2-2.0.so.0;${svencoop_linux_lib_dir}/libdiscord-rpc.so;${fmodex_link_library}"
			BUILD_RPATH "${fmodex_link_dir};${svencoop_linux_lib_dir}"
			INSTALL_RPATH "$ORIGIN;$ORIGIN/..")
	endif()
endif()
