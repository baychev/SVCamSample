#pragma once

#include <windows.h>
#include <vector>
#include <map>
#include <deque>
#include <string>

template <class C> void DSDeleteContainer( C & cntr ) {
    for ( typename C::iterator it = cntr.begin();it != cntr.end(); ++it )
    {
		if (*it )
		{
        delete *it;
        *it = NULL;
		}
		}
    cntr.clear();
}

template <class C> void DSDeleteContainerReverse( C & cntr ) {
    for ( typename C::reverse_iterator rit = cntr.rbegin(); rit != cntr.rend(); ++rit)
    {
        delete *rit;
        *rit = NULL;
    }
    cntr.clear();
}


