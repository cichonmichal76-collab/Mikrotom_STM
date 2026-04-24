#ifndef FW_VERSION_H
#define FW_VERSION_H
#define FW_NAME        "MICROTOME_AXIS_FW"
#define FW_VERSION     "0.3.0-integrated"
#define FW_BUILD_DATE  __DATE__ " " __TIME__
#define FW_GIT_HASH    "manual-dev-build"
const char* FwVersion_Name(void);
const char* FwVersion_Version(void);
const char* FwVersion_BuildDate(void);
const char* FwVersion_GitHash(void);
#endif
