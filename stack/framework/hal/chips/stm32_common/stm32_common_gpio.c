/* * OSS-7 - An opensource implementation of the DASH7 Alliance Protocol for ultra
 * lowpower wireless sensor communication
 *
 * Copyright 2018 University of Antwerp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*! \file stm32_common_gpio.c

 *
 */

#include "debug.h"
#include "stm32_device.h"
#include "hwgpio.h"
#include "stm32_common_gpio.h"
#include "stm32_common_mcu.h"
#include "hwatomic.h"
#include "errors.h"

#define PORT_BASE(pin)  ((GPIO_TypeDef*)(pin & ~0x0F))

#define RCC_GPIO_CLK_ENABLE(__GPIO_PORT__)                    \
do {                                                          \
    switch( __GPIO_PORT__)                                    \
    {                                                         \
      case GPIOA_BASE: __HAL_RCC_GPIOA_CLK_ENABLE(); break;   \
      case GPIOB_BASE: __HAL_RCC_GPIOB_CLK_ENABLE(); break;   \
      case GPIOC_BASE: __HAL_RCC_GPIOC_CLK_ENABLE(); break;   \
      case GPIOD_BASE: __HAL_RCC_GPIOD_CLK_ENABLE(); break;   \
      case GPIOE_BASE: __HAL_RCC_GPIOE_CLK_ENABLE(); break;   \
      case GPIOH_BASE: __HAL_RCC_GPIOH_CLK_ENABLE(); break;   \
    }                                                         \
  } while(0)

#define RCC_GPIO_CLK_DISABLE(__GPIO_PORT__)                   \
do {                                                          \
    switch( __GPIO_PORT__)                                    \
    {                                                         \
      case GPIOA_BASE: __HAL_RCC_GPIOA_CLK_DISABLE(); break;  \
      case GPIOB_BASE: __HAL_RCC_GPIOB_CLK_DISABLE(); break;  \
      case GPIOC_BASE: __HAL_RCC_GPIOC_CLK_DISABLE(); break;  \
      case GPIOD_BASE: __HAL_RCC_GPIOD_CLK_DISABLE(); break;  \
      case GPIOE_BASE: __HAL_RCC_GPIOE_CLK_DISABLE(); break;  \
      case GPIOH_BASE: __HAL_RCC_GPIOH_CLK_DISABLE(); break;  \
    }                                                         \
  } while(0)

typedef struct
{
  gpio_inthandler_t callback;
  uint32_t interrupt_port;
} gpio_interrupt_t;


//the list of configured interrupts
static gpio_interrupt_t interrupts[16];

static uint16_t gpio_pins_configured[6];

__LINK_C void __gpio_init()
{
  for(int i = 0; i < 16; i++)
  {
    interrupts[i].callback = 0x0;
    interrupts[i].interrupt_port = 0xFF; //signal that a port has not yet been chosen
  }
  for(int i = 0; i < 6; i++)
    gpio_pins_configured[i] = 0;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  // set all pins to analog by default
  GPIO_InitTypeDef GPIO_InitStruct= { 0 };
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  // for all pins except debug pins (SWCLK and SWD) // TODO disable in release build
//  GPIO_InitStruct.Pin = GPIO_PIN_All & (~( GPIO_PIN_13 | GPIO_PIN_14) );
//  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOD_CLK_DISABLE();
  __HAL_RCC_GPIOE_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();
}

__LINK_C error_t hw_gpio_configure_pin_stm(pin_id_t pin_id, GPIO_InitTypeDef* init_options)
{
  RCC_GPIO_CLK_ENABLE((uint32_t)PORT_BASE(pin_id));
  init_options->Pin = 1 << GPIO_PIN(pin_id);
  HAL_GPIO_Init(PORT_BASE(pin_id), init_options);

  if  (init_options->Mode == GPIO_MODE_IT_RISING || init_options->Mode == GPIO_MODE_IT_FALLING || init_options->Mode == GPIO_MODE_IT_RISING_FALLING)
  {
    interrupts[GPIO_PIN(pin_id)].interrupt_port = GPIO_PORT(pin_id);
  }

  //RCC_GPIO_CLK_DISABLE((uint32_t)PORT_BASE(pin_id));
  // TODO for now keep the clock for all configured ports as enabled. We might disable them if the pin configuration allows this
  // and if no other pins on the port require this (for eg pins using AF for SPI etc)

  return SUCCESS;
}

__LINK_C error_t hw_gpio_configure_pin(pin_id_t pin_id, bool int_allowed, uint32_t mode, unsigned int out)
{
  //do early-stop error checking
  //if((gpio_pins_configured[GPIO_PORT(pin_id)] & (1<<GPIO_PIN(pin_id))))
  //return EALREADY;
  //else if(int_allowed && (interrupts[GPIO_PIN(pin_id)].interrupt_port != 0xFF))
  //return EBUSY;

  //set the pin to be configured
  //gpio_pins_configured[GPIO_PORT(pin_id)] |= (1<<GPIO_PIN(pin_id));

  //configure the pin itself
  GPIO_InitTypeDef GPIO_InitStruct;
  GPIO_InitStruct.Mode = mode;
  if(GPIO_InitStruct.Mode == GPIO_MODE_IT_RISING
     || GPIO_InitStruct.Mode == GPIO_MODE_IT_FALLING
     || GPIO_InitStruct.Mode == GPIO_MODE_IT_RISING_FALLING)
  {
    GPIO_InitStruct.Pull = GPIO_NOPULL;
  } else {
    GPIO_InitStruct.Pull = GPIO_PULLDOWN; // todo ??
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  }

  return hw_gpio_configure_pin_stm(pin_id, &GPIO_InitStruct);
}

