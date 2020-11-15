#include <stdio.h>
#include <math.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xparameters.h"
#include "sleep.h"
#include "xgpio.h"
#include "xgpiops.h"
#include "xscutimer.h"
#include "wave/wave.h"
#include "i2c/PS_i2c.h"
#include "xil_types.h"
#include <ctype.h>
#include <stdlib.h>
#include "display_ctrl/display_ctrl.h"
#include "display_demo.h"
#include "ThicknessMeasure/ThicknessMeasure_SimilarEhco.h"

#define DYNCLK_BASEADDR		XPAR_AXI_DYNCLK_0_BASEADDR
#define VGA_VDMA_ID 		XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID 		XPAR_VTC_0_DEVICE_ID
DisplayCtrl dispCtrl;
XAxiVdma vdma;

u8 frameBuf[DISPLAY_NUM_FRAMES][DEMO_MAX_FRAME] __attribute__ ((aligned(64)));
u8 *pFrames[DISPLAY_NUM_FRAMES]; //array of pointers to the frame buffers

XIicPs IicInstance;



/*
 *Wave defines
 */
#define WAVE_LEN          	900*270*3        /* Wave total length in byte 1920 * 256 * 3 */
#define WAVE_LEN_1920		1920*270*3
#define WAVE_START_ROW      796               /* Grid and Wave start row in frame 50 */
#define WAVE_START_COLUMN   0                 /* Grid and Wave start column in frame */
#define WAVE_HEIGHT         270    	   		  /* Grid and Wave height 256 */
#define WAVE_WIDTH			900

#define MAX_ADC_LEN_FLOAT		   0x4000      /* ADC max length in byte */
#define MAX_ADC_LEN_SHORT		   0x2000

/*
 * Grid buffer
 */
u8 GridBuffer[WAVE_LEN] ;
u8 GridBuffer_1920[WAVE_LEN_1920];
/*
 * Wave buffer
 */
u8 WaveBuffer[WAVE_LEN_1920] ;
u8 WaveBufferB_Before[WAVE_LEN] ;
u8 WaveBufferA_Before[WAVE_LEN];
u8 WaveBufferB_After[WAVE_LEN];
u8 WaveBufferA_After[WAVE_LEN];
/*
 * Currsor buffer
 */
u8 CurrsorBuffer[WAVE_LEN] ;
/*
 * axi gpio key instance
 */
XGpio XGpioInstance;
XGpio XGpioInstRight;
XGpio XGpioInstLeft;
XGpio XGpioInstCurrsorChange;
/*
 * mio key instance
 */
XGpioPs XPsGPIOInstance;
/*
 * scu timer instance
 */
XScuTimer ScuTimerInstance;
/*
 * interrupt flag
 */
volatile int key_flag_ps ;
/*********   key_flag ���Ը�Ϊһ�β���FFT��������жϱ�־   ��ʱ��Ϊdata_flag,��Ӧ���жϼ�Ϊdata�ж� ***********/
volatile int key_flag_right = 0;
volatile int key_flag_left = 0;
volatile int key_flag_currsor_change = 0;				//Ĭ����0,��һ�����,1,�ڶ������
volatile int right_timer_flag;							//��ʱ���жϱ�־λ
volatile int left_timer_flag;

u32 currsor_blue_hor_x = 0;
u32 currsor_blue_hor_x_b = 0;
u32 currsor_red_hor_x = 0;
u32 currsor_red_hor_x_b = 0;
u8 *framePtr;

u32 wave_width = 1920;

//��16λ�ǻز��ź�
//��16λ�Ǵ����ź�
const int Sample_Point = 4096;
const int M = 12;
const float Volt_Per_Bit = 0.00244140625;
const int CODE_NUM = 16;
const int Main_Pulse_Length = 256;//���ղ��ε�ǰ256�������Ƿ���������

int flag = 0;
//float Echo_1[4096];//��Ҫ��ʾ
float Echo_1_COPY[4096];
float Trig_1[4096];//��Echo_1ǰ256�����ݺ���õ�
//float Echo_2[4096];//��Ҫ��ʾ
float Echo_2_COPY[4096];
float Trig_2[4096];//��Echo_2ǰ256�����ݺ���õ�
//float Echo_Proed1[4096];//��Ҫ��ʾ
//float Echo_Proed2[4096];//��Ҫ��ʾ
//float Echo_Proed[4096];//1��2֮����Ҫ��ʾ
float Echo_FFT_REAL[4096];
float Echo_FFT_IMG[4096];
float Trig1_FFT_REAL[4096];
float Trig1_FFT_IMG[4096];
float Trig2_FFT_REAL[4096];
float Trig2_FFT_IMG[4096];
float Data_Real[4096];
float Data_Img[4096];

int16_t Echo_Proed_2_INT[4096];

float yR[4096];
float yI[4096];

short CH0RxBufferSHORT [MAX_ADC_LEN_SHORT/sizeof(short)] __attribute__ ((aligned(64)));
short *shortptr = CH0RxBufferSHORT;
/*********CH0RxBuffer, Echo_1, Echo_2, �����洢����ǰ�������**********/
float Echo_Proed1 [MAX_ADC_LEN_FLOAT/sizeof(float)] __attribute__ ((aligned(64)));
float Echo_Proed2 [MAX_ADC_LEN_FLOAT/sizeof(float)] __attribute__ ((aligned(64)));
float Echo_Proed [MAX_ADC_LEN_FLOAT/sizeof(float)] __attribute__ ((aligned(64)));
float Echo_1[MAX_ADC_LEN_FLOAT/sizeof(float)] __attribute__ ((aligned(64)));
float Echo_2[MAX_ADC_LEN_FLOAT/sizeof(float)] __attribute__ ((aligned(64)));

IndexType PeakIndex1[1];
IndexType PeakIndex2[1];
IndexType PeakNum[1];

double distance;

/*
 * AXI GPIO defines
 */
#define LEFT_DEVICE_ID	    		XPAR_AXI_GPIO_1_DEVICE_ID
#define LEFT_INTR_ID 				XPAR_FABRIC_AXI_GPIO_1_IP2INTC_IRPT_INTR
#define RIGHT_DEVICE_ID	    		XPAR_AXI_GPIO_2_DEVICE_ID
#define RIGHT_INTR_ID 				XPAR_FABRIC_AXI_GPIO_2_IP2INTC_IRPT_INTR
#define CURRSOR_CHANGE_DEVICE_ID	XPAR_AXI_GPIO_3_DEVICE_ID
#define CURRSOR_CHANGE_INTR_ID 		XPAR_FABRIC_AXI_GPIO_3_IP2INTC_IRPT_INTR

#define KEY_CHANNEL 1									//PL����ʹ��AXI GPIOͨ��1
#define KEY_MASK    		XGPIO_IR_CH1_MASK 			//ͨ��1��λ����

//XGpio            			axi_gpio_inst;				//PL�� AXI GPIO ����ʵ��
/*
 * PSGPIO define
 */
#define MIO_ID          XPAR_PS7_GPIO_0_DEVICE_ID
#define KEY_INTR_ID     XPAR_XGPIOPS_0_INTR
#define PS_KEY_MIO      50
#define GPIO_INPUT      0
#define GPIO_OUTPUT     1
/*
 * timer define
 */
