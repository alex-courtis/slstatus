cmake_minimum_required(VERSION 3.13)
project(slstatus C)

set(CMAKE_C_STANDARD 11)

find_package(X11 REQUIRED)
find_package(Boost COMPONENTS program_options REQUIRED)

link_libraries(${X11_LIBRARIES})

include_directories(.)

add_executable(slstatus
        components/battery.c
        components/cpu.c
        components/datetime.c
        components/disk.c
        components/entropy.c
        components/hostname.c
        components/ip.c
        components/kernel_release.c
        components/keyboard_indicators.c
        components/keymap.c
        components/load_avg.c
        components/netspeeds.c
        components/num_files.c
        components/ram.c
        components/run_command.c
        components/swap.c
        components/temperature.c
        components/uptime.c
        components/user.c
        components/volume.c
        components/wifi.c
        arg.h
        config.h
        slstatus.c
        slstatus.h
        util.c
        util.h)
