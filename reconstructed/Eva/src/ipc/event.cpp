/*
 * event.cpp  -  see include/event.h. CEvent::sm_oEvBuffersPool lives here;
 * CEvent::~CEvent()/CLinkedEvent's own trivial members are inline in the header.
 *
 * CLinkedEvent::sm_oEventsPool used to be (mis-typed) defined here too -- it now
 * lives in events_pool.cpp next to the corrected CEventsPool type it actually is,
 * see event.h's own header-comment correction (CParamTracer family pass, 2026-07-28).
 */

#include "event.h"

CEvBuffersPool CEvent::sm_oEvBuffersPool;
