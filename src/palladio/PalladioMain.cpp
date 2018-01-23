#include "SOPAssign.h"
#include "SOPGenerate.h"
#include "PRTContext.h"
#include "NodeParameter.h"

#include "OP/OP_OperatorTable.h"
#include "UT/UT_Exit.h"

#undef major // fixes warning regarding duplicate macros in <sys/types.h> and <sys/sysmacros.h>
#undef minor
#include "UT/UT_DSOVersion.h"


namespace {

// prt lifecycle
PRTContextUPtr prtCtx;

} // namespace


void newSopOperator(OP_OperatorTable *table) {
	assert(!prtCtx);
	prtCtx.reset(new PRTContext());
	UT_Exit::addExitCallback([](void*){ prtCtx.reset(); });

	if (!prtCtx->mLicHandle)
		return;

	// instantiate assign sop
	auto createSOPAssign = [](OP_Network *net, const char *name, OP_Operator *op) -> OP_Node* {
		return new SOPAssign(prtCtx, net, name, op);
	};
	table->addOperator(new OP_Operator("pldAssign", "pldAssign", createSOPAssign,
			AssignNodeParams::PARAM_TEMPLATES, 1, 1, nullptr, OP_FLAG_GENERATOR
	));

	// instantiate generator sop
	auto createSOPGenerate = [](OP_Network *net, const char *name, OP_Operator *op) -> OP_Node* {
		return new SOPGenerate(prtCtx, net, name, op);
	};
	table->addOperator(new OP_Operator("pldGenerate", "pldGenerate", createSOPGenerate,
	       GenerateNodeParams::PARAM_TEMPLATES, 1, 1, nullptr, OP_FLAG_GENERATOR
	));
}
