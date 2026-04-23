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

#endif
