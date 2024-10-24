# SaoScope

This is a small osciloscope in a SAO form factor based on the STM32G030. It can sample up to 1.75Msps at 12 bit, and up to 3.5Msps at 6 bits. The scope input can read inputs from -15V to +15V.

# Circuit Description

SaoScope is built on a low end STM32G030 MCU with 8K RAM and 32K flash, a 128x64 SSD1306 OLED screen, and a simple 1MOhm impedance input circuit.

Being a Silly Add On, SaoScope gets power from the SAO connector, normally 3.3V. The MCU's secondary I2C peripheral is connected to the SAO connector, as well as one GPIO (PA4).

The 3.3V SAO power is used to power the OLED screen, and is fed through an LDO to get 3.0V to reduce noise in the analog domain. A -3.0V power rail is generated using a charge pump. The +3.0 and -3.0 power rails are used for an input buffer op amp, while the STM32 runs on the positive 3V rail.

Signals to be scoped run through a divider (1MOhm imedance), buffer with an op amp that is powered with the positive and negative rail, and then shifted back up to 0-3V centered on 1.5V, and read with an ADC input. 

A GPIO from the STM32 is run to the SIG pin, which can be used to generate square wave signals using a timer (TIM17), and can also be used with the I2S peripheral to generate or capture arbitrary high speed digital signals. It could also be used as an external trigger for the scope.

The main I2C peripheral connects to the SSD1306 OLED screen. It isn't shared with anything else (like the SAO I2C) so it can run at high speed.

Four dials and four buttons are used to control the scope. To save GPIO these are connected together. A button press will pull the signal near zero, and while unpressed the signal will represent the potentiometer setting, scaled to around 80% of full range.

# Dev environment setup

## Software / IDE
To set up the development environment for this project, you'll need VSCode, the STM32 extension, and some external STM32 tools. Follow the STM32 vscode extension setup instructions.

https://marketplace.visualstudio.com/items?itemName=stmicroelectronics.stm32-vscode-extension

The tl;dr is you also need some tools from ST:

* STM32CubeCLT v1.15.0 or later
* STM32CubeMX v6.11.0 or later (for changing peripheral setup)

## Hardware

You'll need a SWD programmer/debugger. Something like an ST-Link, Segger, or maybe even a [Raspberry Pi Pico](https://github.com/raspberrypi/debugprobe?tab=readme-ov-file)

There are connections on the underside of the board for this.

# Files Overview

## Scope related files

| File Name | Description |
|-----------|-------------|
| app.c     | Contains the main app code so we don't have to edit the generated main.c file |
| sampler.c | Handles sampling from the ADC |
| scope.c   | Scope modes, controls, settings, graphing |
| trigger.c | Handles setting up ADC watchdogs for edge triggers |
| File Name | Description |

## Library files

| File Name  | Description                  |
|------------|------------------------------|
| button.c   | Button debounce library file |
| ssd1306*.c | SSD1306 library              |

## Generated files

Most of these are generated by STM32Cube tooling. In general avoid doing too much code in these and call out to code elsewhere.
| File Name            | Description                                                                 |
|----------------------|-----------------------------------------------------------------------------|
| main.c               | Generated main code from STM32Cube, basic peripheral init                   |
| stm32g0xx_it.c       | Generated interrupt hooks, keep it minimal and jump out to other handlers   |
| syscalls.c           | Generated stub functions for C library                                      |
| sysmem.c             | Generated file, hooks newlib up with stack                                  |
| system_stm32g0xx.c   | Generated, sets up the clock                                                |

# MCU Peripheral Useage

TIM17 is used to generate PWM waveforms on the SIG pin.

