#include "InitialShapeGenerator.h"

#include "GU/GU_Detail.h"
#include "GA/GA_Primitive.h"

#include "boost/algorithm/string.hpp"


namespace {

constexpr bool DBG = true;

} // namespace


namespace p4h {

InitialShapeGenerator::InitialShapeGenerator(const PRTContextUPtr& prtCtx, const GU_Detail* detail) {
	createInitialShapes(prtCtx, detail);
}

InitialShapeGenerator::~InitialShapeGenerator() {
	std::for_each(mIS.begin(), mIS.end(), [](const prt::InitialShape* is) { is->destroy(); });
}

bool extractMainAttributes(SharedShapeData& ssd, const GA_Primitive* prim, const GU_Detail* detail) {
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
	ssd.mRuleFile = utils::toUTF16FromOSNarrow(ruleFile);

	GA_ROHandleS startRuleH(startRuleRef);
	const std::string startRule = startRuleH.get(firstOffset);
	ssd.mStartRule = utils::toUTF16FromOSNarrow(startRule);

	GA_ROHandleS styleH(styleRef);
	const std::string style = styleH.get(firstOffset);
	ssd.mStyle = utils::toUTF16FromOSNarrow(style);

	GA_ROHandleI seedH(seedRef);
	ssd.mSeed = seedH.get(firstOffset);

	return true;
}

void InitialShapeGenerator::createInitialShapes(
		const PRTContextUPtr& prtCtx,
		const GU_Detail* detail
) {
	if (DBG) LOG_DBG << "-- createInitialShapes";

	// extract initial shape geometry
	SharedShapeData ssd;
	ssd.get(detail, shapeData, prtCtx);

	// collect all primitive attributes
	std::vector<std::pair<GA_ROAttributeRef, std::wstring>> attributes;
	{
		GA_Attribute* a;
		GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(detail, a) {
			attributes.emplace_back(GA_ROAttributeRef(a), utils::toUTF16FromOSNarrow(a->getName().toStdString()));
		}
	}

	// loop over all initial shapes and use the first primitive to get the attribute values
	for (size_t isIdx = 0; isIdx < shapeData.mInitialShapeBuilders.size(); isIdx++) {
		const auto& pv = shapeData.mPrimitiveMapping[isIdx];
		const auto& firstPrimitive = pv.front();
		const auto& primitiveMapOffset = firstPrimitive->getMapOffset();

		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pv.size();

		// extract main attrs from first prim in initial shape prim group
		if (!extractMainAttributes(ssd, firstPrimitive, detail))
			continue;

		const ResolveMapUPtr& assetsMap = prtCtx->getResolveMap(ssd.mRPK);
		if (!assetsMap)
			continue;

		// extract primitive attributes
		shapeData.mRuleAttributeBuilders.emplace_back(prt::AttributeMapBuilder::create());
		auto& amb = shapeData.mRuleAttributeBuilders.back();
		for (size_t k = 0; k < attributes.size(); k++) {
			const GA_ROAttributeRef& ar = attributes[k].first;
			const std::wstring key      = boost::replace_all_copy(attributes[k].second, L"_dot_", L"."); // TODO

			if (ar.isInvalid())
				continue;

			switch (ar.getStorageClass()) {
				case GA_STORECLASS_FLOAT: {
					GA_ROHandleD av(ar);
					if (av.isValid()) {
						double v = av.get(primitiveMapOffset);
						if (DBG)
							if (key == L"distanceToCenter")
								LOG_DBG << "   prim float attr: " << ar->getName() << " = " << v;
						amb->setFloat(key.c_str(), v);
					}
					break;
				}
				case GA_STORECLASS_STRING: {
					GA_ROHandleS av(ar);
					if (av.isValid()) {
						const char* v = av.get(primitiveMapOffset);
						const std::wstring wv = utils::toUTF16FromOSNarrow(v);
						//if (DBG) LOG_DBG << "   prim string attr: " << ar->getName() << " = " << v;
						amb->setString(key.c_str(), wv.c_str());
					}
					break;
				}
				case GA_STORECLASS_INT: {
					GA_ROHandleI av(ar);
					if (av.isValid()) {
						const int v = av.get(primitiveMapOffset);
						const bool bv = (v > 0);
						//if (DBG) LOG_DBG << "   prim bool attr: " << ar->getName() << " = " << v;
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
		const auto fqStartRule = ssd.getFullyQualifiedStartRule();
		isb->setAttributes(
				ssd.mRuleFile.c_str(),
				fqStartRule.c_str(),
				ssd.mSeed,
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
		LOG_DBG << utils::objectToXML(initialShape);

		mIS.push_back(initialShape);

		if (DBG) LOG_DBG << p4h::utils::objectToXML(initialShape);
	} // for each partition
}

} // namespace p4h
