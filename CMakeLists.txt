cmake_minimum_required(VERSION 3.19)

# Audio Sending is implemented, but not performant enough yet
add_compile_definitions(SEND_AUDIO=0)

if(NOT IDF_TARGET STREQUAL linux)
  if(NOT DEFINED ENV{WIFI_SSID} OR NOT DEFINED ENV{WIFI_PASSWORD})
    message(FATAL_ERROR "Env variables WIFI_SSID and WIFI_PASSWORD must be set")
  endif()

  add_compile_definitions(WIFI_SSID="$ENV{WIFI_SSID}")
  add_compile_definitions(WIFI_PASSWORD="$ENV{WIFI_PASSWORD}")
endif()

# if(NOT DEFINED ENV{LIVEKIT_URL} OR NOT DEFINED ENV{LIVEKIT_TOKEN})
#   message(FATAL_ERROR "Env variable LIVEKIT_URL and LIVEKIT_TOKEN must be set")
# endif()

# add_compile_definitions(LIVEKIT_URL="ws://192.168.1.35:7880")

add_compile_definitions(LIVEKIT_URL="wss://demo-q060r4b9.livekit.cloud")
add_compile_definitions(LIVEKIT_TOKEN="eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJuYW1lIjoiYmlwaSBlc3AgZmlybXdhcmUiLCJ2aWRlbyI6eyJyb29tSm9pbiI6dHJ1ZSwicm9vbSI6ImJpcGktcm9vbSIsImNhblB1Ymxpc2giOnRydWUsImNhblN1YnNjcmliZSI6dHJ1ZSwiY2FuUHVibGlzaERhdGEiOnRydWV9LCJzdWIiOiJmaXJtd2FyZSIsImlzcyI6IkFQSVRIZ0RXemI4R1ZjWCIsIm5iZiI6MTc0NjQxMTM5MSwiZXhwIjoxODMyODExMzkxfQ.8SE2LyAtpxM8zMN_dXPDKPTo0K8e1F8GaAp6JziXlCY")

set(COMPONENTS src)
set(EXTRA_COMPONENT_DIRS "src" "components/srtp" "components/peer" "components/esp-libopus" "components/esp-protocols/components/esp_websocket_client")

if(IDF_TARGET STREQUAL linux)
	add_compile_definitions(LINUX_BUILD=1)
  list(APPEND EXTRA_COMPONENT_DIRS
    $ENV{IDF_PATH}/examples/protocols/linux_stubs/esp_stubs
    "components/esp-protocols/common_components/linux_compat/esp_timer"
    "components/esp-protocols/common_components/linux_compat/freertos"
    )
endif()

if(NOT DEFINED ENV{SKIP_PROTOBUF_GENERATE})
  execute_process(
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "protoc" "--c_out=deps/livekit-protocol-generated" "-I=./deps/livekit-protocol" "livekit_rtc.proto" "livekit_models.proto"
    COMMAND_ERROR_IS_FATAL ANY
  )
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(src)
