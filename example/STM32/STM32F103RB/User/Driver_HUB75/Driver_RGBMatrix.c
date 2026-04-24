#include "Driver_RGBMatrix.h"

#if (HUB75_SCAN_ROWS == 0)
#error "HUB75_SCAN_ROWS must be greater than 0"
#endif
#if ((HUB75_PANEL_HEIGHT % 2u) != 0u)
#error "HUB75_PANEL_HEIGHT must be even"
#endif
#if (HUB75_SCAN_ROWS > (HUB75_PANEL_HEIGHT / 2u))
#error "HUB75_SCAN_ROWS must not exceed HUB75_PANEL_HEIGHT / 2"
#endif
#if (HUB75_SCAN_ROWS > 32u)
#error "HUB75_SCAN_ROWS must not exceed 32 with A/B/C/D/E address lines"
#endif

volatile uint8_t display_tick = 0;
HUB75_port RGB_Matrix;
uint8_t hub75_blink = 0;
uint8_t hub75_color = HUB75_Color_Pink;
static uint8_t hub75_brightness;
uint8_t hub75_buff[HUB75_PANEL_WIDTH/8 * HUB75_PANEL_HEIGHT];
uint8_t hub75_panel_buff[HUB75_PANEL_WIDTH * HUB75_PANEL_HEIGHT / 2 * 3];
/**
 * Initialization routine.
 * You might need to enable access to DWT registers on Cortex-M7
 *   DWT->LAR = 0xC5ACCE55
 */
void DWT_Init(void)
{
    if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
}

/**
 * Delay routine itself.
 * Time is in microseconds (1/1000000th of a second), not to be
 * confused with millisecond (1/1000th).
 *
 * No need to check an overflow. Let it just tick :)
 *
 * @param uint32_t us  Number of microseconds to delay for
 */
void Delay_us(uint32_t us) // microseconds
{
    uint32_t startTick = DWT->CYCCNT,
             delayTicks = us * (SystemCoreClock/1000000);

    while (DWT->CYCCNT - startTick < delayTicks);
}


void HUB75_Init(void)
{
  HUB75_OE_H();
  HUB75_CLK_L();
  HUB75_LAT_L();

  HUB75_DR1_L();
  HUB75_DG1_L();
  HUB75_DB1_L();
  HUB75_DR2_L();
  HUB75_DG2_L();
  HUB75_DB2_L();

  HUB75_A_L();
  HUB75_B_L();
  HUB75_C_L();
  HUB75_D_L();
  HUB75_E_L();
}

// Set refresh rate of the matrix
// level: 1-4, 1: highest, 4: lowest
void HUB75_SetRefreshRate(uint8_t level)
{
  uint32_t arr = 0;
  switch(level)
  {
    case 1:
      arr = 8U;
      break;
    case 2:
      arr = 12U;
      break;
    case 3:
      arr = 16U;
      break;
    case 4:
    default:
      arr = 20U;
      break;
  }
  __HAL_TIM_SET_COUNTER(&htim1, 0);
  __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
  HAL_TIM_Base_Start_IT(&htim1);
}

/*
 * @brief  Set brightness of the matrix
 * @param brightness Brightness value (0-255)
 * @return None
 */
void HUB75_SetBrightness(uint8_t brightness)
{
  hub75_brightness = brightness;
}

// Change the screen display time by controlling the OE pin
static void HUB75_OE_Window(void)
{
  uint16_t ticks = (uint16_t)hub75_brightness;
  if(ticks == 0)
  {
    HUB75_OE_H();
    return;
  }
  while(ticks--)
    __NOP();
  HUB75_OE_H();
}

void send_scan_line(unsigned char line)
{
  (line & 0x0001) ? HUB75_A_H() : HUB75_A_L();
  (line & 0x0002) ? HUB75_B_H() : HUB75_B_L();
  (line & 0x0004) ? HUB75_C_H() : HUB75_C_L();
  (line & 0x0008) ? HUB75_D_H() : HUB75_D_L();
  (line & 0x0010) ? HUB75_E_H() : HUB75_E_L();
}

