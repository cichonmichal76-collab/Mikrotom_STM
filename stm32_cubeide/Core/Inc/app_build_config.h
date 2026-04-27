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

/*
 * Binary UART protocol v2 is introduced as an optional transport. Keep the
 * existing ASCII protocol as the default until the PC agent supports the new
 * framing end-to-end.
 */
#ifndef APP_PROTOCOL_BINARY_V2
#define APP_PROTOCOL_BINARY_V2 0
#endif

#ifndef APP_PROTOCOL_BINARY_V2_TELEMETRY
#define APP_PROTOCOL_BINARY_V2_TELEMETRY APP_PROTOCOL_BINARY_V2
#endif

/*
 * Current bring-up assumption: communication and command acceptance are
 * considered validated enough to shift diagnostics toward the execution path
 * (encoder/current/FOC/phase order). These flags do not change motion logic by
 * themselves; they make the active diagnostic focus explicit and queryable.
 */
#ifndef APP_DIAG_ASSUME_COMMUNICATION_OK
#define APP_DIAG_ASSUME_COMMUNICATION_OK 1
#endif

#ifndef APP_DIAG_ASSUME_COMMAND_ACCEPTANCE_OK
#define APP_DIAG_ASSUME_COMMAND_ACCEPTANCE_OK 1
#endif

#if APP_DIAG_ASSUME_COMMUNICATION_OK && APP_DIAG_ASSUME_COMMAND_ACCEPTANCE_OK
#define APP_DIAG_EXECUTION_FOCUS 1
#else
#define APP_DIAG_EXECUTION_FOCUS 0
#endif

#endif
