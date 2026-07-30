/* host_stubs.h — force-included (-include) for host builds only.
 *
 * board_config.h names CMSIS peripheral symbols (GPIOA, TIM3, ...) that only
 * exist when building against the real device headers. On the host we only
 * care about the arithmetic, so these stand in as placeholders. Nothing here
 * is ever compiled into firmware for the board.
 */
#ifndef HOST_STUBS_H
#define HOST_STUBS_H
#define GPIOA 0
#define GPIOB 0
#define GPIOC 0
#define GPIOD 0
#define GPIOE 0
#define GPIOF 0
#define TIM1  0
#define TIM2  0
#define TIM3  0
#define TIM4  0
#define TIM5  0
#define TIM8  0
#define TIM15 0
#define TIM16 0
#define TIM17 0
#define TIM20 0
#endif
