
// Access from ARM Running Linux
 
#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
 
 
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/sysinfo.h>

#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

// Screen setting
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 800

// Global variables
int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;

// screen buffer
unsigned char red_ch[SCREEN_WIDTH * SCREEN_HEIGHT] = {0}; // 480*800
unsigned char green_ch[SCREEN_WIDTH * SCREEN_HEIGHT] = {0};
unsigned char blue_ch[SCREEN_WIDTH * SCREEN_HEIGHT] = {0}; 
long pixel = 0;
 
// Functions declaration
void setup_io();
void clc(int frame);
void red(unsigned char r);
void green(unsigned char r);
void blue(unsigned char r);
void clc_data(int frame);
void v_sync(int frame);
void h_sync(int frame);
int ReadBMP(char* filename, unsigned char *buffer);
int DrawPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
int ClearBuffer(void);
int ReadTemp(void);
int ReadBMP_Height(char* filename);
int ReadBMP_Width(char* filename);
void DrawIMG(int start_x, int start_y, unsigned char *buffer, int width, int height);
void MergeIMG(int start_x, int start_y, unsigned char *img, unsigned char *buffer, int img_width, int img_height, int buf_width);
void RotateIMG(unsigned char *buffer, int width, int height);
int DrawTransparrentPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b);
void DrawTransparrentIMG(int start_x, int start_y, unsigned char *buffer, int width, int height);
void DrawDigits(int dig, int start_x, int start_y, unsigned char *buf, int width, int height);
int DrawRectangle(int start_x, int start_y, int end_x, int end_y, unsigned char r, unsigned char g, unsigned char b);
void CpuMemUpdate(void);

//////////////////////////////////////main/////////////////////////
int main(int argc, char *argv[])
{	
  int time_sleep = 16000; // time between frame. in usec
  int g; //for gpio
  int height;
  int width;

  unsigned char buf[SCREEN_WIDTH * SCREEN_HEIGHT * 3];// buf contain argv[1] img. 3 is 3 byte: RGB

  //start argv[1] img coordinats on lcd screen 
  int start_x;
  int start_y;

  /* argv[1] points to the 1st parameter - picture file name*/
  /* argv[2] points to the 2st parameter - time sleep */	
  /* argv[argc] is NULL */
  //printf("argc = %i\n", argc);
  if (argc >= 3) 
  {
	//read time sleep between frame
    time_sleep = atoi(argv[2]);
	//printf("argv[2]: %s\n", argv[2]);
  }
  if (argc >= 2)
  {
	  // open and draw in screen buffer file argv[1]
	  //printf("argv[1]: %s\n", argv[1]);
	height = ReadBMP_Height(argv[1]);
	width = ReadBMP_Width(argv[1]);

	ReadBMP(argv[1], buf);

	start_x = (SCREEN_WIDTH - width) / 2;    //center
	start_y = SCREEN_HEIGHT - height;  //top
	//fill screen buffer
	DrawIMG(start_x, start_y, buf, width, height);
  }

  // Load digits in another buffer
  char path[] = "./lcd_screen/digits.bmp";
  int dig_width = ReadBMP_Width(path);
  int dig_height = ReadBMP_Height(path);
  int dig_size = dig_width * dig_height * 3;
  unsigned char digit_buffer[dig_size];
  ReadBMP(path, digit_buffer);

  // Set up gpi pointer for direct register access
  setup_io();
 
  // Switch GPIO 0..21 to output mode
  // Set GPIO pins 0-21 to output
  for (g=0; g<=21; g++)
  {
    INP_GPIO(g); // must use INP_GPIO before we can use OUT_GPIO
    OUT_GPIO(g);
  }
  //clear GPIO 0...21
  for (g=0; g<=21; g++)
  {
      GPIO_CLR = 1<<g;
  }

  //lcd screen parameter
  int vsync_frame = 2;
  int vertical_back_porch = 3;
  int lcd_height = SCREEN_HEIGHT;
  int vertical_front_porch = 4;

  int hsync_frame = 16;
  int horizontal_back_porch = 24;
  int lcd_width = SCREEN_WIDTH;
  int horizontal_front_porch = 16;


  int column = 0;
  int n = 0;
  while (1)  // one cycle is one frame
  {
	//screen buffer manupulation
	if(n == 0)	//every 2000000/time_sleep sec
	{
		//update pic
	    ClearBuffer();
        DrawIMG(start_x, start_y, buf, width, height); //img from argv[1]

		//update temperature
		int temp = ReadTemp();
		//printf("Temp: %i\n", temp);
		DrawDigits(temp, 15, 0, digit_buffer, dig_width, dig_height);

		//update cpu, memory load
		CpuMemUpdate();

		n = 2000000/time_sleep;
	}
	n--;

	//lcd screen sequence
	for(column=0; column<vsync_frame; column++) // v_sync
	{
		GPIO_SET = 1<<2;//vsync gpio
		h_sync(hsync_frame);// hsync
		clc(horizontal_back_porch);//horizontal back porch
		clc(lcd_width);// zero data
		clc(horizontal_front_porch);//horizontal front porch
		GPIO_CLR = 1<<2;//vsync gpio
	}	
	for(column=0; column<vertical_back_porch; column++) // vertical back porch
	{
		h_sync(hsync_frame);// hsync
		clc(horizontal_back_porch);//horizontal back porch
		clc(lcd_width);// zero data
		clc(horizontal_front_porch);//horizontal front porch
	}	
	for(column=0; column<lcd_height; column++) //lcd data
	{
		h_sync(hsync_frame);
		clc(horizontal_back_porch); //horizontal back porch
		GPIO_SET = 1<<1; //de gpio (display enable)
		clc_data(lcd_width); // buffer data
		GPIO_CLR = 1<<1;
		clc(horizontal_front_porch); //horizontal front porch
	}
	for(column=0; column<vertical_front_porch; column++) // vertical front porch
	{
		h_sync(hsync_frame);
		clc(horizontal_back_porch);
		clc(lcd_width);
		clc(horizontal_front_porch);
	}
	pixel = 0; //this use in clc_data() function

	//CPU relax 
	usleep(time_sleep);
  }// while
  return 0;
} // main