#define TIMER_DEVICE_ID     XPAR_XSCUTIMER_0_DEVICE_ID   	//��ʱ��ID
#define TIMER_IRPT_INTR     XPAR_SCUTIMER_INTR           	//��ʱ���ж�ID
//˽�ж�ʱ����ʱ��Ƶ�� = CPUʱ��Ƶ��/2 = 333MHz
//0.1s��˸һ��,0.2*1000_000_000/(1000/333) - 1 = 1FC1E1F
#define TIMER_LOAD_VALUE    0x1FC1E1F                    //��ʱ��װ��ֵ

#define INT_CFG0_OFFSET 0x00000C00
#define PI 3.1415926535

#define TR_DONE_INTC_ID			63
#define INTC_DEVICE_ID          XPAR_PS7_SCUGIC_0_DEVICE_ID
#define INT_TYPE_RISING_EDGE    0x03
#define INT_TYPE_HIGHLEVEL      0x01
#define INT_TYPE_MASK           0x03

static XScuGic INTCInst;
static void TR_DONE_intr_Handler(void *param);
static int IntcInitFunction(u16 DeviceId);
void FFT(float dataR[],float dataI[],float dataoR[],float dataoI[],int N,int M);/*����FFT���ټ���*/
void IFFT(float dataR[],float dataI[],float dataA[],int N,int M);/*����IFFT���ټ���*/
void FFTR(float dataR[],float real[],float img[],int N,int M);/*ʵ��FFT���ټ���*/
void IFFTR(float dataR[],float dataI[],float dataA[],int N,int M);/*ʵ��IFFT���ټ���*/
void MY_FFTR1(void);

int XAxiDma_Adc_Wave(u8 *frame, u32 stride, XScuGic *InstancePtr);
int Pl_Gpio_Initial(u16 DeviceId, u16 IntrID, XGpio *XGpioPtr, XScuGic *InstancePtr);
int Ps_Gpio_initial(u16 DeviceId, u16 IntrID, XGpioPs *XPsGpioPtr, XScuGic *InstancePtr);
void Left_Interrupt_Handler(void *CallBackRef);
void Right_Interrupt_Handler(void *CallBackRef);
void Change_Interrupt_Handler(void *CallBackRef);
void PsGpio_Interrupt_Handler(void *CallBackRef);
int Timer_Initial(u16 DeviceId, XScuTimer *ScuTimerPtr) ;
void timer_intr_init(XScuGic *intc_ptr,XScuTimer *timer_ptr);
void timer_intr_handler(void *CallBackRef);

void Float2Short(float *floatPtr, short *shortPtr,int multiple,int mag) {
	for(int i = 0;i < MAX_ADC_LEN_FLOAT/sizeof(float); i++) {
		shortPtr[i] = (short)((floatPtr[i * multiple] + mag) * 4096 / (2*mag));
	}
}

void Float2ShortforLast(float *floatPtr, short *shortPtr,int multiple,int mag) {
	for(int i = 0;i < MAX_ADC_LEN_FLOAT/sizeof(float); i++) {
		shortPtr[i] = (short)(fabs((floatPtr[i * multiple])) * 4096 / (mag));
	}
}

static void TR_DONE_intr_Handler(void *param)
{
	if((int)param==1){
		flag = 1;
	}
}

void IntcTypeSetup(XScuGic *InstancePtr, int intId, int intType)
{
    int mask;

    intType &= INT_TYPE_MASK;
    mask = XScuGic_DistReadReg(InstancePtr, INT_CFG0_OFFSET + (intId/16)*4);
    mask &= ~(INT_TYPE_MASK << (intId%16)*2);
    mask |= intType << ((intId%16)*2);
    XScuGic_DistWriteReg(InstancePtr, INT_CFG0_OFFSET + (intId/16)*4, mask);
}

void display_func(u8 *frame, u32 stride){
	currsor_blue_hor_x = 0;
	currsor_blue_hor_x_b = 0;
	currsor_red_hor_x = 0;
	currsor_red_hor_x_b = 0;
	/* Copy grid to Wave buffer */
	memcpy(WaveBuffer, GridBuffer_1920, WAVE_LEN_1920) ;
	memcpy(WaveBufferB_Before, GridBuffer, WAVE_LEN) ;
	memcpy(WaveBufferA_Before, GridBuffer, WAVE_LEN) ;
	memcpy(WaveBufferB_After, GridBuffer, WAVE_LEN) ;
	memcpy(WaveBufferA_After, GridBuffer, WAVE_LEN) ;
	/* draw wave on WaveBufferx */
	Float2ShortforLast(Echo_Proed,shortptr,2,100);
	//Float2Short(Echo_Proed,shortptr,2,100);
	draw_wave(1920, WAVE_HEIGHT, (void *)CH0RxBufferSHORT, WaveBuffer, UNSIGNEDSHORT, 12, CYAN, 16) ;
	Float2Short(Echo_1,shortptr,4,1);
	draw_wave(WAVE_WIDTH, WAVE_HEIGHT, (void *)CH0RxBufferSHORT, WaveBufferA_Before, UNSIGNEDSHORT, 12, BLUE, 16) ;
	Float2Short(Echo_2,shortptr,4,1);
	draw_wave(WAVE_WIDTH, WAVE_HEIGHT, (void *)CH0RxBufferSHORT, WaveBufferB_Before, UNSIGNEDSHORT, 12, GREEN, 16) ;
	Float2Short(Echo_Proed1,shortptr,4,50);
	draw_wave(WAVE_WIDTH, WAVE_HEIGHT, (void *)CH0RxBufferSHORT, WaveBufferA_After, UNSIGNEDSHORT, 12, MAGENTA, 16) ;
	Float2Short(Echo_Proed2,shortptr,4,50);
	draw_wave(WAVE_WIDTH, WAVE_HEIGHT, (void *)CH0RxBufferSHORT, WaveBufferB_After, UNSIGNEDSHORT, 12, DARK_YELLOW, 16) ;
	/* Copy Canvas to frame buffer */
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN, 128, frame, WaveBufferA_Before) ;
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN, 462, frame, WaveBufferB_Before) ;
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN + 960, 462, frame, WaveBufferB_After) ;////////
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN + 960, 128, frame, WaveBufferA_After) ;
	frame_copy(1920, WAVE_HEIGHT, stride, WAVE_START_COLUMN, WAVE_START_ROW, frame, WaveBuffer) ;
}

int IntcInitFunction(u16 DeviceId)
{
    XScuGic_Config *IntcConfig;
    Xil_ExceptionInit();
    int status;

    // Interrupt controller initialization
    IntcConfig = XScuGic_LookupConfig(DeviceId);
    status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Call to interrupt setup
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
                                 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
                                 &INTCInst);
    Xil_ExceptionEnable();

    // Connect SW1~SW3 interrupt to handler
    status = XScuGic_Connect(&INTCInst,
    						 TR_DONE_INTC_ID,
                             (Xil_ExceptionHandler)TR_DONE_intr_Handler,
                             (void *)1);
    if(status != XST_SUCCESS) return XST_FAILURE;

    // Set interrupt type of SW1~SW2 to rising edge
    IntcTypeSetup(&INTCInst, TR_DONE_INTC_ID, INT_TYPE_RISING_EDGE);

    // Enable SW1~SW2 interrupts in the controller
    XScuGic_Enable(&INTCInst, TR_DONE_INTC_ID);

    return XST_SUCCESS;
}

