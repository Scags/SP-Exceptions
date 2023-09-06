
#include <exception>

public void OnPluginStart()
{
	try
	{
		PrintToServer("Hello, ");
		LoadFromAddress(view_as< Address >(0), NumberType_Int32);
		PrintToServer("world!")
	}
	catch
	{
		PrintToServer("Sike!");
	}
}
