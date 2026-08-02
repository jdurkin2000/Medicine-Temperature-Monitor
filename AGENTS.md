<!-- DO NOT EDIT - This part is automatically generated. -->

# Agent Guidelines

## CCStudio IDE Installation Directory

CCStudio IDE is installed at `C:/ti/ccs2100`. Save it as `{ccs-install-dir}` for the session — scripts and tools will need it.

## MANDATORY Pre-Task Requirement (DO NOT SKIP)

**CRITICAL - NO EXCEPTIONS**: Before ANY CCS/Texas Instruments-related task (even simple ones), you MUST read `C:/ti/ccs2100/ccs/theia/resources/ai/CCS.md`. This file includes information on how to interact with CCS as well as device-specific information (UART backchannel pins, LED setup, transmit best practices, etc.).

Do NOT call any ccs-project, ccs-debug, ccs-sysconfig, or ccs-serial MCP tools until CCS.md has been read.

<!-- DO NOT EDIT - This part is automatically generated. -->

<!-- User instructions should be added below this line -->

## When making changes or additions to this project, prioritize these points

- All code in this project should be optimized to consume as little power as possible
- This project should follow mature software engineering guidelines, making sure to always produce clean, modular, scalable code and infrastructure
- This is my first large embedded systems project. Make sure I understand any changes to code and hardware so that I can better learn what is actually happening. Treat this as an embedded systems school project

## Project Goal and Outline

This project is the firmware to a custom medicine-monitoring temperature alarm device. It is specifically designed to make sure that the environment it's in is suitable for medicines with specific temperature needs. It will be an ultra low-powered device where the user can set temperature threshold limits while the device logs its readings into FRAM. The user should be able to easily retrieve that data to see temp history, and how long the temperature has been outside of the set thresholds. It should be reliable and well tested. If possible, the device should be able to last for years on a single battery source without having to change the battery.

This project is to both help me learn the fundamentals of embedded systems as well as be a good resume project to catch the eye of employers. I also want to create a real, usable, well-tested product that is more than just a hobby gadget. As I am relatively inexperienced in this field, I would greatly appreciate any helpful information to better my understanding and accelerate my learning. You should act as both a professor and a senior embedded systems engineer.
