/* editable.cpp - see include/editable.h */

#include "editable.h"
#include "edit_server.h"

CEditable::CEditable(CEditServer *editServer) : mEditServer(editServer) {}

void CEditable::AddDescriptorsMap(CObjectBase *owner, SDescriptor *descriptors,
                                   bool alreadyRegistered)
{
	int count = 0;
	while (descriptors[count].group != 0xff) {
		++count;
	}

	/* mEditServer's own embedded CDataHandler sits at mEditServer + 4
	 * (edit_server.h's "+0x04..0x40024 mData" layout comment) -- never a
	 * separate allocation.
	 */
	CDataHandler *data = reinterpret_cast<CDataHandler *>(
	    reinterpret_cast<char *>(mEditServer) + 4);
	data->AddDescriptors(owner, descriptors, count, alreadyRegistered);
}