void DemoPrintTest(u8 *frame, u32 width, u32 height, u32 stride, int pattern)
{
	u32 xcoi, ycoi;
	u32 iPixelAddr = 0;
	u8 wRed, wBlue, wGreen;


	for(ycoi = 0; ycoi < height; ycoi++)
	{

		/*
		 * Just draw white in the last partial interval (when width is not divisible by 7)
		 */

		for(xcoi = 0; xcoi < (width*BYTES_PIXEL); xcoi+=BYTES_PIXEL)
		{
			wRed = 255;
			wGreen = 255;
			wBlue = 255;

			frame[xcoi+iPixelAddr + 0] = wBlue;
			frame[xcoi+iPixelAddr + 1] = wGreen;
			frame[xcoi+iPixelAddr + 2] = wRed;
			/*
			 * This pattern is printed one vertical line at a time, so the address must be incremented
			 * by the stride instead of just 1.
			 */
		}
		iPixelAddr += stride;

	}

	/*
	 * Flush the framebuffer memory range to ensure changes are written to the
	 * actual memory, and therefore accessible by the VDMA.
	 */
	Xil_DCacheFlushRange((unsigned int) frame, DEMO_MAX_FRAME);
}

int main()
{
	int Status;
	XAxiVdma_Config *vdmaConfig;
	long i=0;
    Xil_DCacheDisable();
    /*
	 * Initialize an array of pointers to the 3 frame buffers
	 */
	for (i = 0; i < DISPLAY_NUM_FRAMES; i++)
	{
		pFrames[i] = frameBuf[i];
	}
	/*
	 * Initialize VDMA driver
	 */
	vdmaConfig = XAxiVdma_LookupConfig(VGA_VDMA_ID);
	if (!vdmaConfig)
	{
		xil_printf("No video DMA found for ID %d\r\n", VGA_VDMA_ID);
	}
	Status = XAxiVdma_CfgInitialize(&vdma, vdmaConfig, vdmaConfig->BaseAddress);
	if (Status != XST_SUCCESS)
	{
		xil_printf("VDMA Configuration Initialization failed %d\r\n", Status);

	}
	/*
	 * Initialize the Display controller and start it
	 */
	Status = DisplayInitialize(&dispCtrl, &vdma, DISP_VTC_ID, DYNCLK_BASEADDR, pFrames, DEMO_STRIDE);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Display Ctrl initialization failed during demo initialization%d\r\n", Status);

	}
	Status = DisplayStart(&dispCtrl);
	if (Status != XST_SUCCESS)
	{
		xil_printf("Couldn't start display during demo initialization%d\r\n", Status);

	}
	DemoPrintTest(dispCtrl.framePtr[dispCtrl.curFrame], dispCtrl.vMode.width, dispCtrl.vMode.height, dispCtrl.stride, DEMO_PATTERN_3);
    print("This is EMAT Group's Project!\n\r");
    IntcInitFunction(INTC_DEVICE_ID);
    XAxiDma_Adc_Wave(dispCtrl.framePtr[dispCtrl.curFrame],  dispCtrl.stride, &INTCInst);
    while(1){
    	if(flag==1){
    		print("Process begin!\n\r");
    		MY_FFTR1();
    		display_func(dispCtrl.framePtr[dispCtrl.curFrame],dispCtrl.stride);
    		show_float(1162, 0, (float)distance*(3.09e-3), framePtr, 4, 8, 0, 0, 0);
//    		for(i=0;i<Sample_Point;i++){
//    			printf("%f , %f , %f , %f , %f\n",Echo_1[i],Echo_2[i],Echo_Proed1[i],Echo_Proed2[i],Echo_Proed[i]);
//    		}
    		print("Process end!\n\r");
    		flag = 0;
    	}else if(right_timer_flag==1){
    		right_timer_flag=0;
			if(key_flag_currsor_change) {
				if(currsor_blue_hor_x == 0 && currsor_blue_hor_x_b == 0) {
					draw_currsor(currsor_blue_hor_x, currsor_blue_hor_x_b, wave_width, WAVE_HEIGHT, WaveBuffer,255,0,0);
					frame_copy(wave_width, WAVE_HEIGHT, dispCtrl.stride, WAVE_START_COLUMN, WAVE_START_ROW, dispCtrl.framePtr[dispCtrl.curFrame], WaveBuffer) ;
					show_num(160, 0, currsor_blue_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 255, 0, 0);
					currsor_blue_hor_x+=5;

				}
				else if(currsor_blue_hor_x == wave_width) {
					show_num(160, 0, currsor_blue_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 255, 0, 0);
					currsor_blue_hor_x = currsor_blue_hor_x;
					currsor_blue_hor_x_b = currsor_blue_hor_x_b;
				}
				else {
					if(currsor_blue_hor_x < currsor_blue_hor_x_b) {
						currsor_blue_hor_x = currsor_blue_hor_x_b + 5;
					}
					draw_currsor(currsor_blue_hor_x, currsor_blue_hor_x_b, wave_width, WAVE_HEIGHT, WaveBuffer,254,0,0);
					frame_copy(wave_width, WAVE_HEIGHT, dispCtrl.stride, WAVE_START_COLUMN, WAVE_START_ROW, dispCtrl.framePtr[dispCtrl.curFrame], WaveBuffer) ;
					currsor_blue_hor_x_b = currsor_blue_hor_x;
					show_num(160, 0, currsor_blue_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 255, 0, 0);
					currsor_blue_hor_x+=5;
				}
			}
			else {
				if(currsor_red_hor_x == 0 && currsor_red_hor_x_b == 0) {
					draw_currsor(0, 0, wave_width, WAVE_HEIGHT, WaveBuffer,0,0,254);
					frame_copy(wave_width, WAVE_HEIGHT, dispCtrl.stride, WAVE_START_COLUMN, WAVE_START_ROW, dispCtrl.framePtr[dispCtrl.curFrame], WaveBuffer) ;
					show_num(570, 0, currsor_red_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 0, 0, 255);
					currsor_red_hor_x+=5;
				}
				else if(currsor_red_hor_x > wave_width) {
					currsor_red_hor_x = currsor_red_hor_x - 5;
					currsor_red_hor_x_b = currsor_red_hor_x_b - 5;
					show_num(570, 0, currsor_red_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 0, 0, 255);
				}
				else{
					if(currsor_red_hor_x < currsor_red_hor_x_b) {
						currsor_red_hor_x = currsor_red_hor_x_b + 5;
					}
					draw_currsor(currsor_red_hor_x, currsor_red_hor_x_b, wave_width, WAVE_HEIGHT, WaveBuffer,0,0,254);
					frame_copy(wave_width, WAVE_HEIGHT, dispCtrl.stride, WAVE_START_COLUMN, WAVE_START_ROW, dispCtrl.framePtr[dispCtrl.curFrame], WaveBuffer) ;
					currsor_red_hor_x_b = currsor_red_hor_x;
					show_num(570, 0, currsor_red_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 0, 0, 255);
					currsor_red_hor_x+=5;
				}
			}
			XScuTimer_Start(&ScuTimerInstance);
    	}else if(left_timer_flag==1){
    		left_timer_flag = 0;
			if(key_flag_currsor_change) {
				if((currsor_blue_hor_x == 0 || currsor_blue_hor_x == 5) && (currsor_blue_hor_x_b == 0)){
					currsor_blue_hor_x = 0;
					show_string(160, 0, "      ", dispCtrl.framePtr[dispCtrl.curFrame], 255, 0, 0);
				}
				else{
					if(currsor_blue_hor_x > currsor_blue_hor_x_b) {
						currsor_blue_hor_x = currsor_blue_hor_x_b - 5;
					}
					draw_currsor(currsor_blue_hor_x, currsor_blue_hor_x_b, wave_width, WAVE_HEIGHT, WaveBuffer,254,0,0);
					frame_copy(wave_width, WAVE_HEIGHT, dispCtrl.stride, WAVE_START_COLUMN, WAVE_START_ROW, dispCtrl.framePtr[dispCtrl.curFrame], WaveBuffer) ;
					show_num(160, 0, currsor_blue_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 255, 0, 0);
					if(currsor_blue_hor_x == 0 && currsor_blue_hor_x_b == 5) {
						currsor_blue_hor_x_b = 0;
					}
					else {
						currsor_blue_hor_x_b = currsor_blue_hor_x;
						currsor_blue_hor_x-=5;
					}
				}
			}
			else {
				if((currsor_red_hor_x == 0 || currsor_red_hor_x == 5) && currsor_red_hor_x_b == 0) {
					currsor_red_hor_x = 0;
					show_string(570, 0, "      ", dispCtrl.framePtr[dispCtrl.curFrame], 0, 0, 255);
				}
				else{
					if(currsor_red_hor_x > currsor_red_hor_x_b) {
						currsor_red_hor_x = currsor_red_hor_x_b - 5;
					}
					draw_currsor(currsor_red_hor_x, currsor_red_hor_x_b, wave_width, WAVE_HEIGHT, WaveBuffer,0,0,254);
					frame_copy(wave_width, WAVE_HEIGHT, dispCtrl.stride, WAVE_START_COLUMN, WAVE_START_ROW, dispCtrl.framePtr[dispCtrl.curFrame], WaveBuffer) ;
					show_num(570, 0, currsor_red_hor_x, dispCtrl.framePtr[dispCtrl.curFrame], 0, 0, 255);
					if(currsor_red_hor_x == 0 && currsor_red_hor_x_b == 5) {
						 currsor_red_hor_x_b = 0;
					}
					else {
						currsor_red_hor_x_b = currsor_red_hor_x;
						currsor_red_hor_x-=5;
					}

				}
			}
			XScuTimer_Start(&ScuTimerInstance);
    	}
    }
    return 0;
}

