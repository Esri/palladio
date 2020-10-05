/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
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

#include "LogHandler.h"
#include "PRTContext.h"
#include "ShapeConverter.h"
#include "Utils.h"

#include "SOP/SOP_Node.h"

class SOPGenerate : public SOP_Node {
public:
	SOPGenerate(const PRTContextUPtr& pCtx, OP_Network* net, const char* name, OP_Operator* op);
	~SOPGenerate() override = default;

	void opChanged(OP_EventType reason, void* data = nullptr) override;

protected:
	OP_ERROR cookMySop(OP_Context& context) override;

private:
	bool handleParams(OP_Context& context);

private:
	const PRTContextUPtr& mPRTCtx;

	AttributeMapUPtr mHoudiniEncoderOptions;
	AttributeMapUPtr mCGAPrintOptions;
	AttributeMapUPtr mCGAErrorOptions;
	std::vector<const wchar_t*> mAllEncoders;
	AttributeMapNOPtrVector mAllEncoderOptions;
	AttributeMapUPtr mGenerateOptions;
};
