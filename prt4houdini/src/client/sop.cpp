#include "SOPAssign.h"
#include "SOPGenerate.h"
#include "PRTContext.h"
#include "parameter.h"

#include "OP/OP_OperatorTable.h"
#include "UT/UT_Exit.h"
#include "UT/UT_DSOVersion.h"


namespace {

// prt lifecycle
p4h::PRTContextUPtr prtCtx;

} // namespace


void newSopOperator(OP_OperatorTable *table) {
	assert(!prtCtx);
	prtCtx.reset(new p4h::PRTContext());
	UT_Exit::addExitCallback([](void*){ prtCtx.reset(); });

	if (!prtCtx->mLicHandle)
		return;

	// instantiate assign sop
	auto createSOPAssign = [](OP_Network *net, const char *name, OP_Operator *op) -> OP_Node* {
		return new p4h::SOPAssign(prtCtx, net, name, op);
	};
	table->addOperator(new OP_Operator("ceAssign", "ceAssign", createSOPAssign,
			p4h::ASSIGN_NODE_PARAM_TEMPLATES, 1, 1, nullptr, OP_FLAG_GENERATOR
	));

	// instantiate generator sop
	auto createSOPGenerate = [](OP_Network *net, const char *name, OP_Operator *op) -> OP_Node* {
		return new p4h::SOPGenerate(prtCtx, net, name, op);
	};
	table->addOperator(new OP_Operator("ceGenerate", "ceGenerate", createSOPGenerate,
	       p4h::GENERATE_NODE_PARAM_TEMPLATES, 1, 1, nullptr, OP_FLAG_GENERATOR
	));
}