//����FFT
void FFT(float dataR[],float dataI[],float dataoR[],float dataoI[],int N,int M)
{

	int i,j,k,r;
	int p,L,B;
	unsigned int I,J,K,F0,F1,m,n;
	float Tr,Ti,temp;
	//�������е���
/*
J=N/2;
for(I = 1; I < N - 2; I++)
{
	//����Ԫ�ػ���
	if(I<J)
	{
		temp = dataR[I];
		dataR[I] = dataR[J];
		dataR[J] = temp;
	}
	//��ȡ��һ��������
	K=N/2;//K��N��Ȩֵ�����������λȨֵ��ʼ�жϡ�
	while(1)
	{
		//������ѭ���жϸ�λ�Ƿ�Ϊ1
 		if(J< K)
		{
			J=J+K;//�жϽ��Ϊ��λΪ0�����ϸ�λȨֵ��0���1��
			break;//ѭ������
 		}
		else
		{
			J=J-K;//�жϽ��Ϊ��λΪ1����ȥ��λȨֵ��1���0��
			K=K/2;//�õ���һλ��Ȩֵ��������һλ0��1���жϡ�
		 }
	}
}*/

//����
	for(I=0;I< N;I++)   //���ݹ����ģ���Ҫ������Ԫ��ִ����䵹��
	{
		/*��ȡ�±�I�ķ���J����ֵ*/
		J=0;
		for(k=0;k<(M/2+0.5);k++)     //k��ʾ����
	     {
	         //*�������*/
	         m=1;//m�����λΪ1�Ķ�������
			 n=(unsigned int)pow(2,M-1);//n�ǵ�MλΪ1�Ķ�������
			 m <<= k; //��m����kλ
	         n >>= k; //��n����kλ
	         F0=I & n;//I��n��λ����ȡ��ǰ�벿�ֵ�kλ
			 F1=I & m;//I��m��λ����ȡ��F0��Ӧ�ĺ�벿�ֵĵ�λ
	         if(F0) J=J | m; //J��m��λ��ʹF0��Ӧ��λΪ1
	         if(F1) J=J | n; //J��n��λ��ʹF1��Ӧ��λΪ1
	     }
	 		//printf("IΪ��");printBin(I,M);printf(" %d\n",I);
	 		//printf("JΪ��");printBin(J,M);printf(" %d\n\n",J);

	 	 if(I<J)
			{
				temp = dataR[I];
				dataR[I] = dataR[J];
				dataR[J] = temp;
				//�����������ֽ�����
				temp = dataI[I];
				dataI[I] = dataI[J];
				dataI[J] = temp;
			}
	}

//����FFT
	/*	Tr = Xr(J+B)cos(2.0*PI*p/N) + Xi(J+B)sin(2.0*PI*p/N)
	    Ti = Xi(J+B)cos(2.0*PI*p/N) - Xr(J+B)sin(2.0*PI*p/N)
		Ar[J] = Xr(J) + Tr
	    Ai[J] = Xi(J) + Ti
	    Ar[J+B] = Xr(J) - Tr
	    Ai[J+B] = Xi(J) - Ti
	   (���� XrΪ��һ����Ar, XiΪ��һ����Ai)*/
	for(L=1; L<=M;L++)	//FFT���μ���L��1--M
	{
		/*��L��������:
		Ȼ����ڵ�L�������������������ᵽ�����������������Ŀ���ڼ��B:�ж����ֵ��������Ҫ��Ҫѭ�����ٴ�;
		����ѭ���Ĳ�ͬ����תָ��PҲ��ͬ��P������Ϊk=2^(M-L)*/
		//�ȼ���һ�¼�� B = 2^(L-1);
		B = 1;
		B = (int)pow(2,L-1);
		for(j=0;j<=B-1;j++)
		//j = 0,1,2,...,2^(L-1) - 1
		{	/*ͬ�ֵ�������*/
			//�ȼ�������k=2^(M-L)
			k=1;
			k = (int)pow(2,M-L);
			//������תָ��p������Ϊkʱ����P=j*k
			p=1;
			p=j*k;
			/*�������������������ǿ���֪��ͬ�ֵ�������Ĵ����պ�Ϊ����k=2^(M-L)��
			ͬ�ֵ��ε����������Ϊ��������Ĵ���*/
			for(i=0; i<=k-1;i++)
				{
					//�����±궨Ϊr
					r=1;
					r=j+2*B*i;
					Tr=dataR[r+B]*cos(2.0*PI*p/N) + dataI[r+B]*sin(2.0*PI*p/N);
					Ti=dataI[r+B]*cos(2.0*PI*p/N) - dataR[r+B]*sin(2.0*PI*p/N);
					dataR[r+B]=dataR[r]-Tr;
					dataI[r+B]=dataI[r]-Ti;
					dataR[r]=dataR[r]+Tr;
					dataI[r]=dataI[r]+Ti;
				}
			}
	}
	//�����ֵ
	for ( i=0;i<N;i++ )
	{
		dataoR[i] = dataR[i];
		dataoI[i] = dataI[i];
	 	//dataA[i]=sqrt(dataR[i]*dataR[i]+dataI[i]*dataI[i]);
	}
}

