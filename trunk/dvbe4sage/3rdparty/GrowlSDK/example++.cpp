/*
	Shared
		cl -I . -w -c example++.cpp -Foexample-shared++.obj
		link example-shared++.obj libgrowl-shared++.lib libgrowl-shared.lib /out:example-shared++.exe ws2_32.lib
	
	Static
		cl -D GROWL_STATIC -D GROWL_CPP_STATIC -I . -w -c example++.cpp -Foexample-static++.obj
		link example-static++.obj libgrowl-static++.lib libgrowl-static.lib /out:example-static++.exe ws2_32.lib
*/

#include <growl++.hpp>

int main(int argc, char **argv)
{
	const char *n[2] = { "alice" , "bob" };
	Growl *growl = new Growl(GROWL_TCP,NULL,"gntp_send++",(const char **const)n,2);
	growl->Notify("bob","title","message");

	delete(growl);

	return 0;
}
