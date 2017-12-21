#include "ShapeGenerator.h"
#include "PrimitivePartition.h"
#include "PrimitiveClassifier.h"
#include "AttributeConversion.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "GA/GA_Primitive.h"
#include "GU/GU_Detail.h"

#include <unordered_map>


namespace {

constexpr bool DBG = false;

const std::set<UT_StringHolder> ATTRIBUTE_BLACKLIST = { CE_PRIM_CLS_NAME, CE_PRIM_CLS_TYPE, CE_SHAPE_RPK,
                                                        CE_SHAPE_RULE_FILE, CE_SHAPE_START_RULE, CE_SHAPE_STYLE,
                                                        CE_SHAPE_SEED };

} // namespace


void ShapeGenerator::get(const GU_Detail* detail, const PrimitiveClassifier& primCls,
                         ShapeData& shapeData, const PRTContextUPtr& prtCtx)
{
	WA("all");

	// extract initial shape geometry
	ShapeConverter::get(detail, primCls, shapeData, prtCtx);

	// collect all primitive attributes
	std::unordered_map<UT_StringHolder, GA_ROAttributeRef> attributes;
	{
		GA_Attribute* a;
		GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(detail, a) {
			const UT_StringHolder& n = a->getName();

			// do not add the main attributes as normal initial shape attributes
			// else they end up as primitive attributes on the generated geometry
			if (ATTRIBUTE_BLACKLIST.count(n) > 0)
				continue;

			attributes.emplace(n, GA_ROAttributeRef(a));
		}

		// also filter out the actual primitive classifier attribute
		std::set<UT_StringHolder> removeMe;
		const GA_Primitive* p;
		GA_FOR_ALL_PRIMITIVES(detail, p) {
			PrimitiveClassifier pc;
			primCls.updateFromPrimitive(pc, detail, p);
			if (removeMe.emplace(pc.name).second) {
				auto it = attributes.find(pc.name);
				if (it != attributes.end()) {
					if (it->second->getStorageClass() == pc.type)
						attributes.erase(it);
				}
			}
		}
	}

	// loop over all initial shapes and use the first primitive to get the attribute values
	for (size_t isIdx = 0; isIdx < shapeData.mInitialShapeBuilders.size(); isIdx++) {
		const auto& pv = shapeData.mPrimitiveMapping[isIdx];
		if (pv.empty())
			continue;

		const auto& firstPrimitive = pv.front();
		const auto& primitiveMapOffset = firstPrimitive->getMapOffset();

		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pv.size();

		// extract main attrs from first prim in initial shape prim group
		if (!getMainAttributes(detail, firstPrimitive))
			continue;

		const ResolveMapUPtr& assetsMap = prtCtx->getResolveMap(mRPK);
		if (!assetsMap)
			continue;

		// extract primitive attributes
		AttributeMapBuilderUPtr amb(prt::AttributeMapBuilder::create());
		for (const auto& attr: attributes) {
			const GA_ROAttributeRef& ar = attr.second;

			if (ar.isInvalid())
				continue;

			const std::wstring ruleAttrName = NameConversion::toRuleAttr(mStyle, attr.first);

			switch (ar.getStorageClass()) {
				case GA_STORECLASS_FLOAT: {
					GA_ROHandleD av(ar);
					if (av.isValid()) {
						double v = av.get(primitiveMapOffset);
						if (DBG) LOG_DBG << "   prim float attr: " << ar->getName() << " = " << v;
						amb->setFloat(ruleAttrName.c_str(), v);
					}
					break;
				}
				case GA_STORECLASS_STRING: {
					GA_ROHandleS av(ar);
					if (av.isValid()) {
						const char* v = av.get(primitiveMapOffset);
						const std::wstring wv = toUTF16FromOSNarrow(v);
						if (DBG) LOG_DBG << "   prim string attr: " << ar->getName() << " = " << v;
						amb->setString(ruleAttrName.c_str(), wv.c_str());
					}
					break;
				}
				case GA_STORECLASS_INT: {
					GA_ROHandleI av(ar);
					if (av.isValid()) {
						const int v = av.get(primitiveMapOffset);
						const bool bv = (v > 0);
						if (DBG) LOG_DBG << "   prim bool attr: " << ar->getName() << " = " << v;
						amb->setBool(ruleAttrName.c_str(), bv);
					}
					break;
				}
				default: {
					LOG_WRN << "prim attr " << ar->getName() << ": unsupported storage class";
					break;
				}
			} // switch key type
		} // for each primitive attribute

		const std::wstring shapeName = L"shape_" + std::to_wstring(isIdx);

		AttributeMapUPtr ruleAttr(amb->createAttributeMap());

		auto& isb = shapeData.mInitialShapeBuilders[isIdx];
		const auto fqStartRule = getFullyQualifiedStartRule();
		isb->setAttributes(
				mRuleFile.c_str(),
				fqStartRule.c_str(),
				mSeed,
				shapeName.c_str(),
				ruleAttr.get(),
				assetsMap.get()
		);

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = isb->createInitialShapeAndReset(&status);
		if (status == prt::STATUS_OK && initialShape != nullptr) {
			if (DBG) LOG_DBG << objectToXML(initialShape);
			shapeData.mInitialShapes.emplace_back(initialShape);
			shapeData.mRuleAttributeBuilders.emplace_back(std::move(amb));
			shapeData.mRuleAttributes.emplace_back(std::move(ruleAttr));
		}
		else
			LOG_WRN << "failed to create initial shape " << shapeName << ": " << prt::getStatusDescription(status);

	} // for each partition
}
