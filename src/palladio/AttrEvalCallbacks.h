#pragma once

#include "Utils.h"

#include "prt/Callbacks.h"


class AttrEvalCallbacks: public prt::Callbacks {
public:
	explicit AttrEvalCallbacks(AttributeMapBuilderVector& ambs, const RuleFileInfoUPtr& ruleFileInfo) : mAMBS(ambs), mRuleFileInfo(ruleFileInfo) { }
	~AttrEvalCallbacks() override = default;

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
	AttributeMapBuilderVector& mAMBS;
	const RuleFileInfoUPtr& mRuleFileInfo;
};
