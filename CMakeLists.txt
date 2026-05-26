cmake_minimum_required(VERSION 3.22.1)

# Dölj (dev)-varningar från tredjepartsbibliotek (libsamplerate)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "Suppress dev warnings")
# Tillåt den äldre JUCE-syntaxen för add_custom_command utan att varna
if(POLICY CMP0175)
    cmake_policy(SET CMP0175 OLD)
endif()

# Säg till CMake att exportera en karta över alla filer för VS Code IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(COPY_AFTER_BUILD "" OFF)

project(TICK VERSION 1.0.0)
set(CPACK_PROJECT_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "NSIS")
set(CPACK_PACKAGE_FILE_NAME "TICK-Live-Automation-${CPACK_PACKAGE_VERSION}")
set(CPACK_NSIS_DISPLAY_NAME "TICK LA")
set(CPACK_NSIS_PACKAGE_NAME "TICK LA")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Tick installer")

if(WIN32)
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64\\\\Common Files")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "VST3\\\\TICK Live Automation")
endif()

include(CPack)

if(APPLE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version")
        set(ICON "tick_icon_macOS.png")
        set(HARDENED_RUNTIME $<IF:$<CONFIG:Debug>,NO,YES>)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(APPLE_IOS TRUE)
        set(ICON "tick_icon_iOS.png")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum iOS deployment version")
        set(PLIST_ADDITION
            "
        <plist>
 <dict>
	<key>ABLLinkStartStopSyncSupported</key>
	<true/>
<key>UTExportedTypeDeclarations</key>
	<array>
		<dict>
			<key>UTTypeConformsTo</key>
			<array>
				<string>public.data</string>
			</array>
			<key>UTTypeIdentifier</key>
			<string>com.talaviram.Tick.preset</string>
			<key>UTTypeTagSpecification</key>
			<dict>
				<key>public.filename-extension</key>
				<string>tickpreset</string>
				<key>public.mime-type</key>
				<string>applications/tickpreset</string>
			</dict>
		</dict>
	</array>
</dict>
</plist>"
        )
    endif()

    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)")
    set(CMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM 7P7G2A8U54)
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(ICON "tick_icon.png")
    # UI/Terminal-Fix: Stäng av tjatiga MSVC-varningar (int->float) för en helt klinisk terminal!
    add_compile_options(/wd4244 /wd4312 /wd4189)
endif()

if(UNIX AND NOT APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(IS_MOBILE YES)
endif()

if(IS_MOBILE)
    set(ICON "tick_icon_iOS.png")
endif()

set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON CACHE BOOL INTERNAL)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

add_subdirectory(${CMAKE_SOURCE_DIR}/libsamplerate)
add_subdirectory(${CMAKE_SOURCE_DIR}/JUCE)

# FIX: Tillåt äldre CMake-skript i tredjepartsbibliotek (Ableton Link)
# Detta förhindrar att VS Codes inbyggda CMake Tools kraschar vid auto-konfigurering.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
# Tysta även varningar om föråldrad CMake-kod (Deprecation Warnings) i Abletons skript
set(CMAKE_WARN_DEPRECATED OFF)

# Ladda ner och länka den officiella plattformsoberoende Ableton Link C++-motorn
include(FetchContent)
FetchContent_Declare(
    ableton_link
    GIT_REPOSITORY https://github.com/Ableton/link.git
    # CI/CD SÄKERHET: Lås till en specifik, stabil release! Annars kraschar ditt projekt 
    # slumpmässigt den dagen Ableton gör en "breaking change" på sin master-branch!
    GIT_TAG        Link-3.0.6
)

