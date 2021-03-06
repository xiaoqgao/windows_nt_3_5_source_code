/***
* istrldbl.cxx - definitions for istream class operator>>(long double) funct
*
*	Copyright (c) 1991-1993, Microsoft Corporation.  All rights reserved.
*
*Purpose:
*	Definitions of operator>>(long double) member function for istream
*	class.
*	[AT&T C++]
*
*Revision History:
*	09-26-91   KRS	Created.  Split off from istream.cxx for granularity.
*	12-30-92   KRS	Fix indirection problem with **endptr.
*	05-10-93   CFW	Re-enable function, use strtod since no _strtold.
*
*******************************************************************************/

#include <cruntime.h>
#include <internal.h>
#include <stdlib.h>
#include <iostream.h>
#pragma hdrstop

#pragma check_stack(on)		// large buffer(s)

// CONSIDER: validate this maximum length
#define MAXLDBLSIZ	32

istream& istream::operator>>(long double& n)
{
_WINSTATIC char ibuffer[MAXLDBLSIZ];
    char ** endptr = (char**)NULL;
    if (ipfx(0))
	{
	if (getdouble(ibuffer, MAXLDBLSIZ)>0)
	    {
	    n = (long double)strtod(ibuffer, endptr);
#if 0
	    if (**endptr)
		{
		//UNDONE: put back any unread characters, if possible
		}
#endif
	    }
        isfx();
	}
return *this;
}