void HUB75_WriteByte(uint8_t p_buff[], uint8_t color)
{
  static uint16_t row = 0;
  uint16_t scan_row = HUB75_MAP_SCAN_ROW(row);
  uint8_t data_r1, data_g1, data_b1, data_r2, data_g2, data_b2 = 0;
  HUB75_OE_H();

  /* write column data */
  for(uint16_t i=0; i<(HUB75_PANEL_WIDTH/8); i++)
  {
    data_r1 = data_g1 = data_b1 = 0;
    data_r2 = data_g2 = data_b2 = 0;

    /* color red */
    if(color & 0x01)
    {
      data_b1 = p_buff[row*(HUB75_PANEL_WIDTH/8)+i];
      data_b2 = p_buff[row*(HUB75_PANEL_WIDTH/8)+(HUB75_PANEL_WIDTH/8*HUB75_PANEL_HEIGHT/2)+i];
    }

    /* color green */
    if(color & 0x02)
    {
      data_r1 = p_buff[row*(HUB75_PANEL_WIDTH/8)+i];
      data_r2 = p_buff[row*(HUB75_PANEL_WIDTH/8)+(HUB75_PANEL_WIDTH/8*HUB75_PANEL_HEIGHT/2)+i];
    }

    /* color blue */
    if(color & 0x04)
    {
      data_g1 = p_buff[row*(HUB75_PANEL_WIDTH/8)+i];
      data_g2 = p_buff[row*(HUB75_PANEL_WIDTH/8)+(HUB75_PANEL_WIDTH/8*HUB75_PANEL_HEIGHT/2)+i];
    }

    /* horizontal 8 bits data */
    for(uint8_t k=0; k<8; k++)
    {
      ((data_r1>>(7-k)) & 0x01) ? HUB75_DR1_H() : HUB75_DR1_L();
      ((data_g1>>(7-k)) & 0x01) ? HUB75_DG1_H() : HUB75_DG1_L();
      ((data_b1>>(7-k)) & 0x01) ? HUB75_DB1_H() : HUB75_DB1_L();
      ((data_r2>>(7-k)) & 0x01) ? HUB75_DR2_H() : HUB75_DR2_L();
      ((data_g2>>(7-k)) & 0x01) ? HUB75_DG2_H() : HUB75_DG2_L();
      ((data_b2>>(7-k)) & 0x01) ? HUB75_DB2_H() : HUB75_DB2_L();

      HUB75_CLK_L();
      HUB75_CLK_H();
    }
  }

  HUB75_OE_H();
  HUB75_LAT_H();

  /* write row addr */
  (scan_row & 0x0001) ? HUB75_A_H() : HUB75_A_L();
  (scan_row & 0x0002) ? HUB75_B_H() : HUB75_B_L();
  (scan_row & 0x0004) ? HUB75_C_H() : HUB75_C_L();
  (scan_row & 0x0008) ? HUB75_D_H() : HUB75_D_L();
  (scan_row & 0x0010) ? HUB75_E_H() : HUB75_E_L();
  HUB75_LAT_L();
  HUB75_OE_L();
  HUB75_OE_Window();

  if(++row >= HUB75_SCAN_ROWS){
    row = 0;
  }
}

