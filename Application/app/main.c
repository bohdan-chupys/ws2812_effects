#include "stm32f4xx.h"
#include "stm324xg_eval.h"
#include "stm32f4_pwm_timer.h"

#include "audio_buffer.h"

#include "usbd_audio_core.h"
#include "usbd_usr.h"
#include "usb_conf.h"

#include "adapter/adapter.h"

#include <stdbool.h>

extern void stm32_cfft_convert(int16_t *buf, uint16_t fft_size);
extern void stm32_normalise_function(int16_t *buf, uint16_t size);
void Delay(__IO uint32_t nTime);
void usb_sampling_wrapper(int16_t *samples, uint16_t size);

__ALIGN_BEGIN USB_OTG_CORE_HANDLE  USB_OTG_dev __ALIGN_END;

static __IO uint32_t TimingDelay;

static struct adapter *ws2812_adapter = NULL;

static struct source_config_function config0 =
{
  .base.type = SOURCE_TYPE_LINEAR,
  .k = 1,
  .b = 1,
  .y_max = 255,
  .change_step_b = 1,
  .change_step_k = 0
};

static struct source_config_function config1 =
{
  .base.type = SOURCE_TYPE_LINEAR,
  .k = 0,
  .b = 100,
  .y_max = 255,
  .change_step_b = 0,
  .change_step_k = 0
};

static struct source_config_function config2 =
{
  .base.type = SOURCE_TYPE_LINEAR,
  .k = 0,
  .b = 100,
  .y_max = 255,
  .change_step_b = 0,
  .change_step_k = 0
};

static struct source_config_music music = 
{
  .base.type = SOURCE_TYPE_MUSIC,
  .is_fft_conversion_async = false,
  .is_sampling_async = true,
  .sampling_fnc = usb_sampling_wrapper,
  .fft_convert_fnc = stm32_cfft_convert,
  .normalise_fnc = stm32_normalise_function
};

void USBAudioInit()
{
  USBD_Init(&USB_OTG_dev,
#ifdef USE_USB_OTG_HS
            USB_OTG_HS_CORE_ID,
#else
            USB_OTG_FS_CORE_ID, 
#endif
            &USR_desc, &AUDIO_cb, &USR_cb);
}

static struct ws2812_operation_fn_table fn = 
{
  .hw_start_dma = start_dma_wrapper,
  .hw_stop_dma = stop_dma_wrapper,
  .hw_start_timer = TIM_start,
  .hw_stop_timer = TIM_stop,
  .hw_delay = Delay
};

int main(void)
{
  RCC_ClocksTypeDef RCC_Clocks;
  struct source_config *first = (struct source_config *)(&music);
  struct source_config *second = (struct source_config *)(&config1);
  struct source_config *third = (struct source_config *)(&config2);

  /* Initialize LEDS */
  STM_EVAL_LEDInit(LED3);
  STM_EVAL_LEDInit(LED4);
       
  /* SysTick end of count event each 10ms */
  RCC_GetClocksFreq(&RCC_Clocks);
  SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);

  RCC_HSEConfig(RCC_HSE_ON);
  while(!RCC_WaitForHSEStartUp())
  {
  }

  timer_pwm_init();

  ws2812_adapter = adapter_init(&fn, HSV, CONFIG_DELAY_MS);
  adapter_set_source_originator_from_config(ws2812_adapter, first, second, third);

  USBAudioInit();

  while (1)
  {
    adapter_process(ws2812_adapter);
  }
}

void Delay(__IO uint32_t nTime)
{
  TimingDelay = nTime;
  
  while(TimingDelay != 0);
}

void TimingDelay_Decrement(void)
{
  if (TimingDelay != 0x00)
  { 
    TimingDelay--;
  }
}

void led_strip_dma_ISRHandler()
{
  ws2812_adapter->base.dma_interrupt(&ws2812_adapter->base);
}

void led_strip_timer_ISRHandler()
{
  ws2812_adapter->base.timer_interrupt(&ws2812_adapter->base);
}

void usb_samping_finish()
{
  sampling_async_finish(ws2812_adapter->aggregator->first);
}

void usb_sampling_wrapper(int16_t *samples, uint16_t size)
{
  um_buffer_handle_register_listener(samples, size, usb_samping_finish);
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
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

