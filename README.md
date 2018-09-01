# lcd_screen
This little program interacts with a DPI screen on raspberry pi when using HDMI. Suitable and tested for Raspberry pi with retropie software. 

# Installation
1. Copy it in /home/pi/lcd_screen direcrtory
2. MAKE: gcc ./lcd_screen/lcd.c -o ./lcd_screen/lcd
3. CHMOD: chmod +x ./lcd_screen/lcd
4. RUN: sudo ./lcd_screen/lcd path/file.bmp usec_per_frame 

when "path/file.bmp" is bmp image, "usec_per_frame" is value in microsecons between frame.
