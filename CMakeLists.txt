cmake_minimum_required(VERSION 3.14)

if (NOT DEFINED ENV{VITASDK} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Please define VITASDK env var to point to your SDK path or set CMAKE_TOOLCHAIN_FILE.")
endif ()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Toolchain file")

project(so_loader C CXX)

include("${VITASDK}/share/vita.cmake" REQUIRED)

# Vita application configuration
set(VITA_APP_NAME "Bloons 2")
set(VITA_TITLEID "BLNS20001")
set(VITA_VPKNAME "Bloons2")
set(VITA_VERSION "01.00")
set(VITA_MKSFOEX_FLAGS "-d ATTRIBUTE2=12")
set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2800000000000000")
add_definitions(-DTITLE_ID="${VITA_TITLEID}")

# Optional: used for "send" and "dump" targets
set(PSVITAIP "192.168.0.198" CACHE STRING "PSVita IP (for FTP access)")
set(PARSECOREPATH "$ENV{PARSECOREPATH}" CACHE STRING "vita-parse-core main.py")

# Game-specific definitions
set(DATA_PATH "ux0:data/bloons2/" CACHE STRING "Path to data (with trailing /)")
set(SO_PATH "${DATA_PATH}libbloons2.so" CACHE STRING "Path to .so")
set(APK_PATH "${DATA_PATH}game.apk" CACHE STRING "Path to the game's .apk")
add_definitions(-DDATA_PATH="${DATA_PATH}" -DSO_PATH="${SO_PATH}" -DAPK_PATH="${APK_PATH}")

option(USE_HIGHRES_ASSETS "Use the game's high resolution (\"retina\") sprite sheets" ON)
if (USE_HIGHRES_ASSETS)
  add_definitions(-DUSE_HIGHRES_ASSETS=1)
else()
  add_definitions(-DUSE_HIGHRES_ASSETS=0)
endif()

option(SKIP_SO_HASH_CHECK "Don't warn when libbloons2.so isn't the supported version" OFF)
if (SKIP_SO_HASH_CHECK)
  add_definitions(-DSKIP_SO_HASH_CHECK)
endif()

# Unique debug definition for loader's logging to not interfere with anything
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG_SOLOADER)
  # Uncomment this for verbose FalsoJNI logging:
  #add_definitions(-DFALSOJNI_DEBUGLEVEL=1)
endif()

# makes sincos, sincosf, etc. visible
add_definitions(-D_GNU_SOURCE -D__POSIX_VISIBLE=999999)

# libbloons2.so is built for the soft-float ABI, so the loader must use the
# softfp calling convention too (see README: requires a softfp vitasdk).
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-q -O3 -g -ffast-math -mfloat-abi=softfp -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=gnu++20 -Wno-write-strings -Wno-psabi")

add_executable(${CMAKE_PROJECT_NAME}
               source/main.c
               source/dynlib.c
               source/java.c
               source/patch.c
               source/game/achievements.c
               source/game/apk.c
               source/game/audio.c
               source/game/coco.c
               source/game/context.c
               source/game/input.c
               source/game/prefs.c
               source/game/settings.c
               source/game/texture.c
               source/reimpl/bits/_ctype.c
               source/reimpl/errno.c
               source/reimpl/io.c
               source/reimpl/libc.c
               source/reimpl/log.c
               source/reimpl/mem.c
               source/reimpl/pthr.c
               source/reimpl/sys.c
               source/utils/dialog.c
               source/utils/glutil.c
               source/utils/init.c
               source/utils/logger.c
               source/utils/trophies.c
               source/utils/utils.c
               lib/pthread-svelte/source/cond_user.c
               lib/pthread-svelte/source/mutex_user.c
               lib/pthread-svelte/source/once.c
               lib/pthread-svelte/source/rwlock.c
               lib/falso_jni/FalsoJNI.c
               lib/falso_jni/FalsoJNI_ImplBridge.c
               lib/falso_jni/FalsoJNI_Logger.c
               lib/falso_jni/converter.c
               lib/sha1/sha1.c
               lib/so_util/so_util.c)

# vitaGL contains C++ code (its shader expression parser), but all of our own
# sources are C. Link with the C++ driver so libstdc++/libsupc++ (operator
# new/delete, exceptions, std::string) get pulled in after every library.
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(${CMAKE_PROJECT_NAME} SYSTEM
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib
                           PUBLIC ${CMAKE_SOURCE_DIR}/lib/kubridge
                           )

target_include_directories(${CMAKE_PROJECT_NAME}
                           PUBLIC ${CMAKE_SOURCE_DIR}/source
                           )

target_link_libraries(${CMAKE_PROJECT_NAME}
                      -Wl,--whole-archive pthread -Wl,--no-whole-archive
                      stdc++
                      vitaGL
                      vitashark
                      SceShaccCgExt
                      taihen_stub
                      SceShaccCg_stub
                      zip
                      bz2
                      lzma
                      zstd
                      crypto
                      z
                      m
                      mathneon
                      ${CMAKE_SOURCE_DIR}/lib/kubridge/libkubridge_stub.a
                      SceAppMgr_stub
                      SceAppUtil_stub
                      SceAudio_stub
                      SceCommonDialog_stub
                      SceCtrl_stub
                      SceDisplay_stub
                      SceGxm_stub
                      SceKernelDmacMgr_stub
                      SceNpTrophy_stub
                      ScePower_stub
                      SceRtc_stub
                      SceSysmodule_stub
                      SceTouch_stub
                      SceVshBridge_stub
                      )

vita_create_self(eboot.bin ${CMAKE_PROJECT_NAME} UNSAFE NOASLR)
vita_create_vpk(${VITA_VPKNAME}.vpk ${VITA_TITLEID} eboot.bin
                VERSION ${VITA_VERSION}
                NAME ${VITA_APP_NAME}
                FILE ${CMAKE_SOURCE_DIR}/extras/cpuinfo cpuinfo
                ${CMAKE_SOURCE_DIR}/extras/meminfo meminfo
                ${CMAKE_SOURCE_DIR}/extras/livearea/icon0.png sce_sys/icon0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/pic0.png sce_sys/pic0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/bg0.png sce_sys/livearea/contents/bg0.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/startup.png sce_sys/livearea/contents/startup.png
                ${CMAKE_SOURCE_DIR}/extras/livearea/template.xml sce_sys/livearea/contents/template.xml
                ${CMAKE_SOURCE_DIR}/extras/trophies/TROPHY.TRP sce_sys/trophy/${VITA_TITLEID}_00/TROPHY.TRP
                )

add_custom_target(send
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ${VITA_TITLEID} | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )

add_custom_target(send_kvdb
                  COMMAND echo destroy | nc ${PSVITAIP} 1338
                  COMMAND curl -T eboot.bin ftp://${PSVITAIP}:1337/ux0:/app/${VITA_TITLEID}/
                  COMMAND echo launch ux0:/app/${VITA_TITLEID}/eboot.bin | nc ${PSVITAIP} 1338
                  DEPENDS ${VITA_VPKNAME}.vpk-vpk
                  )

add_custom_target(dump
                  COMMAND ${CMAKE_SOURCE_DIR}/extras/scripts/get_dump.sh ${PSVITAIP}
                  COMMAND python3 ${PARSECOREPATH} coredump ${CMAKE_PROJECT_NAME}
                  )

add_custom_target(reboot
                  COMMAND echo reboot | nc ${PSVITAIP} 1338
                  )