//ʵ��FFT
void FFTR(float dataR[],float real[],float img[],int N,int M)
{
	int i,k;
	//int j,r,p,B;
	//float Tr,Ti;
	//float yR[N/2],yI[N/2],yA[N/2];
	//float x1R[N/2],x2R[N/2],x1I[N/2],x2I[N/2],xR[N],xI[N];
	for(i=0;i<N/2;i++)
	{
		yR[i]=dataR[2*i];
		yI[i]=dataR[2*i+1];
	}
	FFT(yR,yI,real,img,N/2,M-1);
	//��X1(k)��X2(k)
//	for(k=0;k<N/2;k++)
//	{
//		if(k==0)
//		{
//			x1R[k]=yR[k];
//			x1I[k]=yI[k];
//			x2R[k]=yI[k];
//			x2I[k]=-yR[k];
//		}
//		else
//		{
//			x1R[k]=(yR[k]+yR[N/2-k])/2;
//			x1I[k]=(yI[k]-yI[N/2-k])/2;
//			x2R[k]=(yI[k]+yI[N/2-k])/2;
//			x2I[k]=(yR[N/2-k]-yR[k])/2;
//		}
//	}

	//��M���ĵ�������
//	B=1;
//	B = (int)pow(2,M-1);
//	for(j=0;j<=N/2-1;j++)
//	//j = 0,1,2,...,2^(L-1) - 1
//	{
//		p=1;
//		p=j;
//		//�����±궨Ϊr
//		r=1;
//		r=j;
//		Tr=x2R[r]*cos(2.0*PI*p/N) + x2I[r]*sin(2.0*PI*p/N);
//		Ti=x2I[r]*cos(2.0*PI*p/N) - x2R[r]*sin(2.0*PI*p/N);
//		xR[r]=x1R[r]+Tr;
//		xI[r]=x1I[r]+Ti;
//
//		if(r==0)
//		{
//			xR[N/2]=x1R[0]-x2R[0];
//			xI[N/2]=x1I[0]-x2I[0];
//		}
//		else
//		{
//			xR[N-r]=xR[r];
//			xI[N-r]=-xI[r];
//		}
//	}
//	for(i=0;i<N;i++){
//		real[i]=xR[i];
//		img[i]=xI[i];
//	}
}

//����IFFT
void IFFT(float dataR[],float dataI[],float dataA[],int N,int M)
{
	int i,j,k,r;
	int p,L,B;
	unsigned int I,J,K,F0,F1,m,n;
	float Tr,Ti,temp;
	//�������е���
	//����
	for(I=0;I< N;I++)   //���ݹ����ģ���Ҫ������Ԫ��ִ����䵹��
	{
		/*��ȡ�±�I�ķ���J����ֵ*/
		J=0;
		for(k=0;k<(M/2+0.5);k++)     //k��ʾ����
	     {
	         //*�������*/
	         m=1;//m�����λΪ1�Ķ�������
			 n=(unsigned int)pow(2,M-1);//n�ǵ�MλΪ1�Ķ�������
			 m <<= k; //��m����kλ
	         n >>= k; //��n����kλ
	         F0=I & n;//I��n��λ����ȡ��ǰ�벿�ֵ�kλ
			 F1=I & m;//I��m��λ����ȡ��F0��Ӧ�ĺ�벿�ֵĵ�λ
	         if(F0) J=J | m; //J��m��λ��ʹF0��Ӧ��λΪ1
	         if(F1) J=J | n; //J��n��λ��ʹF1��Ӧ��λΪ1
	     }
	 		//printf("IΪ��");printBin(I,M);printf(" %d\n",I);
	 		//printf("JΪ��");printBin(J,M);printf(" %d\n\n",J);

	 	 if(I<J)
			{
			temp = dataR[I];
			dataR[I] = dataR[J];
			dataR[J] = temp;
			//�����������ֽ�����
			temp = dataI[I];
			dataI[I] = dataI[J];
			dataI[J] = temp;
			}
	}


	//����IFFT
	/*	Tr = Xr(J+B)cos(2.0*PI*p/N) - Xi(J+B)sin(2.0*PI*p/N)
	    Ti = Xi(J+B)cos(2.0*PI*p/N) + Xr(J+B)sin(2.0*PI*p/N)
		Ar[J] = Xr(J) + Tr
	    Ai[J] = Xi(J) + Ti
	    Ar[J+B] = Xr(J) - Tr
	    Ai[J+B] = Xi(J) - Ti
	   (���� XrΪ��һ����Ar, XiΪ��һ����Ai)*/
	for(L=1; L<=M;L++)	//FFT���μ���L��1--M
	{
		/*��L��������:
		Ȼ����ڵ�L�������������������ᵽ�����������������Ŀ���ڼ��B:�ж����ֵ��������Ҫ��Ҫѭ�����ٴ�;
		����ѭ���Ĳ�ͬ����תָ��PҲ��ͬ��P������Ϊk=2^(M-L)*/
		//�ȼ���һ�¼�� B = 2^(L-1);
		B = 1;
		B = (int)pow(2,L-1);
		for(j=0;j<=B-1;j++)
		//j = 0,1,2,...,2^(L-1) - 1
		{	/*ͬ�ֵ�������*/
			//�ȼ�������k=2^(M-L)
			k=1;
			k = (int)pow(2,M-L);
			//������תָ��p������Ϊkʱ����P=j*k
			p=1;
			p=j*k;
			/*�������������������ǿ���֪��ͬ�ֵ�������Ĵ����պ�Ϊ����k=2^(M-L)��
			ͬ�ֵ��ε����������Ϊ��������Ĵ���*/
			for(i=0; i<=k-1;i++)
				{
					//�����±궨Ϊr
					r=1;
					r=j+2*B*i;
					Tr=dataR[r+B]*cos(2.0*PI*p/N) - dataI[r+B]*sin(2.0*PI*p/N);
					Ti=dataI[r+B]*cos(2.0*PI*p/N) + dataR[r+B]*sin(2.0*PI*p/N);
					dataR[r+B]=dataR[r]-Tr;dataR[r+B]=dataR[r+B]/2;
					dataI[r+B]=dataI[r]-Ti;dataI[r+B]=dataI[r+B]/2;
					dataR[r]=dataR[r]+Tr;dataR[r]=dataR[r]/2;
					dataI[r]=dataI[r]+Ti;dataI[r]=dataI[r]/2;
				}
			}
	}

	//�������Ƶ����,dataA[i] = sqrt(dataR[i]*dataR[i]+dataI[i]*dataI[i])
	for ( i=0;i<N;i++ )
	{
		if(dataR[i]>=0)
			dataA[i]=sqrt(dataR[i]*dataR[i]+dataI[i]*dataI[i]);
		else
			dataA[i]=-sqrt(dataR[i]*dataR[i]+dataI[i]*dataI[i]);
	}
}

