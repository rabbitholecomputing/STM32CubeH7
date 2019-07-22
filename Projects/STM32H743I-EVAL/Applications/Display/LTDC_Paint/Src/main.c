/**
  ******************************************************************************
  * @file    Display/LTDC_Paint/Src/main.c 
  * @author  MCD Application Team
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "save.h"
#include "color.h"
#include "color2.h"

/** @addtogroup STM32H7xx_HAL_Applications
  * @{
  */

/** @addtogroup LTDC_Paint
  * @{
  */ 

/* Private typedef -----------------------------------------------------------*/
typedef enum {
  APPLICATION_IDLE = 0,  
  APPLICATION_RUNNIG    
}MSC_ApplicationTypeDef;

/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
USBH_HandleTypeDef  hUSB_Host;
FATFS USBDISK_FatFs;  /* File system object for USB Disk logical drive */
FIL MyFile;           /* File object */
char USB_Path[4];     /* USB Disk logical drive path */

extern  DMA2D_HandleTypeDef hdma2d_eval;

static uint32_t Radius = 2;
static uint32_t x = 0, y = 0;
static TS_StateTypeDef  TS_State;

const uint32_t aBMPHeader1[14]=         
{0x26E64D42, 0x0000000B, 0x00360000, 0x00280000, 0x02440000, 0x01A40000, 0x00010000, 
 0x00000018, 0xF5400000, 0x00000006, 0x00000000, 0x00000000, 0x00000000, 0x0000};

const uint32_t aBMPHeader2[14]=         
{0x13A64D42, 0x00000004, 0x00360000, 0x00280000, 0x01A40000, 0x00D40000, 0x00010000, 
 0x00000018, 0xF5400000, 0x00000006, 0x00000000, 0x00000000, 0x00000000, 0x0000};

/* Variable to save the state of USB */
MSC_ApplicationTypeDef Appli_state = APPLICATION_IDLE;

uint8_t workBuffer[_MAX_SS];

/* Private function prototypes -----------------------------------------------*/
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id);
static void Draw_Menu(void);
static void GetPosition(void);
static void SystemClock_Config(void);
static void Error_Handler(void);
static void Save_Picture(void);
static void Update_Color(void);
static void Update_Size(uint8_t size);
static void CPU_CACHE_Enable(void);
static void MPU_Config(void);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program
  * @param  None
  * @retval None
  */
int main(void)
{
 
  /* Configure the MPU attributes as Write Through for SDRAM*/
  MPU_Config();
  /* Enable the CPU Cache */
  CPU_CACHE_Enable();

  /* STM32H7xx HAL library initialization:
       - Configure the Systick to generate an interrupt each 1 msec
       - Set NVIC Group Priority to 4
       - Low Level Initialization
     */
  HAL_Init();
  
  /* Configure the system clock to 400 MHz */
  SystemClock_Config(); 
  
  BSP_IO_Init();  
  
  /* Configure LED1 and LED3 */
  BSP_LED_Init(LED1);
  BSP_LED_Init(LED3);
  
  /*##-1- LCD Initialization #################################################*/ 
  /* Initialize the LCD */
  BSP_LCD_Init();
  
  /* Foreground Layer Initialization */
  BSP_LCD_LayerDefaultInit(1, LCD_FRAME_BUFFER_LAYER1);
  /* Set Foreground Layer */
  BSP_LCD_SelectLayer(1);
  /* Clear the LCD Foreground layer */
  BSP_LCD_Clear(LCD_COLOR_WHITE);
  /* Enable the color Key for foreground layer */   
  BSP_LCD_SetColorKeying(1, LCD_COLOR_WHITE);
  BSP_LCD_SetLayerVisible(1, DISABLE);
  
  /* Background Layer Initialization */
  BSP_LCD_LayerDefaultInit(0, LCD_FRAME_BUFFER_LAYER0);
  
  /* Set Foreground Layer */
  BSP_LCD_SelectLayer(0);
  
  /* Enable the LCD */
  BSP_LCD_DisplayOn();
  
  /* Clear the LCD Background layer */
  BSP_LCD_Clear(LCD_COLOR_WHITE);
  
  /*##-2- Touch screen initialization ########################################*/
  if(stmpe811_ts_drv.ReadID(TS_I2C_ADDRESS) == STMPE811_ID)
  {
    Touchscreen_Calibration();
  }
  
  BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
  
  /*##-3- USB Initialization #################################################*/ 
  
  /* Set the source of USB clock (PLL3*/
  __HAL_RCC_USB_CONFIG(RCC_USBCLKSOURCE_PLL3);

  /* Init Host Library */
  if (USBH_Init(&hUSB_Host, USBH_UserProcess, 0) != USBH_OK)
  {
    /* USB Initialization Error */
    Error_Handler();
  }
  
  /* Add Supported Class */
  USBH_RegisterClass(&hUSB_Host, USBH_MSC_CLASS);
  
  /* Start Host Process */
  if (USBH_Start(&hUSB_Host) != USBH_OK)
  {
    /* USB Initialization Error */
    Error_Handler();
  }
  
  /* Enable the USB voltage level detector */
  HAL_PWREx_EnableUSBVoltageDetector();
  
  /*##-4- Link the USB Mass Storage disk I/O driver ##########################*/
  if(FATFS_LinkDriver(&USBH_Driver, USB_Path) != 0) 
  {
    /* FatFs Initialization Error */
    Error_Handler();
  }
  
  /*##-5- Register the file system object to the FatFs module ################*/
  if(f_mount(&USBDISK_FatFs, (TCHAR const*)USB_Path, 0) != FR_OK)
  {
    /* FatFs Initialization Error */
    Error_Handler();
  }
  
  /*##-6- Draw the menu ######################################################*/
  Draw_Menu();  
  
  /* Infinite loop */  
  while (1)
  { 
    /*##-7- Configure the touch screen and Get the position ##################*/    
    GetPosition();
    
    USBH_Process(&hUSB_Host);
  }
}

