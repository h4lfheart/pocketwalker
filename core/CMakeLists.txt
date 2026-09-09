file(GLOB_RECURSE SOURCES *.cpp *.h)

add_library(pocketwalker_core STATIC ${SOURCES})
target_include_directories(pocketwalker_core PUBLIC ..)

option(POCKETWALKER_RTC_DEBUG_LOG "Write RTC catch-up debug logs next to save files" ON)
if(POCKETWALKER_RTC_DEBUG_LOG)
    target_compile_definitions(pocketwalker_core PUBLIC POCKETWALKER_RTC_DEBUG_LOG=1)
endif()
