cmake_minimum_required(VERSION 3.20)
project(n1mbus_pre VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# Fetch MinHook
FetchContent_Declare(
  minhook
  GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(minhook)

# Fetch ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        master
)
FetchContent_Populate(imgui)

# Create ImGui target
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)

# --- DLL (n1mbus_dll) ---
add_library(n1mbus_dll SHARED
    src_dll/dllmain.cpp
    src_dll/hook.cpp
    src_dll/jni_manager.cpp
    src_dll/gl_ext.cpp
    src_dll/plugin_loader.cpp
)

target_include_directories(n1mbus_dll PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src_dll
    ${CMAKE_CURRENT_SOURCE_DIR}/src_dll/jni
    ${CMAKE_CURRENT_SOURCE_DIR}/src_dll/modules
)
target_link_libraries(n1mbus_dll PRIVATE opengl32 minhook imgui user32 gdi32 urlmon)

# --- Injector (n1mbus_injector) ---
add_executable(n1mbus_injector WIN32
    src_injector/main.cpp
)

if(MSVC)
  set_target_properties(n1mbus_injector PROPERTIES
    LINK_FLAGS "/MANIFEST:EMBED /MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"")
endif()

target_link_libraries(n1mbus_injector PRIVATE user32 kernel32 comdlg32 imgui opengl32 gdi32 dwmapi)

target_include_directories(n1mbus_injector PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src_dll
)

# Copy assets folder to build output
add_custom_command(TARGET n1mbus_dll POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:n1mbus_dll>/assets
  COMMENT "Copying assets to build directory"
)

# --- Java Agent (n1mbus-agent.jar) ---
find_program(JAVA_COMPILER javac)
find_program(JAVA_JAR jar)

if(JAVA_COMPILER AND JAVA_JAR)
  # Download netty if not present
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/libs/netty-all-4.0.23.Final.jar")
    message(STATUS "Downloading netty-all-4.0.23.Final.jar...")
    file(DOWNLOAD
      "https://repo1.maven.org/maven2/io/netty/netty-all/4.0.23.Final/netty-all-4.0.23.Final.jar"
      "${CMAKE_SOURCE_DIR}/libs/netty-all-4.0.23.Final.jar"
      SHOW_PROGRESS)
  endif()

  set(JAVA_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/java_build")
  set(JAVA_JAR_OUT "${CMAKE_CURRENT_BINARY_DIR}/Release/n1mbus-agent.jar")

  add_custom_command(
    OUTPUT "${JAVA_BUILD_DIR}/n1mbus/N1mbusAgent.class"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${JAVA_BUILD_DIR}"
    COMMAND "${JAVA_COMPILER}" --release 8 -encoding UTF-8
      -cp "${CMAKE_SOURCE_DIR}/libs/netty-all-4.0.23.Final.jar"
      -d "${JAVA_BUILD_DIR}"
      "${CMAKE_SOURCE_DIR}/src_java/N1mbusAgent.java"
    DEPENDS "${CMAKE_SOURCE_DIR}/src_java/N1mbusAgent.java"
    COMMENT "Compiling N1mbusAgent.java"
  )

  add_custom_command(
    OUTPUT "${JAVA_JAR_OUT}"
    COMMAND "${JAVA_JAR}" cf "${JAVA_JAR_OUT}"
      -C "${JAVA_BUILD_DIR}" n1mbus/N1mbusAgent.class
    DEPENDS "${JAVA_BUILD_DIR}/n1mbus/N1mbusAgent.class"
    COMMENT "Creating n1mbus-agent.jar"
  )

  add_custom_target(n1mbus_agent ALL
    DEPENDS "${JAVA_JAR_OUT}"
  )
else()
  message(WARNING "javac or jar not found - n1mbus-agent.jar will NOT be built. Run build_java.ps1 manually.")
endif()