/**
  * @brief  User Process
  * @param  None
  * @retval None
  */
static void USBH_UserProcess(USBH_HandleTypeDef *phost, uint8_t id)
{  
  switch (id)
  { 
  case HOST_USER_DISCONNECTION:
    Appli_state = APPLICATION_IDLE;
    if (f_mount(&USBDISK_FatFs, "", 0) != FR_OK)
    {
      /* FatFs Initialization Error */
      Error_Handler();
    }
    break;
    
  case HOST_USER_CLASS_ACTIVE:
    Appli_state = APPLICATION_RUNNIG;
    break;
  }
}

/**
  * @brief  Configures and gets Touch screen position.
  * @param  None
  * @retval None
  */
static void GetPosition(void)
{
  static uint32_t color_width;  
  static uint32_t color;
  
  if (BSP_LCD_GetXSize() == 640)
  {
    color_width = 36;
  }
  else
  {
    color_width = 19;
  }
  
  /* Get Touch screen position */
  BSP_TS_GetState(&TS_State); 
  
  /* Read the coordinate */
  if(stmpe811_ts_drv.ReadID(TS_I2C_ADDRESS) == STMPE811_ID)
  {
    x = Calibration_GetX(TS_State.x);
    y = Calibration_GetX(TS_State.y);
  }
  else
  {
    x = TS_State.x;
    y = TS_State.y;
  }
  
  if ((TS_State.TouchDetected) & (x > (67 + Radius)) & (y > (7 + Radius) ) & ( x < (BSP_LCD_GetXSize()-(7  + Radius )) ) & (y < (BSP_LCD_GetYSize()-(67 + Radius )) ))
  {
    BSP_LCD_FillCircle((x), (y), Radius);
  }
  else if ((TS_State.TouchDetected) & (x > 0 ) & ( x < 50 ))
  { 
    if ((TS_State.TouchDetected) & ( y > 0 ) & ( y < color_width ))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & ( y > color_width ) & (y < (2 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_YELLOW);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (2 * color_width)) & (y < (3 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_ORANGE);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (3 * color_width)) & (y < (4 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_LIGHTMAGENTA);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (4 * color_width)) & (y < (5 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_DARKGREEN);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (5 * color_width)) &(y < (6 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (6 * color_width)) &(y < (7 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_BROWN);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (7 * color_width)) & (y < (8 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_RED);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (8 * color_width)) & (y < (9 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_DARKMAGENTA);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (9 * color_width)) & (y < (10 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_CYAN);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (10 * color_width)) & (y < (11 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_DARKBLUE);
      Update_Size(Radius);
    }
    else if ((TS_State.TouchDetected) & (y > (11 * color_width)) & (y < (12 * color_width)))
    {
      BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
      Update_Size(Radius);
    }    
    else if ((TS_State.TouchDetected) &  (y > (12 * color_width)) & (y < (13 * color_width)))
    {
      /* Get the current text color */
      color = BSP_LCD_GetTextColor();
      BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
      /* Clear the working window */
      BSP_LCD_FillRect(68, 8, (BSP_LCD_GetXSize() - 75), (BSP_LCD_GetYSize() - 75));
      BSP_LCD_SetTextColor(color);
    }
    else
    {
      x = 0;
      y = 0;
    }
    Update_Color();    
  }
  else if ((TS_State.TouchDetected) & (x > 70 ) & (y > (12 * color_width)) & (y < (13 * color_width)) & ( x < 120 ))
  {    
    Radius = 20;
    Update_Size(Radius);
  }
  else if ((TS_State.TouchDetected) & (x > 120 ) & (y > (12 * color_width)) & (y < (13 * color_width)) & ( x < 170 ))
  {    
    Radius = 15;
    Update_Size(Radius);
  }
  else if ((TS_State.TouchDetected) & (x > 170 ) & (y > (12 * color_width)) & (y < (13 * color_width)) & ( x < 220 ))
  {    
    Radius = 10;
    Update_Size(Radius);
  }
  else if ((TS_State.TouchDetected) & (x > 220 ) & (y > (12 * color_width)) & (y < (13 * color_width)) & ( x < 270 ))
  {    
    Radius = 5;
    Update_Size(Radius);
  }
  else if ((TS_State.TouchDetected) & (x > 270 ) & (y > (12 * color_width)) & (y < (13 * color_width)) & ( x < 320 ))
  {    
    Radius = 2;
    Update_Size(Radius);
  }  
  else if ((TS_State.TouchDetected) & (((x > (BSP_LCD_GetXSize()-5) ) & (y > (12 * color_width)) & (y < (13 * color_width))) | (( x < 55 ) & ( y < 5 ))))
  {    
    TS_State.x = 0;
    TS_State.y = 0;
  }  
  else if ((TS_State.TouchDetected) & (x > 320) & (y > (BSP_LCD_GetYSize() - 50)) & (x < 370) & (y < BSP_LCD_GetYSize() ))
  {   
    Save_Picture();
  }    
}

/**
  * @brief  Draws the menu.
  * @param  None
  * @retval None
  */
static void Draw_Menu(void)
{ 
  /* Set background Layer */
  BSP_LCD_SelectLayer(0);
  
/* Clear the LCD */
  BSP_LCD_Clear(LCD_COLOR_WHITE);  

  if (BSP_LCD_GetXSize() == 640)
  {  
    /* Draw color image */
    BSP_LCD_DrawBitmap(0, 0, (uint8_t *)color);
  }
  else
  {
    /* Draw color image */
    BSP_LCD_DrawBitmap(0, 0, (uint8_t *)color2);
  }
  
  /* Draw save image */
  BSP_LCD_DrawBitmap(310, (BSP_LCD_GetYSize() - 50), (uint8_t *)save);

  /* Set Black as text color */
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
  
  /* Draw working window */
  BSP_LCD_DrawRect(61, 0, (BSP_LCD_GetXSize() - 61), (BSP_LCD_GetYSize() - 60));
  BSP_LCD_DrawRect(63, 3, (BSP_LCD_GetXSize() - 66), (BSP_LCD_GetYSize() - 66));
  BSP_LCD_DrawRect(65, 5, (BSP_LCD_GetXSize() - 70), (BSP_LCD_GetYSize() - 70));
  BSP_LCD_DrawRect(67, 7, (BSP_LCD_GetXSize() - 74), (BSP_LCD_GetYSize() - 74));
  
  /* Draw size icons */
  BSP_LCD_FillRect(60, (BSP_LCD_GetYSize() - 48), 250, 48);
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_FillCircle(95, (BSP_LCD_GetYSize() - 24), 20);
  BSP_LCD_FillCircle(145, (BSP_LCD_GetYSize() - 24), 15);
  BSP_LCD_FillCircle(195, (BSP_LCD_GetYSize() - 24), 10);
  BSP_LCD_FillCircle(245, (BSP_LCD_GetYSize() - 24), 5);
  BSP_LCD_FillCircle(295, (BSP_LCD_GetYSize() - 24), 2);  
  
  BSP_LCD_SetTextColor(LCD_COLOR_DARKRED);
  BSP_LCD_SetFont(&Font8);
  BSP_LCD_DisplayStringAt(360, (BSP_LCD_GetYSize() - 55), (uint8_t *)"Selected Color  Size", LEFT_MODE);  
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK); 
  BSP_LCD_FillRect(380, (BSP_LCD_GetYSize() - 40), 30, 30);  
  BSP_LCD_FillCircle(450, (BSP_LCD_GetYSize()- 24), Radius); 
}

