#ifndef STM32F10X_CONF_H_
#define STM32F10X_CONF_H_

#ifdef  USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0 : assert_failed(__FILE__, __LINE__))
void assert_failed(const char* file, int line);
#else
#define assert_param(expr) ((void)0)
#endif

#endif
