# Function_Generator
Simple function generator for STM32 H723 series (Nucleo Board) with an SH1106 small OLED display.

I created this project not to be some high performance device. Its maximum output frequency is 999 KHz. 

I created it for the following reasons:
1. Develop an API for my own implementation, using a wgen.h and wgen.c file.
2. Using I2C, albeit only to a single peripheral on the bus (the OLED display).
3. Implementing DMA. This communication stays on-board, from the Cortex M7 processor to the Digital to Analog Converter (DAC).
4. Handling interrupts: in this case from the rotary encoder.
5. Developing graphics. I used Piskel to create all of the sprites:  https://www.piskelapp.com/
6. Having the challenge of being able to dynamically change the output frequency while the device is still transmitting; you can create a "tune"
of sorts by turning the rotary encoder if the output is connected to a piezeo speaker.  This involves having to dynamically change the DMA interrupt time via
 ARR (Auto Reload Register) IF the change in frequency is significant enough such that the size of the waveform (in samples) must change. In any case,
a change in frequency stops and starts the DMA transmission, though time-wise, it's not noticeable.
8. Dynamic waveform generation. At first, I wanted an excuse to implement the CORDIC feature of the H723 (realtime). However, I just ended up having the device write
to an array of a size dependent on the output frequncy (upon significant frequency switch), and then have the DMA send data from that array.
9. Pushbutton debouncing.
10. Increased familiarity with the STM32 ecosystem

## Features

-Square wave with 9 (10% to 90%) duty cycles. Ramp waveform with 9 (10% to 90%) symmetry settings, and a sine wave.

-Rotary encoder functions as the scroll device, and the user button selects options.

![IMG_6992](https://github.com/user-attachments/assets/dc0edab3-b745-4e39-b0e3-72ba271213c1)
![IMG_6995](https://github.com/user-attachments/assets/573a13e8-6372-4bd8-98bd-445402149bd7)
![IMG_6993](https://github.com/user-attachments/assets/0b2ba18d-d3ea-4a5d-8092-e7088394db8a)
![IMG_6929](https://github.com/user-attachments/assets/a256b3d9-8069-4d24-b6c2-8bdb92c5bd4e)
![IMG_6928](https://github.com/user-attachments/assets/d1e97b0d-962f-4eb0-a4de-91a48d16180a)
![IMG_6927](https://github.com/user-attachments/assets/eb08ddb8-0793-4e37-b252-142ff1ce265f)

Also attached is my unpolished excel file with values I used to determine the required output buffer (waveform) size, and the required ARR settings.