/**
  * @brief  Save the picture in USB Disk.
  * @param  None
  * @retval None
  */
void Save_Picture(void)
{ 
  FRESULT res1, res2;    /* FatFs function common result code */
  uint32_t byteswritten; /* File write count */
  uint32_t sourceAddress = LCD_FRAME_BUFFER_LAYER0 + ((BSP_LCD_GetXSize() * (BSP_LCD_GetYSize() - 61) + 60) * 4);
  uint32_t index = 0;
  
  /* Configure the DMA2D For ARGB8888 to RGB888 conversion */
  hdma2d_eval.Init.Mode         = DMA2D_M2M_PFC;
  hdma2d_eval.Init.ColorMode    = DMA2D_OUTPUT_RGB888;
  hdma2d_eval.Init.OutputOffset = 0;     
  
  /* Foreground Configuration */
  hdma2d_eval.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d_eval.LayerCfg[1].InputAlpha = 0xFF;
  hdma2d_eval.LayerCfg[1].InputColorMode = DMA2D_INPUT_ARGB8888;
  hdma2d_eval.LayerCfg[1].InputOffset = 0;
  
  hdma2d_eval.Instance = DMA2D; 
  
  HAL_DMA2D_Init(&hdma2d_eval);
  HAL_DMA2D_ConfigLayer(&hdma2d_eval, 1);
  
  BSP_LCD_SetLayerVisible(1, ENABLE);
  BSP_LCD_SetColorKeying(1, LCD_COLOR_WHITE);
  /* Set foreground Layer */
  BSP_LCD_SelectLayer(1);
  BSP_LCD_SetTextColor(LCD_COLOR_DARKRED);
  BSP_LCD_SetFont(&Font20);
  
  /* Turn LED1 and LED3 Off */
  BSP_LED_Off(LED1);
  BSP_LED_Off(LED3);
  
  if (Appli_state == APPLICATION_RUNNIG)
  {
    BSP_LCD_DisplayStringAt(10, (BSP_LCD_GetYSize()-100), (uint8_t *)"Saving ... ", RIGHT_MODE);
    
    /*##-2- Create and Open a new bmp file object with write access ##########*/
    if(f_open(&MyFile, "image.bmp", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
      /* 'image.bmp' file Open for write Error */
      Error_Handler();
    }
    else
    {
      /*##-3- Write data to the BMP file #####################################*/
      /* Write the BMP header */
      if (BSP_LCD_GetXSize() == 640)
      {  
        /* if ampire 640x480 LCD is used */
        res1 = f_write(&MyFile, (uint32_t *)aBMPHeader1, 54, (void *)&byteswritten);
      }
      else
      {  
        /* if ampire 480x272 LCD is used */
        res1 = f_write(&MyFile, (uint32_t *)aBMPHeader2, 54, (void *)&byteswritten);
      }        
      
      /* Note that BMP file is organized as follow :
         -  first a header of 54 bytes
         - then pixels scan lines ordered from last line of the image to first one.
         It is then mandatory to reorder the image line from last line to first.
      */
      for(index=0; index < (BSP_LCD_GetYSize() - 60); index++)
      {        
        /* Convert a Line from ARGB8888 to RGB888 using the DMA2D */
        if (HAL_DMA2D_Start(&hdma2d_eval, sourceAddress, CONVERTED_LINE_BUFFER, (BSP_LCD_GetXSize() - 60), 1) == HAL_OK)
        {
          /* Polling For DMA transfer */  
          HAL_DMA2D_PollForTransfer(&hdma2d_eval, 10);
        }
        
        /*As the DMA2D destiantion address is located in the D1 AXI-SRAM which 
        is cacheable, it is necessary to invalidate the data cache the DMA2D transfer 
        before saving these data to the bmp file using the CPU */
        SCB_InvalidateDCache_by_Addr((uint32_t *)CONVERTED_LINE_BUFFER, ((BSP_LCD_GetXSize()-60)*3)); 
        
        /* Save Converted line to the bmp file */
        res2 = f_write(&MyFile, (uint32_t *)CONVERTED_LINE_BUFFER, ((BSP_LCD_GetXSize()-60)*3), (void *)&byteswritten);
        if((res2 != FR_OK) || (byteswritten == 0))
        {
          break;
        }
        /* Upodate DMA2D source Address */
        sourceAddress -= BSP_LCD_GetXSize()*4;
      }
      
      if((res1 != FR_OK) || (res2 != FR_OK) || (byteswritten == 0))
      {
        /* 'image' file Write or EOF Error */
        BSP_LED_On(LED3);
        BSP_LCD_DisplayStringAt(10, (BSP_LCD_GetYSize()-100), (uint8_t *)" Aborted...", RIGHT_MODE);
        /* Wait for 2s */
        HAL_Delay(2000);
        /* Disable the Layer 2 */
        BSP_LCD_SetLayerVisible(1, DISABLE);
        /* Select Layer 1 */
        BSP_LCD_SelectLayer(0);
      }
      else
      {
        /*##-4- Close the open bmp file #####################################*/
        f_close(&MyFile);
        
        /* Success of the demo: no error occurrence */
        BSP_LED_On(LED1);
        BSP_LCD_SetTextColor(LCD_COLOR_DARKGREEN);
        BSP_LCD_DisplayStringAt(10, (BSP_LCD_GetYSize()-100), (uint8_t *)" Saved     ", RIGHT_MODE);
        /* Wait for 2s */
        HAL_Delay(2000);
        /* Disable the Layer 2 */
        BSP_LCD_SetLayerVisible(1, DISABLE);
        /* Select Layer 1 */
        BSP_LCD_SelectLayer(0);
      }
    }
  }
  else
  {
    /* USB not connected */
    BSP_LCD_DisplayStringAt(10, (BSP_LCD_GetYSize()-100), (uint8_t *)"USB KO... ", RIGHT_MODE);
    /* Wait for 2s */
    HAL_Delay(2000);
    /* Disable the Layer 2 */
    BSP_LCD_SetLayerVisible(1, DISABLE);
    /* Select Layer 1 */
    BSP_LCD_SelectLayer(0);
  }
}

/**
  * @brief  Clock Config.
  * @param  hltdc: LTDC handle
  * @note   This API is called by BSP_LCD_Init()
  *         Being __weak it can be overwritten by the application
  * @retval None
  */
void BSP_LCD_ClockConfig(LTDC_HandleTypeDef *hltdc, void *Params)
{
  static RCC_PeriphCLKInitTypeDef  periph_clk_init_struct;

  if(stmpe811_ts_drv.ReadID(TS_I2C_ADDRESS) == STMPE811_ID)
  {
    /* AMPIRE480272 LCD clock configuration */
    /* LCD clock configuration */
    /* PLL3_VCO Input = HSE_VALUE/PLL3M = 1 Mhz */
    /* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 336 Mhz */
    /* PLLLCDCLK = PLL3_VCO Output/PLL3R = 336/37 = 9.08 Mhz */
    /* LTDC clock frequency = PLLLCDCLK = 9.08 Mhz */
    /* USB uses same pll3 as clock frequency and PLL3Q as devider: USB clock frequency = 48 Mhz */ 
    periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periph_clk_init_struct.PLL3.PLL3M = 25;    
    periph_clk_init_struct.PLL3.PLL3N = 336;
    periph_clk_init_struct.PLL3.PLL3FRACN = 0;
    periph_clk_init_struct.PLL3.PLL3P = 2;
    periph_clk_init_struct.PLL3.PLL3Q = 7;  
    periph_clk_init_struct.PLL3.PLL3R = 37;
    HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct);  
  }
  else
  {

    /* In case of double layers the bandwidth is arround 80MBytesPerSec => 20MHz (<25MHz) */
    /* so the PLL3R is configured to provide this clock */
    /* AMPIRE640480 LCD clock configuration */
    /* PLL3_VCO Input = HSE_VALUE/PLL3M = 1 Mhz */
    /* PLL3_VCO Output = PLL3_VCO Input * PLL3N = 336 Mhz */
    /* PLLLCDCLK = PLL3_VCO Output/PLL3R = 336/16 = 21Mhz */
    /* LTDC clock frequency = PLLLCDCLK = 21 Mhz */     
    /* USB uses same pll3 as clock frequency and PLL3Q as devider: USB clock frequency = 48 Mhz */ 
    periph_clk_init_struct.PLL3.PLL3R = 16;
    periph_clk_init_struct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    periph_clk_init_struct.PLL3.PLL3M = 25;    
    periph_clk_init_struct.PLL3.PLL3N = 336;
    periph_clk_init_struct.PLL3.PLL3FRACN = 0;
    periph_clk_init_struct.PLL3.PLL3P = 2;
    periph_clk_init_struct.PLL3.PLL3Q = 7;  
    HAL_RCCEx_PeriphCLKConfig(&periph_clk_init_struct); 
  }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
static void Error_Handler(void)
{
  /* Turn LED3 on */
  BSP_LED_On(LED3);
  while(1)
  {
  }
}

/**
  * @brief  Update the selected color
  * @param  None
  * @retval None
  */
static void Update_Color(void)
{
  static uint32_t color;
  
  /* Get the current text color */
  color = BSP_LCD_GetTextColor();
  /* Update the selected color icon */
  BSP_LCD_FillRect(380, (BSP_LCD_GetYSize()-40), 30, 30);
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);    
  BSP_LCD_DrawRect(380, (BSP_LCD_GetYSize()-40), 30, 30);
  BSP_LCD_SetTextColor(color);  
}