//////////////////////functions here/////////////////////////////

void CpuMemUpdate(void)
{
	//function update memory and cpu load into screen buffer
	//memory and cpu load its small rectangle in bottom lcd screen

	struct sysinfo si;
	sysinfo(&si);

	int mem_x = SCREEN_WIDTH - SCREEN_WIDTH * (si.freeram >> 10) / (si.totalram >> 10);
	DrawRectangle(mem_x, 0, SCREEN_WIDTH, 3, 200, 40, 100);
	
	//printf("CPU Load: %lu\n", si.loads[0]);
	int cpu_x = SCREEN_WIDTH - SCREEN_WIDTH * si.loads[0] / 100000; 
	DrawRectangle(cpu_x, 3, SCREEN_WIDTH, 6, 220, 100, 40);
}

void DrawDigits(int dig, int start_x, int start_y, unsigned char *buf, int width, int height)
{
	//this function draw digits from input value
	//this function not universal. only draw temperature from dig in format "12.345"

	int digits_per_width = 4;
	int digits_pet_height = 3;

	int dig_width = width / digits_per_width; 
    int dig_height = height / digits_pet_height; 
	int dig_size = dig_width * dig_height * 3;
	int i = 0;
	int j = 0;
	int n = 0;

    unsigned char digit[11][dig_size];

	// digit parser
	// from bufer *buf to array digit[][]
	// one item digit[] is one number
	MergeIMG(dig_width*0, 0, digit[0], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*1, 0, digit[1], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*2, 0, digit[2], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*3, 0, digit[3], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*0, dig_height, digit[4], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*1, dig_height, digit[5], buf, dig_width, dig_height, width);
    MergeIMG(dig_width*2, dig_height, digit[6], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*3, dig_height, digit[7], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*0, dig_height*2, digit[8], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*1, dig_height*2, digit[9], buf, dig_width, dig_height, width);
	MergeIMG(dig_width*2, dig_height*2, digit[10], buf, dig_width, dig_height, width);//digit[10] = space

	//rotate all digits
	for(i = 0; i < 11; i++)
	{
		RotateIMG(digit[i], dig_width, dig_height);
	}

	char temp[6];
	sprintf(temp, "%d", dig);

	i = 0;
	for(n = 0; n < 6; n++) // 6 digit max per lcd line
	{ 
		if(n != 2) // 3 digit is space
		{
			// digit
			int number = temp[i]-'0'; // char {0,1,2,3,4...} -> int 0, 1, 2, 3, 4...
			DrawTransparrentIMG(dig_width*(5-n)+start_x, dig_height*0+start_y, digit[number], dig_width, dig_height);
			i++;
		}
		else
		{
			// space
			DrawTransparrentIMG(dig_width*(5-n)+start_x, dig_height*0+start_y, digit[10], dig_width, dig_height);
		}
	}

}


