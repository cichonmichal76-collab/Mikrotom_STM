#ifndef APP_BUILD_CONFIG_H
#define APP_BUILD_CONFIG_H

/*
 * Shared build flags for modules that need to know whether the real motion
 * loop is compiled in or the package is running in safe-integration mode.
 */
#ifndef APP_SAFE_INTEGRATION
#define APP_SAFE_INTEGRATION 1
#endif

#if APP_SAFE_INTEGRATION
#define APP_MOTION_IMPLEMENTED 0
#else
#define APP_MOTION_IMPLEMENTED 1
#endif

/*
 * Keep the legacy debug stream disabled by default even when real motion is
 * compiled in. The operator protocol should remain the only USART2 producer.
 */
#ifndef APP_LEGACY_UART_STREAMING
#define APP_LEGACY_UART_STREAMING 0
#endif

/*
 * VBUS is needed by the safety monitor before motion is allowed, so keep the
 * measurement path enabled in both safe-integration and motion builds.
 */
#ifndef APP_VBUS_MEASUREMENT_ENABLED
#define APP_VBUS_MEASUREMENT_ENABLED 1
#endif

#endif
