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

#include "TestCallbacks.h"

#include "../palladio/PRTContext.h"
#include "../palladio/Utils.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

struct GenerateData { // TODO: could use ShapeData from production code
	InitialShapeBuilderVector mInitialShapeBuilders;
	InitialShapeNOPtrVector mInitialShapes;

	AttributeMapBuilderVector mRuleAttributeBuilders;
	AttributeMapVector mRuleAttributes;

	~GenerateData() {
		std::for_each(mInitialShapes.begin(), mInitialShapes.end(), [](const prt::InitialShape* is) {
			assert(is != nullptr);
			is->destroy();
		});
	}
};

void generate(TestCallbacks& tc, const PRTContextUPtr& prtCtx, const std::filesystem::path& rpkPath,
              const std::wstring& ruleFile, const std::vector<std::wstring>& initialShapeURIs,
              const std::vector<std::wstring>& startRules);