/**
  * @brief  Updates the selected size
  * @param  size: Size to be updated
  * @retval None
  */
static void Update_Size(uint8_t size)
{
  static uint32_t color;
  
  /* Get the current text color */ 
  color = BSP_LCD_GetTextColor();
  /* Update the selected size icon */
  BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
  BSP_LCD_FillCircle(450, (BSP_LCD_GetYSize()-24), 20);
  BSP_LCD_SetTextColor(color);  
  BSP_LCD_FillCircle(450, (BSP_LCD_GetYSize()-24), size);
  BSP_LCD_SetTextColor(LCD_COLOR_BLACK);    
  BSP_LCD_DrawCircle(450, (BSP_LCD_GetYSize()-24), size);
  BSP_LCD_SetTextColor(color);  
}

/**
  * @brief  System Clock Configuration
  *         The system Clock is configured as follow : 
  *            System Clock source            = PLL (HSE)
  *            SYSCLK(Hz)                     = 400000000 (CPU Clock)
  *            HCLK(Hz)                       = 200000000 (AXI and AHBs Clock)
  *            AHB Prescaler                  = 2
  *            D1 APB3 Prescaler              = 2 (APB3 Clock  100MHz)
  *            D2 APB1 Prescaler              = 2 (APB1 Clock  100MHz)
  *            D2 APB2 Prescaler              = 2 (APB2 Clock  100MHz)
  *            D3 APB4 Prescaler              = 2 (APB4 Clock  100MHz)
  *            HSE Frequency(Hz)              = 25000000
  *            PLL_M                          = 5
  *            PLL_N                          = 160
  *            PLL_P                          = 2
  *            PLL_Q                          = 4
  *            PLL_R                          = 2
  *            VDD(V)                         = 3.3
  *            Flash Latency(WS)              = 4
  * @param  None
  * @retval None
  */