void HUB75_WritePixel(uint8_t p_buff[])
{
  static uint16_t plane = 0;
  static uint16_t row = 0;
  uint16_t scan_row = HUB75_MAP_SCAN_ROW(row);
  uint8_t data_r1, data_g1, data_b1, data_r2, data_g2, data_b2 = 0;

  static uint16_t bcm_cnt = 15;
  HUB75_OE_H();

  for(uint16_t i=0; i<HUB75_PANEL_WIDTH; i++)
  {
    if(plane == 0)
    {
      data_r1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 1 & 0x01; //plane3 bit1
      data_r2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 0 & 0x01; //plane3 bit0
      data_g1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 1 & 0x01; //plane2 bit1
      data_g2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 0 & 0x01; //plane2 bit0
      data_b1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 1 & 0x01; //plane1 bit1
      data_b2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 0 & 0x01; //plane1 bit0
    }
    else if(plane == 1)
    {
      data_r1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 5 & 0x01; //plane1 bit5
      data_r2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 2 & 0x01; //plane1 bit2
      data_g1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 6 & 0x01; //plane1 bit6
      data_g2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 3 & 0x01; //plane1 bit3
      data_b1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 7 & 0x01; //plane1 bit7
      data_b2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+i] >> 4 & 0x01; //plane1 bit4
    }
    else if(plane == 2)
    {
      data_r1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 5 & 0x01; //plane2 bit5
      data_r2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 2 & 0x01; //plane2 bit2
      data_g1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 6 & 0x01; //plane2 bit6
      data_g2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 3 & 0x01; //plane2 bit3
      data_b1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 7 & 0x01; //plane2 bit7
      data_b2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+i] >> 4 & 0x01; //plane2 bit4
    }
    else if(plane == 3)
    {
      data_r1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 5 & 0x01; //plane3 bit5
      data_r2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 2 & 0x01; //plane3 bit2
      data_g1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 6 & 0x01; //plane3 bit6
      data_g2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 3 & 0x01; //plane3 bit3
      data_b1 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 7 & 0x01; //plane3 bit7
      data_b2 = p_buff[row*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+i] >> 4 & 0x01; //plane3 bit4
    }


    (data_r1) ? HUB75_DR1_H() : HUB75_DR1_L();
    (data_g1) ? HUB75_DG1_H() : HUB75_DG1_L();
    (data_b1) ? HUB75_DB1_H() : HUB75_DB1_L();
    (data_r2) ? HUB75_DR2_H() : HUB75_DR2_L();
    (data_g2) ? HUB75_DG2_H() : HUB75_DG2_L();
    (data_b2) ? HUB75_DB2_H() : HUB75_DB2_L();
    HUB75_CLK_L();
    HUB75_CLK_H();
  }

  HUB75_OE_H();
  HUB75_LAT_H();
  /* write row addr */
  (scan_row & 0x0001) ? HUB75_A_H() : HUB75_A_L();
  (scan_row & 0x0002) ? HUB75_B_H() : HUB75_B_L();
  (scan_row & 0x0004) ? HUB75_C_H() : HUB75_C_L();
  (scan_row & 0x0008) ? HUB75_D_H() : HUB75_D_L();
  (scan_row & 0x0010) ? HUB75_E_H() : HUB75_E_L();
  HUB75_LAT_L();
  HUB75_OE_L();
  HUB75_OE_Window();

  /* BCM 8-4-2-1 */
  if(++row == HUB75_SCAN_ROWS)
  {
    row = 0;
    if(bcm_cnt > 1)
    {
      bcm_cnt--;
      if(bcm_cnt == ((8>>plane)-1))
      {
        plane++;
      }
    }
    else
    {
      bcm_cnt = 15;
      plane = 0;
    }
  }
}