//ʵ��IFFT
void IFFTR(float dataR[],float dataI[],float dataA[],int N,int M)
{
	int i,k;
	int j,r,p,B;
	float Tr,Ti;
	float yR[N/2],yI[N/2],yA[N/2];
	float x1R[N/2],x2R[N/2],x1I[N/2],x2I[N/2],xR[N],xI[N];
	for(i=0;i<N/2;i++)
	{
		yR[i]=dataR[2*i];//printf("%f\n",yR[i]);
		yI[i]=dataR[2*i+1];//printf("%f\n",yI[i]);

	}
	IFFT(yR,yI,yA,N/2,M-1);
	for(k=0;k<N/2;k++)
	{
		printf("%d,%f\n",N/2,yR[k]);
		//printf("%f\n",yI[N/2]);
	}
	//��X1(k)��X2(k)
	for(k=0;k<N/2;k++)
	{
		if(k==0)
		{
			x1R[k]=yR[k];
			x1I[k]=yI[k];
			x2R[k]=yI[k];
			x2I[k]=-yR[k];
		}
		else
		{
			x1R[k]=(yR[k]+yR[N/2-k])/2;
			x1I[k]=(yI[k]-yI[N/2-k])/2;
			x2R[k]=(yI[k]+yI[N/2-k])/2;
			x2I[k]=(yR[N/2-k]-yR[k])/2;
		}

	}

	//��M���ĵ�������
	B = 1;
	B = (int)pow(2,M-1);
	for(j=0;j<=B-1;j++)
	//j = 0,1,2,...,2^(L-1) - 1
	{
		p=1;
		p=j;
		//�����±궨Ϊr
		r=1;
		r=j;
		Tr=x2R[r]*cos(2.0*PI*p/N) - x2I[r]*sin(2.0*PI*p/N);
		Ti=x2I[r]*cos(2.0*PI*p/N) + x2R[r]*sin(2.0*PI*p/N);
		dataR[r]=x1R[r]+Tr;dataR[r]=dataR[r]/2;
		dataI[r]=x1I[r]+Ti;dataI[r]=dataI[r]/2;

		if(r==0)
		{
			dataR[N/2]=x1R[0]-x2R[0];dataR[N/2]=dataR[N/2]/2;
			dataI[N/2]=x1I[0]-x2I[0];dataI[N/2]=dataI[N/2]/2;
		}
		else
		{
			dataR[N-r]=dataR[r];
			dataI[N-r]=-dataI[r];
		}
	}


	for ( i=0;i<N;i++ )
	{
		//dataA[i]=sqrt(xR[i]*xR[i]+xI[i]*xI[i]);
	 	dataA[i]=sqrt(dataR[i]*dataR[i]+dataI[i]*dataI[i]);
	}
}

void MY_FFTR1(void){
	int temp;
	int i;

	//ȡֵ
	for(i=4;i<16384;i=i+4){
		temp = (int)Xil_In16(0x10000000+i+2);
		temp = temp&0x0FFF;
		Echo_1[i/4] = temp*Volt_Per_Bit-4.8;
		Echo_1_COPY[i/4] = Echo_1[i/4];
	}
	Echo_1[0] = Echo_1[4095];
	Echo_1_COPY[0] = Echo_1[0];
	for(i=16388;i<32768;i=i+4){
		temp = (int)Xil_In16(0x10000000+i+2);
		temp = temp&0x0FFF;
		Echo_2[(i-16384)/4] = temp*Volt_Per_Bit-4.8;
		Echo_2_COPY[(i-16384)/4] = Echo_2[(i-16384)/4];
	}
	Echo_2[0] = Echo_2[4095];
	Echo_2_COPY[0] = Echo_2[0];
	//ȡ�����
	for(i=0;i<4096;i++){
		if(i<200){
			Trig_1[i] = Echo_1[i];
			Trig_2[i] = Echo_2[i];
		}else{
			Trig_1[i]=0;
			Trig_2[i]=0;
		}
	}

	//A��ز�ѹ��
	FFT(Echo_1_COPY,yI,Echo_FFT_REAL,Echo_FFT_IMG,Sample_Point,M);
	for(i=0;i<4096;i++){
		yI[i]=0;
	}
	//A�������ѹ��
	FFT(Trig_1,yI,Trig1_FFT_REAL,Trig1_FFT_IMG,Sample_Point,M);
	for(i=0;i<4096;i++){
		yI[i]=0;
	}
	//A��ز��������FFT������IFFT
	for(i=0;i<Sample_Point;i++){
		Trig1_FFT_IMG[i]=-Trig1_FFT_IMG[i];
		Data_Real[i]=(Echo_FFT_REAL[i]*Trig1_FFT_REAL[i]-Trig1_FFT_IMG[i]*Echo_FFT_IMG[i]);
		Data_Img[i] =(Echo_FFT_REAL[i]*Trig1_FFT_IMG[i]+Trig1_FFT_REAL[i]*Echo_FFT_IMG[i]);
	}
	IFFT(Data_Real,Data_Img,Echo_Proed1,Sample_Point,M);

	//B��ز�ѹ��
	FFT(Echo_2_COPY,yI,Echo_FFT_REAL,Echo_FFT_IMG,Sample_Point,M);
	for(i=0;i<4096;i++){
		yI[i]=0;
	}
	//B�������ѹ��
	FFT(Trig_2,yI,Trig2_FFT_REAL,Trig2_FFT_IMG,Sample_Point,M);
	for(i=0;i<4096;i++){
		yI[i]=0;
	}
	//B��ز��������FFT������IFFT
	for(i=0;i<Sample_Point;i++){
		Trig2_FFT_IMG[i]=-Trig2_FFT_IMG[i];
		Data_Real[i]=(Echo_FFT_REAL[i]*Trig2_FFT_REAL[i]-Trig2_FFT_IMG[i]*Echo_FFT_IMG[i]);
		Data_Img[i] =(Echo_FFT_REAL[i]*Trig2_FFT_IMG[i]+Trig2_FFT_REAL[i]*Echo_FFT_IMG[i]);
	}
	IFFT(Data_Real,Data_Img,Echo_Proed2,Sample_Point,M);

	//�Ӻͣ���ʽת��
	for(i=0;i<Sample_Point;i++){
		Echo_Proed[i]=Echo_Proed1[i]+Echo_Proed2[i];
		if(Echo_Proed[i]>100){
			Echo_Proed_2_INT[i]=2000;
		}else if(Echo_Proed[i]<-100){
			Echo_Proed_2_INT[i]=0;
		}else{
			Echo_Proed_2_INT[i] = (int)Echo_Proed[i]*10+1000;
		}
	}
	//�Զ��������
	distance = MeasureEchoPosition_SimilarEhco(Echo_Proed_2_INT,0,2048,PeakIndex1,PeakIndex2,PeakNum);
}

