/*
 * Copyright 2014-2019 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Utils.h"

#include "prt/Callbacks.h"

#include <vector>


class AttrEvalCallbacks: public prt::Callbacks {
public:
	explicit AttrEvalCallbacks(AttributeMapBuilderVector& ambs, const std::vector<RuleFileInfoUPtr>& ruleFileInfo) : mAMBS(ambs), mRuleFileInfo(ruleFileInfo) { }
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

#if (PRT_VERSION_MAJOR > 1 && PRT_VERSION_MINOR > 0)
	prt::Status attrBoolArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const bool* ptr, size_t size) override { return prt::STATUS_OK; }
	prt::Status attrFloatArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const double* ptr, size_t size) override { return prt::STATUS_OK; }
	prt::Status attrStringArray(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* const* ptr, size_t size) override { return prt::STATUS_OK; }
#endif

private:
	AttributeMapBuilderVector& mAMBS;
	const std::vector<RuleFileInfoUPtr>& mRuleFileInfo;
};
