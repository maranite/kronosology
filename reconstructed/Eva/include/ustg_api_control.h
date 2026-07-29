/*
 * ustg_api_control.h  -  USTGAPIControl, real class (confirmed via nm -C
 * `USTGAPIControl::SysLogPrintf(char const*, ...)`). Only SysLogPrintf is
 * reconstructed this pass -- a thin variadic wrapper around vsyslog(3),
 * needed to unblock CDDriverIO::scsi_req_sense's own SysLogPrintf calls
 * (see dd_driver_io.h). Reconstructed 2026-07-29, CDDriverIO follow-up to
 * scsi_driver_base.h.
 */
#ifndef USTG_API_CONTROL_H
#define USTG_API_CONTROL_H

class USTGAPIControl {
public:
	static void SysLogPrintf(const char *fmt, ...);
};

#endif /* USTG_API_CONTROL_H */
