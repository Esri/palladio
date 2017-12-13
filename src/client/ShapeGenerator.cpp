#include "ShapeGenerator.h"
#include "LogHandler.h"

#include "GU/GU_Detail.h"
#include "GA/GA_Primitive.h"


namespace {

constexpr bool DBG = false;

// TODO: factor this out into a MainAttributeHandler or such
bool extractMainAttributes(ShapeConverter& ssd, const GA_Primitive* prim, const GU_Detail* detail) {
	GA_ROAttributeRef rpkRef(detail->findPrimitiveAttribute("ceShapeRPK"));
	if (rpkRef.isInvalid())
		return false;

	GA_ROAttributeRef ruleFileRef(detail->findPrimitiveAttribute("ceShapeRuleFile"));
	if (ruleFileRef.isInvalid())
		return false;

	GA_ROAttributeRef startRuleRef(detail->findPrimitiveAttribute("ceShapeStartRule"));
	if (startRuleRef.isInvalid())
		return false;

	GA_ROAttributeRef styleRef(detail->findPrimitiveAttribute("ceShapeStyle"));
	if (styleRef.isInvalid())
		return false;

	GA_ROAttributeRef seedRef(detail->findPrimitiveAttribute("ceShapeSeed"));
	if (seedRef.isInvalid())
		return false;

	GA_Offset firstOffset = prim->getMapOffset();

	GA_ROHandleS rpkH(rpkRef);
	const std::string rpk = rpkH.get(firstOffset);
	ssd.mRPK = boost::filesystem::path(rpk);

	GA_ROHandleS ruleFileH(ruleFileRef);
	const std::string ruleFile = ruleFileH.get(firstOffset);
	ssd.mRuleFile = toUTF16FromOSNarrow(ruleFile);

	GA_ROHandleS startRuleH(startRuleRef);
	const std::string startRule = startRuleH.get(firstOffset);
	ssd.mStartRule = toUTF16FromOSNarrow(startRule);

	GA_ROHandleS styleH(styleRef);
	const std::string style = styleH.get(firstOffset);
	ssd.mStyle = toUTF16FromOSNarrow(style);

	GA_ROHandleI seedH(seedRef);
	ssd.mSeed = seedH.get(firstOffset);

	return true;
}

} // namespace


void ShapeGenerator::get(
	const GU_Detail* detail,
	ShapeData& shapeData,
	const PRTContextUPtr& prtCtx
) {
	if (DBG) LOG_DBG << "-- createInitialShapes";

	// extract initial shape geometry
	ShapeConverter::get(detail, shapeData, prtCtx);

	// collect all primitive attributes
	std::vector<std::pair<GA_ROAttributeRef, UT_StringHolder>> attributes;
	{
		GA_Attribute* a;
		GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(detail, a) {
			attributes.emplace_back(GA_ROAttributeRef(a), a->getName());
		}
	}

	// loop over all initial shapes and use the first primitive to get the attribute values
	for (size_t isIdx = 0; isIdx < shapeData.mInitialShapeBuilders.size(); isIdx++) {
		const auto& pv = shapeData.mPrimitiveMapping[isIdx];
		const auto& firstPrimitive = pv.front();
		const auto& primitiveMapOffset = firstPrimitive->getMapOffset();

		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pv.size();

		// extract main attrs from first prim in initial shape prim group
		if (!extractMainAttributes(*this, firstPrimitive, detail))
			continue;

		const ResolveMapUPtr& assetsMap = prtCtx->getResolveMap(mRPK);
		if (!assetsMap)
			continue;

		// extract primitive attributes
		shapeData.mRuleAttributeBuilders.emplace_back(prt::AttributeMapBuilder::create());
		auto& amb = shapeData.mRuleAttributeBuilders.back();
		for (const auto& k: attributes) {
			const GA_ROAttributeRef& ar = k.first;

			if (ar.isInvalid())
				continue;

			const std::string nKey = NameConversion::toRuleAttr(k.second);
			const std::wstring key = toUTF16FromOSNarrow(nKey);

			switch (ar.getStorageClass()) {
				case GA_STORECLASS_FLOAT: {
					GA_ROHandleD av(ar);
					if (av.isValid()) {
						double v = av.get(primitiveMapOffset);
						if (DBG) LOG_DBG << "   prim float attr: " << ar->getName() << " = " << v;
						amb->setFloat(key.c_str(), v);
					}
					break;
				}
				case GA_STORECLASS_STRING: {
					GA_ROHandleS av(ar);
					if (av.isValid()) {
						const char* v = av.get(primitiveMapOffset);
						const std::wstring wv = toUTF16FromOSNarrow(v);
						if (DBG) LOG_DBG << "   prim string attr: " << ar->getName() << " = " << v;
						amb->setString(key.c_str(), wv.c_str());
					}
					break;
				}
				case GA_STORECLASS_INT: {
					GA_ROHandleI av(ar);
					if (av.isValid()) {
						const int v = av.get(primitiveMapOffset);
						const bool bv = (v > 0);
						if (DBG) LOG_DBG << "   prim bool attr: " << ar->getName() << " = " << v;
						amb->setBool(key.c_str(), bv);
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

		shapeData.mRuleAttributes.emplace_back(amb->createAttributeMap());

		auto& isb = shapeData.mInitialShapeBuilders[isIdx];
		const auto fqStartRule = getFullyQualifiedStartRule();
		isb->setAttributes(
				mRuleFile.c_str(),
				fqStartRule.c_str(),
				mSeed,
				shapeName.c_str(),
				shapeData.mRuleAttributes.back().get(),
				assetsMap.get()
		);

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = isb->createInitialShapeAndReset(&status);
		if (status != prt::STATUS_OK) {
			LOG_WRN << "failed to create initial shape: " << prt::getStatusDescription(status);
			return;
		}
		LOG_DBG << objectToXML(initialShape);

		shapeData.mInitialShapes.emplace_back(initialShape);

		if (DBG) LOG_DBG << objectToXML(initialShape);
	} // for each partition
}