static void SystemClock_Config(void)
{
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_OscInitTypeDef RCC_OscInitStruct;
  HAL_StatusTypeDef ret = HAL_OK;
  
  /*!< Supply configuration update enable */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /* The voltage scaling allows optimizing the power consumption when the device is
     clocked below the maximum system frequency, to update the voltage scaling value
     regarding system frequency refer to product datasheet.  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  
  /* Enable HSE Oscillator and activate PLL with HSE as source */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
  RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;

  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
  if(ret != HAL_OK)
  {
    Error_Handler();
  }

 
/* Select PLL as system clock source and configure  bus clocks dividers */
  RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
                                 RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1);

  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;  
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2; 
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2; 
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2; 
  ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
  if(ret != HAL_OK)
  {
    Error_Handler();
  }
 
  /*activate CSI clock mondatory for I/O Compensation Cell*/  
  __HAL_RCC_CSI_ENABLE() ;
    
  /* Enable SYSCFG clock mondatory for I/O Compensation Cell */
  __HAL_RCC_SYSCFG_CLK_ENABLE() ;
  
  /* Enables the I/O Compensation Cell */    
  HAL_EnableCompensationCell();  
}

/**
  * @brief  CPU L1-Cache enable.
  * @param  None
  * @retval None
  */
static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}


/**
  * @brief  Configure the MPU attributes as Write Through for External SDRAM.
  * @note   The Base Address is 0xD0000000 .
  *         The Configured Region Size is 32MB because same as SDRAM size.
  * @param  None
  * @retval None
  */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct;
  
  /* Disable the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU attributes as WT for SDRAM */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress = 0xD0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enable the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}


#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{ 
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */

  /* Infinite loop */
  while (1)
  {
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/