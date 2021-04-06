#include "BaseXSession.h"
#include <iostream>

int main()
{
	BaseXSession session;

	try{
		session.open("127.0.0.1", "1984", "admin", "admin");
	}
	catch(BaseXNetworkError& e)
	{
		std::cerr << e.what() << std::endl;
		return 1;
	}

	try{
		std::cout << session.execute("xquery /") << std::endl;
	}
	catch(BaseXNetworkError& e)
	{
		std::cerr << e.what() << std::endl;
		return 2;
	}
	catch(BaseXCommandError& e)
	{
		std::cerr << e.what() << std::endl;
	}

	session.close();
	return 0;
}