__LINK_C error_t hw_gpio_set(pin_id_t pin_id)
{
  HAL_GPIO_WritePin(PORT_BASE(pin_id), 1 << GPIO_PIN(pin_id), GPIO_PIN_SET);
  return SUCCESS;
}

__LINK_C error_t hw_gpio_clr(pin_id_t pin_id)
{
  HAL_GPIO_WritePin(PORT_BASE(pin_id), 1 << GPIO_PIN(pin_id), GPIO_PIN_RESET);
  return SUCCESS;
}

__LINK_C error_t hw_gpio_toggle(pin_id_t pin_id)
{
  HAL_GPIO_TogglePin(PORT_BASE(pin_id), 1 << GPIO_PIN(pin_id));
  return SUCCESS;
}

__LINK_C bool hw_gpio_get_out(pin_id_t pin_id)
{
  // todo check pin is not input pin
  return (HAL_GPIO_ReadPin(PORT_BASE(pin_id), 1 << GPIO_PIN(pin_id)) == GPIO_PIN_SET);
}

__LINK_C bool hw_gpio_get_in(pin_id_t pin_id)
{
  return (HAL_GPIO_ReadPin(PORT_BASE(pin_id), 1 << GPIO_PIN(pin_id)) == GPIO_PIN_SET);
}

static void gpio_int_callback(uint8_t pin)
{
  start_atomic();
  assert(interrupts[pin].callback != 0x0);
  pin_id_t id = PIN(interrupts[pin].interrupt_port, pin);
  interrupts[pin].callback(id,hw_gpio_get_in(id));
  // TODO clear?
  end_atomic();
}

__LINK_C error_t hw_gpio_set_edge_interrupt(pin_id_t pin_id, uint8_t edge)
{
  uint32_t exti_line = 1 << GPIO_PIN(pin_id);
  switch(edge)
  {
    case GPIO_RISING_EDGE:
      LL_EXTI_DisableFallingTrig_0_31(exti_line);
      LL_EXTI_EnableRisingTrig_0_31(exti_line);
      break;
    case GPIO_FALLING_EDGE:
      LL_EXTI_DisableRisingTrig_0_31(exti_line);
      LL_EXTI_EnableFallingTrig_0_31(exti_line);
      break;
    case (GPIO_RISING_EDGE | GPIO_FALLING_EDGE):
      LL_EXTI_EnableRisingTrig_0_31(exti_line);
      LL_EXTI_EnableFallingTrig_0_31(exti_line);
      break;
    default:
      assert(false);
      break;
  }

  return SUCCESS;
}

__LINK_C error_t hw_gpio_configure_interrupt(pin_id_t pin_id, gpio_inthandler_t callback, uint8_t event_mask)
{
  if (interrupts[GPIO_PIN(pin_id)].interrupt_port != 0xFF)
  {
    if (interrupts[GPIO_PIN(pin_id)].interrupt_port != GPIO_PORT(pin_id))
      return EOFF;
  } else {
    interrupts[GPIO_PIN(pin_id)].interrupt_port = GPIO_PORT(pin_id);
  }

  if(callback == 0x0 || event_mask > (GPIO_RISING_EDGE | GPIO_FALLING_EDGE))
    return EINVAL;

  error_t err;
  start_atomic();
  //do this check atomically: interrupts[..] callback is altered by this function
  //so the check belongs in the critical section as well
  /*if(interrupts[GPIO_PIN(pin_id)].callback != 0x0 && interrupts[GPIO_PIN(pin_id)].callback != callback)
    err = EBUSY;
  else
  {*/
    interrupts[GPIO_PIN(pin_id)].callback = callback;

    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // set external interrupt configuration
    uint32_t temp = SYSCFG->EXTICR[GPIO_PIN(pin_id) >> 2U];
    CLEAR_BIT(temp, ((uint32_t)0x0FU) << (4U * (GPIO_PIN(pin_id) & 0x03U)));
    SET_BIT(temp, (GPIO_PORT(pin_id)) << (4 * (GPIO_PIN(pin_id) & 0x03U)));
    SYSCFG->EXTICR[GPIO_PIN(pin_id) >> 2U] = temp;

    uint32_t exti_line = 1 << GPIO_PIN(pin_id);
    /* First Disable Event on provided Lines */
    LL_EXTI_DisableEvent_0_31(exti_line);
    /* Then Enable IT on provided Lines */
    LL_EXTI_EnableIT_0_31(exti_line);

    switch(event_mask)
    {
      case GPIO_RISING_EDGE:
        LL_EXTI_DisableFallingTrig_0_31(exti_line);
        LL_EXTI_EnableRisingTrig_0_31(exti_line);
        break;
      case GPIO_FALLING_EDGE:
        LL_EXTI_DisableRisingTrig_0_31(exti_line);
        LL_EXTI_EnableFallingTrig_0_31(exti_line);
        break;
      case (GPIO_RISING_EDGE | GPIO_FALLING_EDGE):
        LL_EXTI_EnableRisingTrig_0_31(exti_line);
        LL_EXTI_EnableFallingTrig_0_31(exti_line);
        break;
      case 0:
        LL_EXTI_DisableIT_0_31(exti_line);
        break;
      default:
        assert(false);
        break;
    }
    err = SUCCESS;
  //}

  end_atomic();
  return err;
}


