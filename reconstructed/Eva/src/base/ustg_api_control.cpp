/*
 * ustg_api_control.cpp  -  USTGAPIControl::SysLogPrintf. Ground truth:
 *   void USTGAPIControl::SysLogPrintf(char *param_1,...)
 *   {
 *     vsyslog(4,param_1,&stack0x00000008);
 *   }
 * `4` is LOG_WARNING. `&stack0x00000008` is the varargs cursor right after
 * the format-string argument -- the standard va_start(ap, fmt)/vsyslog/
 * va_end translation.
 */
#include <cstdarg>
#include <syslog.h>

#include "ustg_api_control.h"

void USTGAPIControl::SysLogPrintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsyslog(LOG_WARNING, fmt, ap);
	va_end(ap);
}