void RotateIMG(unsigned char *buffer, int width, int height)
{
	// rotate img from buffer into 180 degrees
	unsigned char tmp1;
	unsigned char tmp2;
	unsigned char tmp3;	
	int size = width * height * 3;
	for(int i = 0; i < size/2; i=i+3)
	{
		tmp1 = buffer[i]; // Red
		tmp2 = buffer[i+1]; // Green
		tmp3 = buffer[i+2];	// Blue

		buffer[i] = buffer[size - i - 3];
		buffer[i+1] = buffer[size - i - 2];
		buffer[i+2] = buffer[size - i - 1];

		buffer[size - i - 1] = tmp3;
		buffer[size - i - 2] = tmp2;
		buffer[size - i - 3] = tmp1;			
	}
}

int DrawRectangle(int start_x, int start_y, int end_x, int end_y, unsigned char r, unsigned char g, unsigned char b)
{
	//draw rectangle in screen buffer
	if(end_x > SCREEN_WIDTH) end_x = SCREEN_WIDTH;
	if(end_y > SCREEN_HEIGHT) end_y = SCREEN_HEIGHT;
	if(start_x > end_x || start_y > end_y) return 0;

	for(int y = start_y; y < end_y; y++)
    {
        for(int x = start_x; x < end_x; x++)
        {
			DrawPixel(x, y, r, g, b);
		}
	}
	return 1;
}

int DrawPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	//draw pixel in screen buffer
	if(x > SCREEN_WIDTH || x < 0 || y > SCREEN_HEIGHT || y < 0)
	{
		//printf("DrawPixel Error. X or Y exceed screen format.");
		return 0;
	}
	int coord = x + y * SCREEN_WIDTH;
	red_ch[coord] = r;
	green_ch[coord] = g;
	blue_ch[coord] = b;
	return 1;
}

int DrawTransparrentPixel(int x, int y, unsigned char r, unsigned char g, unsigned char b)
{
	//draw pixel in screen buffer
	//black data 0x00 do not draw
	if(x > SCREEN_WIDTH || x < 0 || y > SCREEN_HEIGHT || y < 0)
	{
		//printf("DrawPixel Error. X or Y exceed screen format.");
		return 0;
	}
	int coord = x + y * SCREEN_WIDTH;
	if(r != 0) red_ch[coord] = r;
	if(g != 0) green_ch[coord] = g;	
	if(b != 0) blue_ch[coord] = b;
	return 1;
}

void DrawIMG(int start_x, int start_y, unsigned char *buffer, int width, int height)
{
	//this function fill screen buffer

	//crop
	if(height > SCREEN_HEIGHT)
	{
		height = SCREEN_HEIGHT;
	}
	if(width > SCREEN_WIDTH)
	{
		width = SCREEN_WIDTH;
	}
	//fill
	for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {     
			DrawPixel(x+start_x, y+start_y, buffer[3*x + 3*y*width + 2], buffer[3*x + 3*y*width + 1], buffer[3*x + 3*y*width]);
        }
    }
}

void DrawTransparrentIMG(int start_x, int start_y, unsigned char *buffer, int width, int height)
{
	//this function fill screen buffer
	//black data 0x00 do not fill
	//crop
	if(height > SCREEN_HEIGHT)
	{
		height = SCREEN_HEIGHT;
	}
	if(width > SCREEN_WIDTH)
	{
		width = SCREEN_WIDTH;
	}
	//fill
	for(int i = 0; i < height; i++)
    {
        for(int j = 0; j < width; j++)
        {     
			DrawTransparrentPixel(j+start_x, i+start_y, buffer[3*j + 3*i*width + 2], buffer[3*j + 3*i*width + 1], buffer[3*j + 3*i*width]);
        }
    }
}

void MergeIMG(int start_x, int start_y, unsigned char *img, unsigned char *buffer, int img_width, int img_height, int buf_width)
{
	//Merge buffer to img
	for(int y = 0; y < img_height; y++)
	{
		for(int x = 0; x < img_width; x++)
		{
			img[(x+y*img_width)*3] = buffer[(x+start_x+(y+start_y)*buf_width)*3];
			img[(x+y*img_width)*3+1] = buffer[(x+start_x+(y+start_y)*buf_width)*3+1];
			img[(x+y*img_width)*3+2] = buffer[(x+start_x+(y+start_y)*buf_width)*3+2];				
		}
	}

}