# Stäng av Abletons egna tester och exempelprogram för att spara tid och undvika onödiga fel
set(LINK_BUILD_QT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LINK_BUILD_AUDIO_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LINK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# FIX: Lura Ableton Link att ASIO är "ON" för att tysta deras interna WASAPI-varning i VS Code. 
# Eftersom vi har stängt av byggandet av exempelprogrammen ovan, kommer koden ändå aldrig att kompilera!
set(LINK_BUILD_ASIO ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ableton_link)

# CI/CD & Bygg-optimering: Ableton Link bygger automatiskt massa exempelprogram (LinkHut) 
# och tester i bakgrunden. Dessa innehåller föråldrad C++ kod som kraschar moderna 
# kompilatorer (C++20). Vi plockar bort dem från bygget helt och hållet!
foreach(_link_target LinkHut LinkHutSilent LinkDiscoveryTest LinkCoreTest link_hut abl_link)
    if(TARGET ${_link_target})
        set_target_properties(${_link_target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endforeach()

set(PLUGIN_FORMATS_TO_BUILD VST3)

if(APPLE)
    list(APPEND PLUGIN_FORMATS_TO_BUILD AU AUv3)
endif()

juce_add_plugin(TICK
    ICON_BIG "Media/Logo/${ICON}"
    BUNDLE_ID com.talaviram.Tick
    COMPANY_NAME "Tal Aviram"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD ${COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Tala
    PLUGIN_CODE Tick
    FORMATS ${PLUGIN_FORMATS_TO_BUILD}
    PRODUCT_NAME "TICK"
    DOCUMENT_EXTENSIONS tickpreset
    NEEDS_WEB_BROWSER FALSE
    HARDENED_RUNTIME_ENABLED ${HARDENED_RUNTIME}
    AAX_IDENTIFIER com.talaviram.Tick
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    AU_EXPORT_PREFIX TickAU
    AAX_CATEGORY AAX_ePlugInCategory_SWGenerators
    VST2_CATEGORY kPlugCategGenerator
    VST3_CATEGORIES Generator

    # iOS
    APP_GROUPS_ENABLED TRUE
    APP_GROUP_IDS group.talaviram.TICKPresets
    BACKGROUND_AUDIO_ENABLED TRUE
    BLUETOOTH_PERMISSION_ENABLED TRUE
    BACKGROUND_BLE_ENABLED TRUE
    DOCUMENT_BROWSER_ENABLED TRUE
    FILE_SHARING_ENABLED TRUE
    ICLOUD_PERMISSIONS_ENABLED TRUE
    NETWORK_MULTICAST_ENABLED TRUE
    REQUIRES_FULL_SCREEN FALSE
    IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
    PLIST_TO_MERGE ${PLIST_ADDITION}
)

source_group("JUX" REGULAR_EXPRESSION "JUX/")
target_sources(TICK
    PRIVATE
    JUX/components/ListBox.h
    JUX/components/ListBox.cpp
    JUX/components/ListBoxMenu.h
    JUX/components/ListBoxMenu.cpp
)

source_group("Source" REGULAR_EXPRESSION "Source/")
source_group("Source/Components" REGULAR_EXPRESSION "Source/components")
source_group("Source/Model" REGULAR_EXPRESSION "Source/model")
source_group("Source/Utils" REGULAR_EXPRESSION "Source/utils")
target_sources(TICK
    PRIVATE
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/components/AboutView.h
    Source/components/DialogComponent.h
    Source/components/DialogComponent.cpp
    Source/components/EditBeatView.h
    Source/components/EditBeatView.cpp
    Source/components/LookAndFeel.h
    Source/components/LookAndFeel.cpp
    Source/components/ManageSamplesView.h
    Source/components/ManageSamplesView.cpp
    Source/components/PerformView.h
    Source/components/PerformView.cpp
    Source/components/PresetsView.h
    Source/components/PresetsView.cpp
    Source/components/TopBar.h
    Source/components/TopBar.cpp
    Source/components/TransportBar.h
    Source/components/TransportBar.cpp
    Source/model/AbletonLink.h
    Source/model/AbletonLink.cpp
    Source/model/JuceState.h
    Source/model/TapModel.h
    Source/model/TapModel.cpp
    Source/model/TickModel.h
    Source/model/TickModel.cpp
    Source/utils/SamplesPaint.h
    Source/utils/SamplesPaint.cpp
    Source/utils/TickSplash.h
    Source/utils/TickSplash.cpp
    Source/utils/UtilityFunctions.h
    Source/utils/UtilityFunctions.cpp
)

if(APPLE_IOS)
    target_sources(TICK
        PRIVATE
        Source/utils/UtilityFunctions.mm
    )

    target_sources(TICK_Standalone PRIVATE "Installer/Factory")
    set_source_files_properties("Installer/Factory" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
endif()

# Standalone är avstängt, så vi binder inte dessa källfiler
# target_sources(TICK_Standalone
#     PRIVATE
#     Source/Standalone.cpp
# )

target_compile_definitions(TICK
    PUBLIC
    DONT_SET_USING_JUCE_NAMESPACE=1
    JUCE_DONT_DECLARE_PROJECTINFO=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_CONTENT_SHARING=1
    # Nätverk: ASIO krävs för Ableton Links asynkrona trådar
    ASIO_STANDALONE=1
)

# OS-Specifika flaggor för att Ableton Link ska hitta rätt nätverksbibliotek
if(WIN32)
    target_compile_definitions(TICK PUBLIC
        LINK_PLATFORM_WINDOWS=1
        _WIN32_WINNT=0x0A00 # Tvinga fram Windows 10 SDK för att få tillgång till htonll
        WIN32_LEAN_AND_MEAN=1 # Förhindra att windows.h blockerar winsock2.h!
    )
elseif(APPLE)
    target_compile_definitions(TICK PUBLIC LINK_PLATFORM_MACOSX=1)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(TICK PUBLIC LINK_PLATFORM_LINUX=1)
endif()

juce_add_binary_data(TickBinaryData
    SOURCES
    factory_default.preset
    factory_samples.zip
    Media/Font/KlokanTechNotoSans-Bold.ttf
    Media/Font/KlokanTechNotoSans-Italic.ttf
    Media/Font/KlokanTechNotoSans-Regular.ttf
    Media/Logo/tick_icon.svg
    Media/Logo/tick_icon_with_text.svg
    Media/about_text.txt
    Media/arrow_back_ios-24px.svg
    Media/background.png
    Media/check_circle-24px.svg
    Media/edit-24px.svg
    Media/delete-24px.svg
    Media/expand_more-24px.svg
    Media/folder_open-24px.svg
    Media/ios_share_black_24dp.svg
    Media/lock_clock-24px.svg
    Media/metro_tick_icon.svg
    Media/more_horiz-24px.svg
    Media/more_vert-24px.svg
    Media/play-button.svg
    Media/sample-icon.svg
    Media/settings_black_24dp.svg
    Media/stop-button.svg
    Media/unfold_more_black_24dp.svg
)

add_custom_target(
    generate_git_revision
    COMMAND ${CMAKE_COMMAND}
    -Dlocal_dir="${CMAKE_CURRENT_SOURCE_DIR}"
    -Doutput_dir="${CMAKE_CURRENT_SOURCE_DIR}/Source/utils"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/gitversion.cmake")

add_dependencies(TICK generate_git_revision)

juce_generate_juce_header(TICK)

target_link_libraries(TICK
    PRIVATE
    TickBinaryData
    juce::juce_audio_utils
    juce::juce_dsp
    samplerate
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Nätverk: Ableton Link är "Header-Only". Eftersom dess inbyggda CMake-Alias 
# göms av FetchContent, länkar vi manuellt Windows nätverksbibliotek istället!
if(WIN32)
    target_link_libraries(TICK PRIVATE ws2_32 mswsock)
endif()

target_include_directories(TICK
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Source
    ${CMAKE_SOURCE_DIR}/JUX
    ${CMAKE_SOURCE_DIR}/libsamplerate/include
    ${ableton_link_SOURCE_DIR}/include
    ${ableton_link_SOURCE_DIR}/modules/asio-standalone/asio/include
    ${IOS_INCLUDES}
)

# install(TARGETS TICK_Standalone
#         DESTINATION .)

install(TARGETS TICK_VST3
        DESTINATION .)