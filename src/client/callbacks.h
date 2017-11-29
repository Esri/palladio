#pragma once

#include "logging.h"
#include "utils.h"
#include "../codec/encoder/HoudiniCallbacks.h"

#include "prt/AttributeMap.h"
#include "prt/Callbacks.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif

#include "GEO/GEO_PolyCounts.h"
#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "UT/UT_Vector3.h"
#include "UT/UT_Interrupt.h"

#ifdef P4H_TC_GCC
#	pragma GCC diagnostic pop
#endif

#include <string>
#include <vector>
#include <mutex>


namespace p4h {

class AttrEvalCallbacks: public prt::Callbacks {
public:
	explicit AttrEvalCallbacks(AttributeMapBuilderPtr& amb, const RuleFileInfoPtr& ruleFileInfo) : mAMB(amb), mRuleFileInfo(ruleFileInfo) { }
	virtual ~AttrEvalCallbacks() = default;

	prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message) override;
	prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) override;
	prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) override;
	prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) override;
	prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override;
	prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override;
	prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override;
	prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override;
	prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override;
	prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override;

private:
	AttributeMapBuilderPtr& mAMB;
	const RuleFileInfoPtr& mRuleFileInfo;
};

class HoudiniGeometry : public HoudiniCallbacks {
public:
	explicit HoudiniGeometry(GU_Detail* gdp, UT_AutoInterrupt* autoInterrupt = nullptr);

protected:
	void add(
			const wchar_t* name,
			const double* vtx, size_t vtxSize,
			const double* nrm, size_t nrmSize,
			const double* uvs, size_t uvsSize,
			int32_t* counts, size_t countsSize,
			int32_t* indices, size_t indicesSize,
			uint32_t* uvIndices, size_t uvIndicesSize,
			int32_t* holes, size_t holesSize,
			const prt::AttributeMap** materials, size_t materialsSize,
			const uint32_t* faceRanges
	) override;

	prt::Status generateError(size_t isIndex, prt::Status status, const wchar_t* message) override;
	prt::Status assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) override;
	prt::Status cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) override;
	prt::Status cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) override;
	prt::Status cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override;
	prt::Status cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override;
	prt::Status cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override;
	prt::Status attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) override;
	prt::Status attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) override;
	prt::Status attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) override;

	prt::Callbacks::Continuation progress(float percentageCompleted) override {
		if (mAutoInterrupt && mAutoInterrupt->wasInterrupted()) // TODO: is this thread-safe?
			return prt::Callbacks::CANCEL_AND_FINISH;
		return prt::Callbacks::progress(percentageCompleted);
	}

private:
	GU_Detail* mDetail;
	UT_AutoInterrupt* mAutoInterrupt;
};



} // namespace p4h
