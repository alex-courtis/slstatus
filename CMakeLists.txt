cmake_minimum_required(VERSION 3.13)
project(slstatus C)

set(CMAKE_C_FLAGS "-std=c99 -pedantic -Wall -Wextra -Os")

add_compile_definitions(
        _DEFAULT_SOURCE)

add_executable(slstatus
        slstatus.c
        slstatus.h
        arg.h
        config.h
        config.mk
        util.c
        util.h
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
        components/lm_sensors.c)

target_link_libraries(slstatus
        X11
        sensors)
