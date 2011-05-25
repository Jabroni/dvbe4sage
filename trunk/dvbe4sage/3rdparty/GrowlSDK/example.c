/*
	Shared
		cl -I . -w -c example.c -Foexample-shared.obj
		link example-shared.obj libgrowl-shared.lib /out:example-shared.exe ws2_32.lib
	
	Static
		cl -D GROWL_STATIC -D GROWL_CPP_STATIC -I . -w -c example.c -Foexample-static.obj
		link example-static.obj libgrowl-static.lib /out:example-static.exe ws2_32.lib
*/

#include <stdio.h>
#include "growl.h"

int main(int argc, char* argv[])
{
	growl( "localhost" , "app" , "notify1" , "a title" , "a message" , NULL , NULL , NULL );
	return 0;
}
