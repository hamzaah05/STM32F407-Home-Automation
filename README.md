# STM32F407 Home Automation Firmware

![Hardware Setup](https://example.com/hardware-photo.jpg)

## Introduction
This project implements a home automation system using the STM32F407 microcontroller. It controls three lights (LT1, LT2, LT3) and a fan through Bluetooth communication with an HC‑05 module. A 16x2 LCD provides real‑time feedback, displaying the ON/OFF status of each device. The firmware ensures reliability and persistence by saving states into RTC backup registers, so the configuration is restored automatically after reset or power loss.

## System Design and Register Configuration
The design integrates multiple STM32F407 peripherals, configured directly through register manipulation for transparency and control.

- **Clock Enable**:  
  - `RCC->AHB1ENR` enables GPIOC and GPIOD clocks.  
  - `RCC->APB2ENR` enables USART6 clock.  
  - `RCC->APB1ENR` enables PWR clock for backup domain.  

- **GPIO Configuration**:  
  - `GPIOx->MODER` sets pins as output or alternate function.  
  - `GPIOx->OTYPER` ensures push‑pull outputs.  
  - `GPIOx->OSPEEDR` sets high‑speed operation.  
  - `GPIOx->AFR[0]` configures PC6/PC7 as AF8 for USART6.  
  - `GPIOx->ODR` writes ON/OFF states to lights, fan, and LCD pins.  

- **USART6 Configuration**:  
  - `USART6->BRR` sets baud rate (integer + fractional).  
  - `USART6->CR1` enables TX, RX, RXNE interrupt, and USART.  
  - `USART6->SR` status register used to check RXNE flag.  
  - `USART6->DR` holds received/transmitted data.  
  - NVIC functions configure interrupt priority and enable IRQ.  

- **SysTick Timer**:  
  - `SysTick->LOAD` sets reload value for 1 ms tick.  
  - `SysTick->VAL` resets counter.  
  - `SysTick->CTRL` enables SysTick with interrupt.  
  - Used for millisecond delays in LCD and command timing.  

- **RTC Backup Registers**:  
  - `PWR->CR` with DBP bit enables backup domain access.  
  - `RTC->BKP0R–RTC->BKP3R` store LT1, LT2, LT3, and FAN states.  
  - On startup, values are restored and written to GPIO outputs.  

## Command Grammar
| Command | Action              |
|---------|---------------------|
| `1`     | Turn ON LT1         |
| `2`     | Turn OFF LT1        |
| `3`     | Turn ON LT2         |
| `4`     | Turn OFF LT2        |
| `5`     | Turn ON LT3         |
| `6`     | Turn OFF LT3        |
| `F`     | Turn ON Fan         |
| `f`     | Turn OFF Fan        |

Commands are received via USART6 interrupt when `USART6->SR` sets RXNE. The character is read from `USART6->DR`, processed, and corresponding GPIO outputs are updated. States are saved in RTC backup registers and reflected on the LCD.

## LCD Layout

LCD commands are sent in 4‑bit mode by writing to PC2–PC5 and toggling EN (PC1). RS (PC0) selects command/data mode. Timing is controlled using SysTick delays.

[## Video Demonstration
[![Watch the demo](hardware-thumbnail.png)](https://drive.google.com/your-video-link)](https://drive.google.com/file/d/1npZ_dO4amtHcNS8yN931VfEGnY4YUg1J/view?usp=sharing)
