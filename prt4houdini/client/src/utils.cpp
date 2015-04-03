#include "utils.h"

#include "prt/API.h"

#ifdef _WIN32
#	include <Windows.h>
#else
#	include <dlfcn.h>
#endif


namespace prt4hdn {
namespace utils {

const prt::AttributeMap* createValidatedOptions(const wchar_t* encID, const prt::AttributeMap* unvalidatedOptions) {
	const prt::EncoderInfo* encInfo = prt::createEncoderInfo(encID);
	const prt::AttributeMap* validatedOptions = 0;
	const prt::AttributeMap* optionStates = 0;
	encInfo->createValidatedOptionsAndStates(unvalidatedOptions, &validatedOptions, &optionStates);
	optionStates->destroy();
	encInfo->destroy();
	return validatedOptions;
}

std::string objectToXML(prt::Object const* obj) {
	if (obj == 0)
		throw std::invalid_argument("object pointer is not valid");
	const size_t siz = 4096;
	size_t actualSiz = siz;
	std::string buffer(siz, ' ');
	obj->toXML((char*)&buffer[0], &actualSiz);
	buffer.resize(actualSiz-1); // ignore terminating 0
	if(siz < actualSiz)
		obj->toXML((char*)&buffer[0], &actualSiz);
	return buffer;
}

void getPathToCurrentModule(boost::filesystem::path& path) {
#ifdef _WIN32
	HMODULE dllHandle = 0;
	if(!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)getPathToCurrentModule, &dllHandle)) {
		DWORD c = GetLastError();
		char msg[255];
		FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, c, 0, msg, 255, 0);
		throw std::runtime_error("error while trying to get current module handle': " + std::string(msg));
	}
	assert(sizeof(TCHAR) == 1);
	const size_t PATHMAXSIZE = 4096;
	TCHAR pathA[PATHMAXSIZE];
	DWORD pathSize = GetModuleFileName(dllHandle, pathA, PATHMAXSIZE);
	if(pathSize == 0 || pathSize == PATHMAXSIZE) {
		DWORD c = GetLastError();
		char msg[255];
		FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, 0, c, 0, msg, 255, 0);
		throw std::runtime_error("error while trying to get current module path': " + std::string(msg));
	}
	path = pathA;
#else /* macosx or linux */
	Dl_info dl_info;
	if(dladdr((void*)getPathToCurrentModule, &dl_info) == 0) {
		char* error = dlerror();
		throw std::runtime_error("error while trying to get current module path': " + std::string(error ? error : ""));
	}
	path = dl_info.dli_fname;
#endif
}

std::string getSharedLibraryPrefix() {
	#if defined(_WIN32)
	return "";
	#elif defined(__APPLE__)
	return "lib";
	#elif defined(linux)
	return "lib";
	#else
	#	error unsupported build platform
	#endif
}

std::string getSharedLibrarySuffix() {
	#if defined(_WIN32)
	return ".dll";
	#elif defined(__APPLE__)
	return ".dylib";
	#elif defined(linux)
	return ".so";
	#else
	#	error unsupported build platform
	#endif
}

}
}