int XAxiDma_Adc_Wave(u8 *frame, u32 stride, XScuGic *InstancePtr)
{

	int Status;
	framePtr = frame;
	//initial the mio
	Ps_Gpio_initial(MIO_ID, KEY_INTR_ID, &XPsGPIOInstance, InstancePtr);
	Status = XScuGic_Connect(InstancePtr, KEY_INTR_ID,
			(Xil_ExceptionHandler)PsGpio_Interrupt_Handler,
			(void *)&XPsGPIOInstance) ;
	if (Status != XST_SUCCESS) {
		return XST_FAILURE ;
	}

	//initial the axi gpio key right
	Status = Pl_Gpio_Initial(RIGHT_DEVICE_ID, RIGHT_INTR_ID, &XGpioInstRight, InstancePtr) ;
	if(Status != XST_SUCCESS) {
		xil_printf("key right initialization failed %d\r\n", Status);
		return XST_FAILURE;
	}
	//connect and enable interrupt
	Status = XScuGic_Connect(InstancePtr, RIGHT_INTR_ID, Right_Interrupt_Handler, &XGpioInstRight);
	if (Status != XST_SUCCESS) {
		return Status;
	}
	//enable AXI GPIO
	XScuGic_Enable(InstancePtr, RIGHT_INTR_ID);

	//initial the axi gpio key left
	Status = Pl_Gpio_Initial(LEFT_DEVICE_ID, LEFT_INTR_ID, &XGpioInstLeft, InstancePtr) ;
	if(Status != XST_SUCCESS) {
		xil_printf("key left initialization failed %d\r\n", Status);
		return XST_FAILURE;
	}
	//connect and enable interrupt
	Status = XScuGic_Connect(InstancePtr, LEFT_INTR_ID, Left_Interrupt_Handler, &XGpioInstLeft);
	if (Status != XST_SUCCESS) {
		return Status;
	}
	//enable AXI GPIO
	XScuGic_Enable(InstancePtr, LEFT_INTR_ID);

	//initial the axi gpio key currsor change
	Status = Pl_Gpio_Initial(CURRSOR_CHANGE_DEVICE_ID, CURRSOR_CHANGE_INTR_ID, &XGpioInstCurrsorChange, InstancePtr) ;
	if(Status != XST_SUCCESS) {
		xil_printf("key currsor change initialization failed %d\r\n", Status);
		return XST_FAILURE;
	}
	//connect and enable interrupt
	Status = XScuGic_Connect(InstancePtr, CURRSOR_CHANGE_INTR_ID, Change_Interrupt_Handler, &XGpioInstCurrsorChange);
	if (Status != XST_SUCCESS) {
		return Status;
	}
	//enable AXI GPIO
	XScuGic_Enable(InstancePtr, CURRSOR_CHANGE_INTR_ID);
	//initial timer
	Status = Timer_Initial(TIMER_DEVICE_ID, &ScuTimerInstance) ;
	timer_intr_init(InstancePtr,&ScuTimerInstance);

	/* Grid Overlay */
	draw_grid(WAVE_WIDTH, WAVE_HEIGHT,GridBuffer) ;
	draw_grid(1920, WAVE_HEIGHT,GridBuffer_1920) ;
	/* menu */
	show_string(0, 0, "Cur1:", frame, 255, 0, 0);
	show_string(410, 0, "Cur2:", frame, 0, 0, 255);
	show_string(820, 0, "Thickness:", frame, 0, 0, 0);
	//show_string(1400, 0, "SNR:", frame, 0, 0, 0);
	show_string(0, 398, "B_Before", frame, 0, 0, 0);
	show_string(0, 64, "A_Before", frame, 0, 0, 0);
	show_string(920, 398, "B_After", frame, 0, 0, 0);
	show_string(920, 64, "A_After", frame, 0, 0, 0);
	show_string(0, 732, "After", frame, 0, 0, 0);
	show_string(400,732,"Speed of sound: 3090 m/s", frame, 0, 0, 0);
	//show_float(1528, 0, 0.1234, frame, 5, 8, 0, 0, 0);
	/* Copy grid to Wave buffer */
	memcpy(WaveBuffer, GridBuffer_1920, WAVE_LEN_1920) ;
	memcpy(WaveBufferB_Before, GridBuffer, WAVE_LEN) ;
	memcpy(WaveBufferA_Before, GridBuffer, WAVE_LEN) ;
	memcpy(WaveBufferB_After, GridBuffer, WAVE_LEN) ;
	memcpy(WaveBufferA_After, GridBuffer, WAVE_LEN) ;
	/* Copy Canvas to frame buffer */
//	frame_copy(wave_width, WAVE_HEIGHT, stride, WAVE_START_COLUMN, 462, frame, WaveBufferB_Before) ;////////
//	frame_copy(wave_width, WAVE_HEIGHT, stride, WAVE_START_COLUMN, 128, frame, WaveBufferA_Before) ;
//	frame_copy(wave_width, WAVE_HEIGHT, stride, WAVE_START_COLUMN, WAVE_START_ROW, frame, WaveBuffer) ;
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN, 462, frame, WaveBufferB_Before) ;////////
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN, 128, frame, WaveBufferA_Before) ;
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN + 960, 462, frame, WaveBufferB_After) ;////////
	frame_copy(WAVE_WIDTH, WAVE_HEIGHT, stride, WAVE_START_COLUMN + 960, 128, frame, WaveBufferA_After) ;
	frame_copy(1920, WAVE_HEIGHT, stride, WAVE_START_COLUMN , WAVE_START_ROW, frame, WaveBuffer) ;
	return XST_SUCCESS ;
}

/*
 *Initial AXi GPIO and connect interrupt to handler,
 */