void HUB75_WritePanel(uint8_t R, uint8_t G, uint8_t B, uint16_t row, uint16_t column)
{
  uint8_t *p_plane1 = NULL, *p_plane2 = NULL, *p_plane3 = NULL;

  if((row >= HUB75_PANEL_HEIGHT) || (column >= HUB75_PANEL_WIDTH))
    return;

  p_plane1 = &hub75_panel_buff[row%(HUB75_PANEL_HEIGHT/2)*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*0+column];
  p_plane2 = &hub75_panel_buff[row%(HUB75_PANEL_HEIGHT/2)*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*1+column];
  p_plane3 = &hub75_panel_buff[row%(HUB75_PANEL_HEIGHT/2)*HUB75_PANEL_WIDTH+HUB75_PANEL_WIDTH*HUB75_PANEL_HEIGHT/2*2+column];

  if(row < (HUB75_PANEL_HEIGHT/2))      //Top
  {
    if((R >> 7) & 0x01) *p_plane3 |= (1 << 1);
    else                *p_plane3 &= ~(1 << 1);

    if((R >> 6) & 0x01) *p_plane1 |= (1 << 5);
    else                *p_plane1 &= ~(1 << 5);

    if((R >> 5) & 0x01) *p_plane2 |= (1 << 5);
    else                *p_plane2 &= ~(1 << 5);

    if((R >> 4) & 0x01) *p_plane3 |= (1 << 5);
    else                *p_plane3 &= ~(1 << 5);

    if((G >> 7) & 0x01) *p_plane2 |= (1 << 1);
    else                *p_plane2 &= ~(1 << 1);

    if((G >> 6) & 0x01) *p_plane1 |= (1 << 6);
    else                *p_plane1 &= ~(1 << 6);

    if((G >> 5) & 0x01) *p_plane2 |= (1 << 6);
    else                *p_plane2 &= ~(1 << 6);

    if((G >> 4) & 0x01) *p_plane3 |= (1 << 6);
    else                *p_plane3 &= ~(1 << 6);

    if((B >> 7) & 0x01) *p_plane1 |= (1 << 1);
    else                *p_plane1 &= ~(1 << 1);

    if((B >> 6) & 0x01) *p_plane1 |= (1 << 7);
    else                *p_plane1 &= ~(1 << 7);

    if((B >> 5) & 0x01) *p_plane2 |= (1 << 7);
    else                *p_plane2 &= ~(1 << 7);

    if((B >> 4) & 0x01) *p_plane3 |= (1 << 7);
    else                *p_plane3 &= ~(1 << 7);
  }
  else                                  //Bottom
  {
    if((R >> 7) & 0x01) *p_plane3 |= (1 << 0);
    else                *p_plane3 &= ~(1 << 0);

    if((R >> 6) & 0x01) *p_plane1 |= (1 << 2);
    else                *p_plane1 &= ~(1 << 2);

    if((R >> 5) & 0x01) *p_plane2 |= (1 << 2);
    else                *p_plane2 &= ~(1 << 2);

    if((R >> 4) & 0x01) *p_plane3 |= (1 << 2);
    else                *p_plane3 &= ~(1 << 2);

    if((G >> 7) & 0x01) *p_plane2 |= (1 << 0);
    else                *p_plane2 &= ~(1 << 0);

    if((G >> 6) & 0x01) *p_plane1 |= (1 << 3);
    else                *p_plane1 &= ~(1 << 3);

    if((G >> 5) & 0x01) *p_plane2 |= (1 << 3);
    else                *p_plane2 &= ~(1 << 3);

    if((G >> 4) & 0x01) *p_plane3 |= (1 << 3);
    else                *p_plane3 &= ~(1 << 3);

    if((B >> 7) & 0x01) *p_plane1 |= (1 << 0);
    else                *p_plane1 &= ~(1 << 0);

    if((B >> 6) & 0x01) *p_plane1 |= (1 << 4);
    else                *p_plane1 &= ~(1 << 4);

    if((B >> 5) & 0x01) *p_plane2 |= (1 << 4);
    else                *p_plane2 &= ~(1 << 4);

    if((B >> 4) & 0x01) *p_plane3 |= (1 << 4);
    else                *p_plane3 &= ~(1 << 4);

	}
}

void HUB75_LoadRGB565Frame(const uint16_t *frame, uint16_t width, uint16_t height)
{
  uint16_t y_max = height;
  uint16_t x_max = width;

  if(frame == NULL)
    return;
  if((width == 0U) || (height == 0U))
    return;

  if(y_max > HUB75_PANEL_HEIGHT)
    y_max = HUB75_PANEL_HEIGHT;
  if(x_max > HUB75_PANEL_WIDTH)
    x_max = HUB75_PANEL_WIDTH;

  memset(hub75_panel_buff, 0, sizeof(hub75_panel_buff));

  for(uint16_t y = 0; y < y_max; y++)
  {
    for(uint16_t x = 0; x < x_max; x++)
    {
      uint16_t pixel = frame[(uint32_t)y * width + x];
      uint8_t r5 = (uint8_t)((pixel >> 11) & 0x1F);
      uint8_t g6 = (uint8_t)((pixel >> 5) & 0x3F);
      uint8_t b5 = (uint8_t)(pixel & 0x1F);
      uint8_t r8 = (uint8_t)((r5 << 3) | (r5 >> 2));
      uint8_t g8 = (uint8_t)((g6 << 2) | (g6 >> 4));
      uint8_t b8 = (uint8_t)((b5 << 3) | (b5 >> 2));
      HUB75_WritePanel(r8, g8, b8, y, x);
    }
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM1)
    display_tick = 1;
}

/*
 * @brief  Execute one HUB75 display refresh step gated by TIM1.
 * @note   TIM1 update interrupt sets display_tick in HAL_TIM_PeriodElapsedCallback().
 *         This function only pushes one scan step when that TIM1 flag is set.
 * @retval None
 */
void HUB75_Display(void)
{
  if(display_tick == 0)
    return;
  display_tick = 0;
  HUB75_WritePixel(hub75_panel_buff);
}
