#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#pragma GCC diagnostic ignored "-Wattributes"
#include "SOP/SOP_Node.h"
#pragma GCC diagnostic pop


namespace prt {
class CacheObject;
class AttributeMap;
}


namespace prt4hdn {


class SOP_PRT : public SOP_Node {
public:
	static OP_Node* myConstructor(OP_Network*, const char*, OP_Operator*);

public:
	SOP_PRT(OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOP_PRT();

protected:
	void createInitialShape(const GA_Group* group, void* ctx);
	virtual OP_ERROR cookMySop(OP_Context &context);

private:
	prt::CacheObject* mPRTCache;

	const prt::AttributeMap* mHoudiniEncoderOptions;;
	const prt::AttributeMap* mCGAPrintOptions;
	const prt::AttributeMap* mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	std::vector<const prt::AttributeMap*> mAllEncoderOptions;
};


} // namespace prt4hdn
