/*
 * event.cpp  -  see include/event.h. Only the two static pool singletons live here;
 * CEvent::~CEvent()/CLinkedEvent's own trivial members are inline in the header.
 */

#include "event.h"

CEvBuffersPool CEvent::sm_oEvBuffersPool;
CEvBuffersPool CLinkedEvent::sm_oEventsPool;
