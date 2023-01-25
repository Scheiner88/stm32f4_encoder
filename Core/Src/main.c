#include "main.h"
#include "stm32f4xx.h"

uint8_t pps_flag = 0; //Создание переменной флаг

int Clock_Init(void);
static void TIM2_Init(void);
static void GPIO_Init(void);

void clear_pps_flag(void);
int get_pps_flag(void);

int main(void)
{
	Clock_Init();
	TIM2_Init();
	GPIO_Init();

	while(1)
	{
		if(get_pps_flag())
		{
			LL_GPIO_TogglePin(GPIOG, LL_GPIO_PIN_14);
			clear_pps_flag();
		}
	}
}

int Clock_Init(void)
{
	__IO int StartUpCounter;

	//ЗАПУСК КВАРЦЕВОГО ГЕНЕРАТОРА:

	RCC->CR |= (1<<RCC_CR_HSEON_Pos); //Запускаем генератор HSE

	//Ждем успешного запуска или окончания тайм-аута
	for(StartUpCounter = 0; ; StartUpCounter++)
	{
		//Если успешно запустилось, то выходим из цикла
		if(RCC->CR & (1<<RCC_CR_HSERDY_Pos)) break;

		//Если не запустилось, то отключаем все, что включили
		if(StartUpCounter > 0x1000) RCC->CR &= ~(1<<RCC_CR_HSEON_Pos);
	}

	//НАСТРОЙКА И ЗАПУСК PLL:
	//Частота кварца 8 MHz
	//f_{PLL general clock output} = [(HSE_VALUE/PLLM)*PLLN]/PLLP

	//Устанавливаем PLLM = 8 <---> (00 1000)
	RCC->PLLCFGR |= (RCC_PLLCFGR_PLLM_3);
	RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLM_1 | RCC_PLLCFGR_PLLM_2 | RCC_PLLCFGR_PLLM_5 | RCC_PLLCFGR_PLLM_0 | RCC_PLLCFGR_PLLM_4);

	//Устанавливаем PLLN = 144 <---> (0 1001 0000)
	RCC->PLLCFGR |= (RCC_PLLCFGR_PLLN_4 | RCC_PLLCFGR_PLLN_7);
	RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLN_0 | RCC_PLLCFGR_PLLN_1 | RCC_PLLCFGR_PLLN_2 | RCC_PLLCFGR_PLLN_3 | RCC_PLLCFGR_PLLN_5 | RCC_PLLCFGR_PLLN_6 | RCC_PLLCFGR_PLLN_8);

	//Устанавливаем PLLP = 2 <---> (00)
	RCC->PLLCFGR &= ~(RCC_PLLCFGR_PLLP_0 | RCC_PLLCFGR_PLLP_1);

	RCC->PLLCFGR |= (1<<RCC_PLLCFGR_PLLSRC_Pos);   //Тактирование PLL от HSE

	RCC->CR |= (1<<RCC_CR_PLLON_Pos);              //Запускаем PLL

	//Ждем успешного запуска или окончания тайм-аута
	for(StartUpCounter = 0; ; StartUpCounter++)
	{
		if(RCC->CR & (1<<RCC_CR_PLLRDY_Pos)) break;

		if(StartUpCounter > 0x1000)
		{
			RCC->CR &= ~(1<<RCC_CR_HSEON_Pos);
			RCC->CR &= ~(1<<RCC_CR_PLLON_Pos);
		}
	}

	// НАСТРОЙКА FLASH И ДЕЛИТЕЛЕЙ:

	//Устанавливаем 2 цикла ожидания для Flash
	FLASH->ACR |= (0x02<<FLASH_ACR_LATENCY_Pos);

	RCC->CFGR |= (0x00<<RCC_CFGR_PPRE2_Pos) //Делитель шины APB2 равен 1
		| (0x04<<RCC_CFGR_PPRE1_Pos)        //Делитель шины APB1 равен 2
		| (0x00<<RCC_CFGR_HPRE_Pos);        //Делитель AHB равен 1


	RCC->CFGR |= (0x02<<RCC_CFGR_SW_Pos);   //Переключаемся на работу от PLL

	//Ждем, пока переключимся
	while((RCC->CFGR & RCC_CFGR_SWS_Msk) != (0x02<<RCC_CFGR_SWS_Pos)){ }

	//После того, как переключились на внешний источник такирования отключаем внутренний RC-генератор (HSI) для экономии энергии
	RCC->CR &= ~(1<<RCC_CR_HSION_Pos);

	return 0;
}

static void TIM2_Init(void)
{
	LL_TIM_InitTypeDef TIM_InitStruct = {0};   //Создание массива? с определёнными заранее переменными и их типами данных

	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2); //Включение таймера

	//Инициализация прерывания
	NVIC_SetPriority(TIM2_IRQn, NVIC_EncodePriority(NVIC_GetPriorityGrouping(),0, 0));
	NVIC_EnableIRQ(TIM2_IRQn);

	TIM_InitStruct.Prescaler = 7200;                          // Предделитель входной частоты (72 Мгц/Prescaler)
	TIM_InitStruct.CounterMode = LL_TIM_COUNTERMODE_UP;       // Счёт вверх
	TIM_InitStruct.Autoreload = 10000 - 1;                    // Количество тактов таймера до перезагрузки
	TIM_InitStruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1; // Дополнительный делитель для формирования дополнительного тактового сигнала
	LL_TIM_Init(TIM2, &TIM_InitStruct);                       // Проверка настроек таймера
	LL_TIM_DisableARRPreload(TIM2);                           // Отключение предварительной загрузки автоматической перезагрузки (ARR)
	LL_TIM_SetClockSource(TIM2, LL_TIM_CLOCKSOURCE_INTERNAL); // Установка тактирования от внутреннего источника (шина APB1)
	LL_TIM_SetTriggerOutput(TIM2, LL_TIM_TRGO_UPDATE);        // Событие обновления используется в качестве выходного сигнала триггера
	LL_TIM_DisableMasterSlaveMode(TIM2);                      // Отключение режима Master/Slave.

	LL_TIM_EnableIT_UPDATE(TIM2); // Включение прерывания по событию UPDATE
	LL_TIM_EnableCounter(TIM2);   // Включение счётчика таймера
}

static void GPIO_Init(void)
{
	LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOH);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOG);

	/**/
	LL_GPIO_ResetOutputPin(GPIOG, LL_GPIO_PIN_14);

	/**/
	GPIO_InitStruct.Pin = LL_GPIO_PIN_14;
	GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
	GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

int get_pps_flag(void) // Функция, возвращающая значение флага
{
	return pps_flag;
}

void clear_pps_flag(void) // Функция очистки флага
{
	pps_flag = 0;
}

void TIM2_Callback(void) // Функция, вызываемая в случае прерывания
{
	LL_TIM_ClearFlag_UPDATE(TIM2);
	pps_flag = 1;
}

void TIM2_IRQHandler(void)
{
	TIM2_Callback();
}
