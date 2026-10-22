# Standalone rebuild of MiniGL clients: all SDL modules use the selected SDK.
cmake_minimum_required(VERSION 3.20)
project(JazzMiniGLClients C ASM)
set(MGL_ROOT "" CACHE PATH "MiniGL source root containing libraryroot and backend")
set(SDL_ROOT "" CACHE PATH "SDL 1.2 MiniGL source tree")
set(SDL_EXTRA_INCLUDE "${SDL_ROOT}/../include" CACHE PATH "SDL port compatibility include directory")
# Prepare compatibility changes in the build tree, preserving the SDL checkout.
set(_sdl_copy "${CMAKE_CURRENT_BINARY_DIR}/sdl-src")
file(COPY "${SDL_ROOT}/" DESTINATION "${_sdl_copy}"
  FILES_MATCHING PATTERN "*.c" PATTERN "*.h")
file(COPY "${SDL_EXTRA_INCLUDE}/SDL" DESTINATION "${_sdl_copy}/include"
  FILES_MATCHING PATTERN "*.h")
set(SDL_ROOT "${_sdl_copy}")
set(_image "${SDL_ROOT}/video/amigaos/SDL_cgximage.c")
file(READ "${_image}" _image_source)
# Current SDKs require machine/types.h to be reached through sys/types.h.
string(REPLACE "#include <machine/types.h>" "#include <sys/types.h>"
  _image_source "${_image_source}")
file(WRITE "${_image}" "${_image_source}")
set(_timer "${SDL_ROOT}/timer/amigaos/SDL_systimer.c")
file(READ "${_timer}" _timer_source)
string(REPLACE "struct Library *TimerBase;" "struct Device *TimerBase;"
  _timer_source "${_timer_source}")
file(WRITE "${_timer}" "${_timer_source}")
set(_wrapper "${SDL_ROOT}/video/amigaos/SDL_cgxgl_wrapper.c")
file(READ "${_wrapper}" _wrapper_source)
foreach(_old_fallback IN ITEMS
    "#define glInterleavedArrays(format, stride, pointer) ((void)0)")
  string(REPLACE "${_old_fallback}" "/* Use the v29 SDK dispatch wrapper. */"
    _wrapper_source "${_wrapper_source}")
endforeach()
file(WRITE "${_wrapper}" "${_wrapper_source}")
set(_sdk "${MGL_ROOT}/libraryroot")
add_library(minigl_dispatch STATIC
  "${_sdk}/client/minigl_base.c"
  "${_sdk}/client/minigl_open.c"
  "${_sdk}/client/minigl_dispatch_stub_gcc.s")
target_include_directories(minigl_dispatch PRIVATE "${_sdk}/include")
target_compile_options(minigl_dispatch PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-mcpu=68060>)

# Source list of the existing SDL port, excluding optional Apollo assembly.
set(_sdl_sources
  audio/SDL_audio.c audio/SDL_audiocvt.c audio/SDL_mixer.c
  audio/SDL_mixer_m68k.c audio/SDL_wave.c audio/amigaos/SDL_ahiaudio.c
  SDL_error.c SDL_fatal.c video/SDL_RLEaccel.c video/SDL_blit.c
  video/SDL_blit_0.c video/SDL_blit_1.c video/SDL_blit_A.c video/SDL_blit_N.c
  video/SDL_bmp.c video/SDL_cursor.c video/SDL_pixels.c video/SDL_surface.c
  video/SDL_stretch.c video/SDL_yuv.c video/SDL_yuv_sw.c video/SDL_video.c
  timer/amigaos/SDL_systimer.c timer/SDL_timer.c joystick/SDL_joystick.c
  joystick/SDL_sysjoystick.c SDL_cdrom.c SDL_syscdrom.c
  events/SDL_quit.c events/SDL_active.c events/SDL_keyboard.c events/SDL_mouse.c
  events/SDL_resize.c file/SDL_rwops.c SDL.c events/SDL_events.c
  thread/amigaos/SDL_sysmutex.c thread/amigaos/SDL_syssem.c
  thread/amigaos/SDL_systhread.c thread/amigaos/SDL_thread.c
  thread/amigaos/SDL_syscond.c video/amigaos/SDL_cgxvideo.c
  video/amigaos/SDL_cgxmodes.c video/amigaos/SDL_cgximage.c
  video/amigaos/SDL_amigaevents.c video/amigaos/SDL_amigamouse.c
  video/amigaos/SDL_cgxgl.c video/amigaos/SDL_cgxwm.c
  video/amigaos/SDL_cgxyuv.c video/amigaos/SDL_cgxaccel.c
  video/amigaos/SDL_cgxgl_wrapper.c video/SDL_gamma.c stdlib/SDL_stdlib.c
  stdlib/SDL_string.c stdlib/SDL_malloc.c stdlib/SDL_getenv.c)
list(TRANSFORM _sdl_sources PREPEND "${SDL_ROOT}/")
add_library(SDL_mgl STATIC ${_sdl_sources})
target_include_directories(SDL_mgl PRIVATE
  "${_sdk}/include" "${SDL_ROOT}" "${SDL_EXTRA_INCLUDE}"
  "${SDL_ROOT}/include" "${SDL_ROOT}/include/SDL")
target_compile_options(SDL_mgl PRIVATE -std=gnu99
  -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types
  -Wno-error=implicit-int -Wno-error=int-conversion -Wno-error=return-mismatch)
target_compile_definitions(SDL_mgl PRIVATE
  NO_AMIGADEBUG HAVE_OPENGL USE_MGLAPI SDL_VIDEO_OPENGL NO_SIGNAL_H
  SDL_AUDIO_DRIVER_AHI ENABLE_AHI NOIXEMUL SDL_MGL_SHARED
  GfxBase=JazzSdlGfxBase IntuitionBase=JazzSdlIntuitionBase
  CyberGfxBase=JazzSdlCyberGfxBase LowLevelBase=JazzSdlLowLevelBase
  TimerBase=JazzSdlTimerBase KeymapBase=JazzSdlKeymapBase)
