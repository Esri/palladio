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

constexpr const char* OP_PLD_ASSIGN   = "pldAssign";
constexpr const char* OP_PLD_GENERATE = "pldGenerate";
