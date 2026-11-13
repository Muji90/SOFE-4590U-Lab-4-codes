#ifndef AI_TIMING_H
#define AI_TIMING_H

#include <stdint.h>
#include "stm32f411xe.h"

/*
 * STM32F411 Cortex-M4 DWT cycle timer.
 */

static inline void AI_Timer_Init(void)
{
    CoreDebug->DEMCR |=
        CoreDebug_DEMCR_TRCENA_Msk;

    DWT->CYCCNT = 0;

    DWT->CTRL |=
        DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t AI_Timer_Start(void)
{
    return DWT->CYCCNT;
}

static inline uint32_t AI_Timer_GetCycles(
    uint32_t start
)
{
    return DWT->CYCCNT - start;
}

static inline uint32_t AI_Timer_CyclesToUs(
    uint32_t cycles,
    uint32_t cpu_clock_hz
)
{
    if (cpu_clock_hz == 0)
    {
        return 0;
    }

    return (uint32_t)(
        ((uint64_t)cycles * 1000000ULL)
        / cpu_clock_hz
    );
}

#endif
