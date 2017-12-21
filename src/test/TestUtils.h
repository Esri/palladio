#pragma once

#include "TestCallbacks.h"

#include "../client/PRTContext.h"
#include "../client/utils.h"

#include "boost/filesystem.hpp"

#include <vector>
#include <string>


struct GenerateData { // TODO: could use ShapeData from production code
	InitialShapeBuilderVector         mInitialShapeBuilders;
	InitialShapeNOPtrVector           mInitialShapes;

	AttributeMapBuilderVector         mRuleAttributeBuilders;
	AttributeMapVector                mRuleAttributes;

	~GenerateData() {
		std::for_each(mInitialShapes.begin(), mInitialShapes.end(), [](const prt::InitialShape* is){
			assert(is != nullptr);
			is->destroy();
		});
	}
};


void generate(TestCallbacks& tc,
              const PRTContextUPtr& prtCtx,
              const boost::filesystem::path& rpkPath,
              const std::wstring& ruleFile,
              const std::vector<std::wstring>& initialShapeURIs,
              const std::vector<std::wstring>& startRules);
