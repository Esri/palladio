#pragma once

#ifdef _WIN32
#	ifdef PLD_TEST_EXPORTS
#		define PLD_TEST_EXPORTS_API __declspec(dllexport)
#	else
#		define PLD_TEST_EXPORTS_API
#	endif
#else
#	define PLD_TEST_EXPORTS_API __attribute__ ((visibility ("default")))
#endif