__LINK_C error_t hw_gpio_enable_interrupt(pin_id_t pin_id)
{
  __HAL_GPIO_EXTI_CLEAR_IT(1 << GPIO_PIN(pin_id));

  uint32_t exti_line = 1 << GPIO_PIN(pin_id);
  LL_EXTI_EnableIT_0_31(exti_line);

#if defined(STM32L0)
  if(GPIO_PIN(pin_id) <= 1) {
    HAL_NVIC_SetPriority(EXTI0_1_IRQn, 2, 0); // TODO on boot
    HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);
    return SUCCESS;
  } else if (GPIO_PIN(pin_id) > 1 && GPIO_PIN(pin_id) <= 3) {
    HAL_NVIC_SetPriority(EXTI2_3_IRQn, 2, 0); // TODO on boot
    HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);
    return SUCCESS;
  } else if (GPIO_PIN(pin_id) > 3 && GPIO_PIN(pin_id) <= 15) {
    HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0); // TODO on boot
    HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);
    return SUCCESS;
  } else {
    assert(false);
  }
}
#elif defined(STM32L1)
  if(GPIO_PIN(pin_id) <= 4) {
    HAL_NVIC_SetPriority(EXTI0_IRQn + GPIO_PIN(pin_id), 2, 0); // TODO on boot
    HAL_NVIC_EnableIRQ(EXTI0_IRQn + GPIO_PIN(pin_id));
    return SUCCESS;
  } else if (GPIO_PIN(pin_id) <= 9) {
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 2, 0); // TODO on boot
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    return SUCCESS;
  } else if (GPIO_PIN(pin_id) > 9 && GPIO_PIN(pin_id) <= 15) {
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 2, 0); // TODO on boot
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    return SUCCESS;
  } else {
    assert(false);
  }
}
#else
  #error "STM32 family not supported"
#endif

__LINK_C error_t hw_gpio_disable_interrupt(pin_id_t pin_id)
{
  //TODO: check if no other pins are still using the interrupt
  /*
   if(GPIO_PIN(pin_id) <= 1) {
    HAL_NVIC_DisableIRQ(EXTI0_1_IRQn);
    return SUCCESS;
  } else if (GPIO_PIN(pin_id) >= 2 && GPIO_PIN(pin_id) < 4) {
    HAL_NVIC_DisableIRQ(EXTI2_3_IRQn);
    return SUCCESS;
  } else if (GPIO_PIN(pin_id) >= 4 && GPIO_PIN(pin_id) < 16) {
    HAL_NVIC_DisableIRQ(EXTI4_15_IRQn);
    return SUCCESS;
  }
  */

	uint32_t exti_line = 1 << GPIO_PIN(pin_id);
	LL_EXTI_DisableIT_0_31(exti_line);

	return SUCCESS;
}

void EXTI0_1_IRQHandler(void)
{
  // will check the different pins here instead of using the HAL
  uint8_t pin_id = 0;
  for (;pin_id <= 1; pin_id++)
  {
    uint16_t pin = 1 << pin_id;
    if(__HAL_GPIO_EXTI_GET_IT(pin) != RESET)
    {
      __HAL_GPIO_EXTI_CLEAR_IT(pin);
      gpio_int_callback(pin_id);
      return;
    }
  }
}

void EXTI2_3_IRQHandler(void)
{
  // will check the different pins here instead of using the HAL

  uint8_t pin_id = 2;
  for (;pin_id <= 3; pin_id++)
  {
    uint16_t pin = 1 << pin_id;
    if(__HAL_GPIO_EXTI_GET_IT(pin) != RESET)
    {
      __HAL_GPIO_EXTI_CLEAR_IT(pin);
      gpio_int_callback(pin_id);
      return;
    }
  }
}

void EXTI4_15_IRQHandler(void)
{
  // will check the different pins here instead of using the HAL

  uint8_t pin_id = 4;
  for (;pin_id <= 15; pin_id++)
  {
    uint16_t pin = 1 << pin_id;
    if(__HAL_GPIO_EXTI_GET_IT(pin) != RESET)
    {
      __HAL_GPIO_EXTI_CLEAR_IT(pin);
      gpio_int_callback(pin_id);
      return;
    }
  }
}
