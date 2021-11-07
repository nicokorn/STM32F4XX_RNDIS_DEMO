# STM32_HAL_RNDIS
Author: 
<br>Nico Korn

Project: 
<br>RNDIS for ST HAL library

Used target: 
<br>Blackpill board with stm32f411

Usage:	
<br>To have platform independent network interfaces over usb which is working with Linux, Windows, Mac OS ect.

Demo video:
https://www.youtube.com/watch?v=yCingW1lEwQ

Description:
<br> Remote NDIS (RNDIS) is a bus-independent class specification for Ethernet (802.3) network devices on dynamic Plug and Play (PnP) buses such as USB, 1394, Bluetooth, and InfiniBand. Remote NDIS defines a bus-independent message protocol between a host computer and a Remote NDIS device over abstract control and data channels. Remote NDIS is precise enough to allow vendor-independent class driver support for Remote NDIS devices on the host computer.
<br>This rndis project is based on the HAL library and uses FreeRTOS and the FreeRTOS TCP/IP stack.
<br>Just flash the image on to a blackpill board with stm32f411 and plug it into you computer usb port. You computer will register a new ethernet interface based on rndis and will be getting immediately an ip address from the dhcp server on the blackpill.
<br>On the TCP/IP stack I developed lightweight Web, DHCP and DNS servers. You can call the Webserver by entering "rndis.go" in your webrowser. The following dns request will be handled by the implemented DNS server. 
<br>On the webserver is a small overview of the system and the possibilty to control the blue board led.
<br>For frame management I implemented a ringbuffer "queuex". The ringbuffers parameters can be found in its header file. I'm using staticly allocated memory for better performance. Each interface has its own ringbuffer, so they don't block each other.
I tried also a linked list with heap allocation, but that apporach was less performand due to memory allocation during runtime but memory wise it was more efficient.
Data handling on the rndis usb interface is zero copy -> As soon as a complete frame has been received the head will jump to the next ringbuffer slot (if it is not occupied by the tail of course).
It should be easy to port the library to other st mcu's. Generate a new cdc usb project with cubemx and replace the usb relevant rndis files with the ones from this project.

Pictures:
<br><img src="https://github.com/nicokorn/STM32F4XX_RNDIS_DEMO/docs/screenshot.PNG" alt="px1">
<br><img src="https://github.com/nicokorn/STM32F4XX_RNDIS_DEMO/docs/blackpill.PNG" alt="px2">

Info: 
<br>The rndis library is based on the library from Sergey Fetisov: https://github.com/fetisov/lrndis and is using a rndis protocol library from Colin O'Flynn. Many thanks for your efforts! For more general information about RNDIS visit: https://docs.microsoft.com/de-de/windows-hardware/drivers/network/overview

<br>Versions: 
<br>STM32_HAL_RNDIS 0.2.0.0 
<br>FreeRTOS Kernel V10.3.1 
<br>STM32Cube_FW_F4_V1.26.0 
<br>STM32CubeMX 6.3.0 

