cmake_minimum_required(VERSION 3.20)
project(RecoveryTotal VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific settings
if(WIN32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/gui.cpp
    src/scanner.cpp
    src/recovery.cpp
    src/file_signatures.cpp
    src/ntfs_parser.cpp
    src/utils.cpp
    src/usb_diagnostic.cpp
)

set(HEADERS
    src/gui.h
    src/scanner.h
    src/recovery.h
    src/file_signatures.h
    src/ntfs_parser.h
    src/utils.h
    src/usb_diagnostic.h
)

# Resource file for app icon and manifest
set(RESOURCES
    resources/app.rc
)

# Executable
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

# Link Windows libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    comctl32      # Common controls (ListView, TreeView, ProgressBar)
    comdlg32      # Common dialogs (folder browser)
    shell32       # Shell functions
    ole32         # COM
    gdi32         # GDI drawing
    user32        # Window management
    kernel32      # Kernel functions
    advapi32      # Security / privilege APIs
    shlwapi       # Shell lightweight utility
    uxtheme       # Visual styles
    setupapi      # Device setup/enumeration (USB diagnostics)
)

# Enable visual styles manifest
target_compile_definitions(${PROJECT_NAME} PRIVATE
    _WIN32_WINNT=0x0A00  # Windows 10+
    WINVER=0x0A00
)