int Pl_Gpio_Initial(u16 DeviceId, u16 IntrID, XGpio *XGpioPtr, XScuGic *InstancePtr)
{
	int Status;
	/* Initialize the gpio device. */
	Status = XGpio_Initialize(XGpioPtr, DeviceId);
	if (Status != XST_SUCCESS) {
		xil_printf("Initialization failed %d\r\n", Status);
		return XST_FAILURE;
	}
	//configure the axi gpio as input
	XGpio_SetDataDirection(XGpioPtr, KEY_CHANNEL, 1); 	//����PL AXI GPIOͨ��1Ϊ����
	XGpio_InterruptEnable(XGpioPtr, KEY_MASK);			//ʹ��ͨ��1�ж�
	XGpio_InterruptGlobalEnable(XGpioPtr);				//ʹ��AXI GPIOȫ���ж�
	//
	//�����ж����ȼ��ʹ�������(�ߵ�ƽ����)
	XScuGic_SetPriorityTriggerType(InstancePtr, IntrID, 0xA0, 0x1);
	//�����ж�ID���жϴ�����
	return XST_SUCCESS ;
}
int Ps_Gpio_initial(u16 DeviceId, u16 IntrID, XGpioPs *XPsGpioPtr, XScuGic *InstancePtr) {

	XGpioPs_Config *GpioConfig ;
	int Status ;
	/*
	 * Initialize the gpio.
	 */
	GpioConfig = XGpioPs_LookupConfig(DeviceId) ;
	Status = XGpioPs_CfgInitialize(XPsGpioPtr, GpioConfig, GpioConfig->BaseAddr) ;
	if (Status != XST_SUCCESS)
	{
		return XST_FAILURE ;
	}
	/*
	 * Set the direction for the pin to be input.
	 * Set interrupt type as rising edge and enable gpio interrupt
	 */
	XGpioPs_SetDirectionPin(XPsGpioPtr, PS_KEY_MIO, GPIO_INPUT) ;
	XGpioPs_SetIntrTypePin(XPsGpioPtr, PS_KEY_MIO, XGPIOPS_IRQ_TYPE_EDGE_RISING) ;
	XGpioPs_IntrEnablePin(XPsGpioPtr, PS_KEY_MIO) ;
	/*
	 * set priority and trigger type
	 */
	XScuGic_SetPriorityTriggerType(InstancePtr, KEY_INTR_ID,
			0xA0, 0x3);
	XScuGic_Enable(InstancePtr, KEY_INTR_ID) ;
	return XST_FAILURE ;
}
void Left_Interrupt_Handler(void *CallBackRef) {
	int key_value = 1;
	XGpio *GpioPtr = (XGpio *)CallBackRef;

	print("Left Interrupt Detected!\n");
	//XGpio_InterruptDisable(GpioPtr, KEY_MASK);				//�ر�AXI GPIO�ж�ʹ��
	XGpio_InterruptClear(GpioPtr, KEY_MASK);				//����ж�
	key_value = XGpio_DiscreteRead(GpioPtr, KEY_CHANNEL);	//��ȡ��������
	if(key_value == 0){										//�жϰ�������
		key_flag_left = 1;
		XScuTimer_Start(&ScuTimerInstance);
	}
	//usleep(25000);
	//XGpio_InterruptClear(GpioPtr, KEY_MASK);				//����ж�
	//XGpio_InterruptEnable(GpioPtr, KEY_MASK);				//ʹ��AXI GPIO�ж�
}
void Right_Interrupt_Handler(void *CallBackRef) {
	int key_value = 1;
	XGpio *GpioPtr = (XGpio *)CallBackRef;

	print("Right Interrupt Detected!\n");
	//XGpio_InterruptDisable(GpioPtr, KEY_MASK);				//�ر�AXI GPIO�ж�ʹ��
	XGpio_InterruptClear(GpioPtr, KEY_MASK);//����ж�
	key_value = XGpio_DiscreteRead(GpioPtr, KEY_CHANNEL);	//��ȡ��������
	if(key_value == 0){										//�жϰ�������
		key_flag_right = 1;
		XScuTimer_Start(&ScuTimerInstance);
	}
	//usleep(25000);
	//XGpio_InterruptClear(GpioPtr, KEY_MASK);//����ж�
	//XGpio_InterruptEnable(GpioPtr, KEY_MASK);				//ʹ��AXI GPIO�ж�
}
void Change_Interrupt_Handler(void *CallBackRef) {
	int key_value = 1;
	XGpio *GpioPtr = (XGpio *)CallBackRef;

	print("Change Currsor Interrupt Detected!\n");
	//XGpio_InterruptDisable(GpioPtr, KEY_MASK);				//�ر�AXI GPIO�ж�ʹ��
	XGpio_InterruptClear(GpioPtr, KEY_MASK);				//����ж�
	key_value = XGpio_DiscreteRead(GpioPtr, KEY_CHANNEL);	//��ȡ��������
	if(key_value == 0){										//�жϰ�������
		key_flag_currsor_change = ~key_flag_currsor_change;
	}
	//usleep(25000);
	//XGpio_InterruptEnable(GpioPtr, KEY_MASK);				//ʹ��AXI GPIO�ж�
}
void PsGpio_Interrupt_Handler(void *CallBackRef) {

	XGpioPs *GpioInstancePtr = (XGpioPs *)CallBackRef ;
	int Int_val ;
	print("Calculate Interrupt Detected!\n");
	Int_val = XGpioPs_IntrGetStatusPin(GpioInstancePtr, PS_KEY_MIO) ;
	/*
	 * Clear interrupt.
	 */
	XGpioPs_IntrClearPin(GpioInstancePtr, PS_KEY_MIO) ;
	if (Int_val) {
		//show_float(1012, 0, (abs(currsor_blue_hor_x - currsor_red_hor_x)) * (6.18e-3), framePtr, 4, 8, 0, 0, 0);//(abs(currsor_blue_hor_x - currsor_red_hor_x)) * (5.23e-3)
		show_float(1162, 0, (float)distance*(6.18e-3), framePtr, 4, 8, 0, 0, 0);
	}
}
/*
 * ��ʱ����ʼ������
 */
int Timer_Initial(u16 DeviceId, XScuTimer *ScuTimerPtr) {

	XScuTimer_Config *TimerConfig;
	int Status;
	//˽�ж�ʱ����ʼ��
	TimerConfig = XScuTimer_LookupConfig(DeviceId);
	if (NULL == TimerConfig)
		return XST_FAILURE;
	Status = XScuTimer_CfgInitialize(ScuTimerPtr, TimerConfig,
			TimerConfig->BaseAddr);
	if (Status != XST_SUCCESS)
		return XST_FAILURE;
	XScuTimer_LoadTimer(ScuTimerPtr, TIMER_LOAD_VALUE); // ���ؼ�������
	XScuTimer_EnableAutoReload(ScuTimerPtr);            // �����Զ�װ��ģʽ
	return XST_SUCCESS;
}

//��ʱ���жϳ�ʼ��
void timer_intr_init(XScuGic *intc_ptr,XScuTimer *timer_ptr)
{
    //���ö�ʱ���ж�
    XScuGic_Connect(intc_ptr, TIMER_IRPT_INTR,
          (Xil_ExceptionHandler)timer_intr_handler, (void *)timer_ptr);
    XScuGic_SetPriorityTriggerType(intc_ptr, KEY_INTR_ID,
    			0x00, 0x3);					   //????
    XScuGic_Enable(intc_ptr, TIMER_IRPT_INTR); //ʹ��GIC�еĶ�ʱ���ж�
    XScuTimer_EnableInterrupt(timer_ptr);      //ʹ�ܶ�ʱ���ж�
}

//��ʱ���жϴ������
void timer_intr_handler(void *CallBackRef)
{
	u32 key_value;
	XScuTimer *timer_ptr;
	XScuTimer_Stop(&ScuTimerInstance);
	timer_ptr = (XScuTimer *) CallBackRef;
	if(key_flag_right) {
		key_value = XGpio_DiscreteRead(&XGpioInstRight, KEY_CHANNEL);
		if(key_value == 0) {
			right_timer_flag = 1;
		}
		else {
			key_flag_right = 0;
			right_timer_flag = 0;
		}
	}
	if(key_flag_left) {
		key_value = XGpio_DiscreteRead(&XGpioInstLeft, KEY_CHANNEL);
		if(key_value == 0) {
			left_timer_flag = 1;
		}
		else {
			key_flag_left = 0;
			left_timer_flag = 0;
		}
	}
	XScuTimer_ClearInterruptStatus(timer_ptr);
}