int ClearBuffer(void)
{
	// clear screen buffer
	int i = 0;      
	for(i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
	{
		red_ch[i] = 0;
		green_ch[i] = 0;
		blue_ch[i] = 0;
	}
}

int ReadTemp(void)
{
	//Read temp from file in 12345 format. Its 12.345 deg C. 
	//works only on raspberry pi
	FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
	if (f == NULL) return 0;
	int temp = 0; 

	fscanf (f, "%d", &temp);
	fclose(f);

	return temp;
}

int ReadBMP(char* filename, unsigned char *buffer)
{
	//Read BMP to array buffer
    int i;
	int j;
    FILE* f = fopen(filename, "rb");
    if(f == NULL) 
	{
		printf("Can't open file %s", filename);
		return 0;	
	}

	int header_size = 54;
    unsigned char info[header_size];

	fread(info, sizeof(unsigned char), header_size, f); // read header

    // extract info from header
	int start_data_addr = *(int*)&info[10];
    int width = *(int*)&info[18];
    int height = *(int*)&info[22];
	short bit_per_pixel = *(short*)&info[28];

	if(bit_per_pixel != 24)
	{
		printf("Not a 24bit img\n");
		fclose(f);
		return 0;
	}

	//printf("Name: %s\n", filename);
	//printf("Width: %i\n", width);	
	//printf("Height: %i\n", height);
	
	if(start_data_addr > header_size)// extended header v5
	{
		int null_size = start_data_addr - header_size;
		unsigned char null[null_size];
		fread(null, sizeof(unsigned char), null_size, f);// read in null array 
	}

	int size = 3 * width * abs(height);
	//printf("size: %i\n", size);
	//unsigned char data[size];
    unsigned char tmp1;

	fread(buffer, sizeof(unsigned char), size, f); // read the rest of the data at once
    fclose(f);

	int heightSign = 1;
    if(height < 0)
	{
		//image not flip
	    heightSign = -1;
    }

	// flip img
    if(heightSign == 1){
        for(i = 0; i < height/2; i++)
        {
            for(j = 0; j < width*3; j++)
            {     
                tmp1 = buffer[j + width*3*i];
                buffer[j + width*3*i] = buffer[j + width*3*(height - 1 - i)];
                buffer[j + width*3*(height - 1 - i)] = tmp1;
            }
        }
    }

    return 1;
}

int ReadBMP_Height(char* filename)
{
	//Read BMP height from BMP header
    FILE* f = fopen(filename, "rb");
    if(f == NULL) return 0;

	int header_size = 54;
    unsigned char info[header_size];

	fread(info, sizeof(unsigned char), header_size, f); // read header

    // extract info from header
    int height = *(int*)&info[22];

    fclose(f);

    return abs(height);
}

int ReadBMP_Width(char* filename)
{
	//Read BMP width from BMP header
    FILE* f = fopen(filename, "rb");
    if(f == NULL) return 0;

	int header_size = 54;
    unsigned char info[header_size];

	fread(info, sizeof(unsigned char), header_size, f); // read header

    // extract info from header
    int width = *(int*)&info[18];

    fclose(f);

    return width;
}

void clc_data(int frame) // clock with data
{
	//Set RGB pixels from scree buffer in GPIO port and do clock frame
	int n=0;
	for (n=0; n<frame; n++)
	{
		red(red_ch[pixel]);
		green(green_ch[pixel]);
		blue(blue_ch[pixel]);
		pixel++;

		GPIO_SET = 1<<0;//clck
		GPIO_CLR = 1<<0;//clck
	}
}
void clc(int frame)// clock 
{ 
	// Do clock frame
	int n=0;
	for (n=0; n<frame; n++)
	{
		GPIO_SET = 1<<0;
		GPIO_CLR = 1<<0;
	}
}
void red(unsigned char r) // set red gpio pins 16-21
{
	// this function set all 6 bit gpio pins red color
    r = r >> 2; //normalize. 8 -> 6 bit
    GPIO_SET = r<<16; // start gpio adres - red: 16-21
	r ^= 0b111111;    // inverter
	GPIO_CLR = r<<16; // reset unuse bit
}
void green(unsigned char r) // set green gpio pins 10-15
{
    r = r >> 2;
    GPIO_SET = r<<10;
	r ^= 0b111111; 
	GPIO_CLR = r<<10;
}

void blue(unsigned char r) // set blue gpio pins 4-9
{
    r = r >> 2;
    GPIO_SET = r<<4;
	r ^= 0b111111; 
	GPIO_CLR = r<<4;
}
void v_sync(int frame) // vsync with clock
{
	//not use
	GPIO_SET = 1<<2;//vsync gpio
	clc(frame);
	GPIO_CLR = 1<<2;
}
void h_sync(int frame) // hsync with clock
{
	GPIO_SET = 1<<3;
	clc(frame);
	GPIO_CLR = 1<<3;
}
void setup_io()
{
	// Set up a memory regions to access GPIO
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }
 
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any adddress in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );
 
   close(mem_fd); //No need to keep mem_fd open after mmap
 
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
 
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
 
 
} // setup_io