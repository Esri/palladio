#include "ShapeConverter.h"
#include "AttributeConversion.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "GU/GU_Detail.h"
#include "GEO/GEO_PrimPolySoup.h"
#include "UT/UT_String.h"

#include "boost/variant.hpp"


namespace {

constexpr bool DBG = false;

} // namespace


void ShapeConverter::get(const GU_Detail* detail, const PrimitiveClassifier& primCls,
                         ShapeData& shapeData, const PRTContextUPtr& prtCtx)
{
	WA("all");

	assert(shapeData.isValid());

	// -- partition primitives into initial shapes by primitive classifier values
	PrimitivePartition primPart(detail, primCls);
	const PrimitivePartition::PartitionMap& shapePartitions = primPart.get();

	// -- copy all coordinates
	std::vector<double> coords;
	assert(detail->getPointRange().getEntries() == detail->getNumPoints());
	coords.reserve(detail->getNumPoints()*3);
	GA_Offset ptoff;
	GA_FOR_ALL_PTOFF(detail, ptoff) {
		UT_Vector3 p = detail->getPos3(ptoff);
		if (DBG) LOG_DBG << "coords " << coords.size()/3 << ": " << p.x() << ", " << p.y() << ", " << p.z();
		coords.push_back(static_cast<double>(p.x()));
		coords.push_back(static_cast<double>(p.y()));
		coords.push_back(static_cast<double>(p.z()));
	}

	// -- loop over all primitive partitions and create shape builders
	shapeData.mInitialShapeBuilders.reserve(shapePartitions.size());
	uint32_t isIdx = 0;
	for (auto pIt = shapePartitions.begin(); pIt != shapePartitions.end(); ++pIt, ++isIdx) {
		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pIt->second.size();

		// merge primitive geometry inside partition (potential multi-polygon initial shape)
		std::vector<uint32_t> indices, faceCounts, holes;
		for (const auto& prim: pIt->second) {
			if (DBG) LOG_DBG << "   -- prim index " << prim->getMapIndex() << ", type: " << prim->getTypeName() << ", id = " << prim->getTypeId().get();
			const auto& primType = prim->getTypeId();
			if (primType == GA_PRIMPOLY || primType == GA_PRIMPOLYSOUP) {
				const GA_Size vtxCnt = prim->getVertexCount();
				faceCounts.push_back(static_cast<uint32_t>(vtxCnt));
				for (GA_Size i = vtxCnt - 1; i >= 0; i--) {
					indices.push_back(static_cast<uint32_t>(prim->getPointIndex(i)));
					if (DBG) LOG_DBG << "      vtx " << i << ": point idx = " << prim->getPointIndex(i);
				}
			}
		} // for each polygon

		InitialShapeBuilderUPtr isb(prt::InitialShapeBuilder::create());

		isb->setGeometry(coords.data(), coords.size(),
		                 indices.data(), indices.size(),
		                 faceCounts.data(), faceCounts.size(),
		                 holes.data(), holes.size());

		shapeData.mInitialShapeBuilders.emplace_back(std::move(isb));
		shapeData.mPrimitiveMapping.emplace_back(pIt->second);
	} // for each primitive partition

	assert(shapeData.isValid());
}

void ShapeConverter::put(GU_Detail* detail, const PrimitiveClassifier& primCls, const ShapeData& shapeData) const {
	WA("all");

    // TODO: factor out
	GA_RWAttributeRef clsAttrNameRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_PRIM_CLS_NAME, 1));
	GA_RWHandleS clsAttrNameH(clsAttrNameRef);

	GA_RWAttributeRef clsTypeRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, CE_PRIM_CLS_TYPE, 1));
	GA_RWHandleI clsTypeH(clsTypeRef);

	GA_RWAttributeRef clsNameRef;
	using HandleType = boost::variant<GA_RWHandleF, GA_RWHandleI, GA_RWHandleS>;
	HandleType clsNameH;
	switch (primCls.type) {
		case GA_STORECLASS_FLOAT:
			clsNameRef = GA_RWAttributeRef(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, primCls.name, 1));
			clsNameH = GA_RWHandleF(clsNameRef);
			break;
		case GA_STORECLASS_INT:
			clsNameRef = GA_RWAttributeRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, primCls.name, 1));
			clsNameH = GA_RWHandleI(clsNameRef);
			break;
		case GA_STORECLASS_STRING:
			clsNameRef = GA_RWAttributeRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, primCls.name, 1));
			clsNameH = GA_RWHandleS(clsNameRef);
			break;
		default:
			break;
	}

	// setup main attributes handles (potentially overwrite existing attributes)
	// TODO: factor this out into a MainAttributeHandler or such
	GA_RWAttributeRef rpkRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_RPK, 1));
	GA_RWHandleS rpkH(rpkRef);

	GA_RWAttributeRef ruleFileRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_RULE_FILE, 1));
	GA_RWHandleS ruleFileH(ruleFileRef);

	GA_RWAttributeRef startRuleRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_START_RULE, 1));
	GA_RWHandleS startRuleH(startRuleRef);

	GA_RWAttributeRef styleRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_STYLE, 1));
	GA_RWHandleS styleH(styleRef);

	GA_RWAttributeRef seedRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_SEED, 1));
	GA_RWHandleI seedH(seedRef);

	// generate primitive attribute handles from all default rule attribute names from all initial shapes
	std::map<std::string, GA_RWAttributeRef> mAttrRefs;
	AttributeMapVector defaultRuleAttributeMaps;
	defaultRuleAttributeMaps.reserve(shapeData.mRuleAttributeBuilders.size());
	for (auto& amb: shapeData.mRuleAttributeBuilders) {
		defaultRuleAttributeMaps.emplace_back(amb->createAttributeMap());
		const auto& dra = defaultRuleAttributeMaps.back();

		size_t keyCount = 0;
		const wchar_t* const* cKeys = dra->getKeys(&keyCount);
		for (size_t k = 0; k < keyCount; k++) {
			const wchar_t* key = cKeys[k];
			const std::string nKey = toOSNarrowFromUTF16(key);

			// make sure to only generate an attribute handle once
			if (mAttrRefs.count(nKey) > 0)
				continue;

			UT_String primAttrName = NameConversion::toPrimAttr(nKey);

			switch (dra->getType(key)) {
				case prt::AttributeMap::PT_FLOAT: {
					GA_RWAttributeRef ar(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, primAttrName, 1));
					if (ar.isValid())
						mAttrRefs.emplace(nKey, ar);
					break;
				}
				case prt::AttributeMap::PT_BOOL: {
					mAttrRefs.emplace(nKey, detail->addIntTuple(GA_ATTRIB_PRIMITIVE, primAttrName, 1));
					break;
				}
				case prt::AttributeMap::PT_STRING: {
					mAttrRefs.emplace(nKey, detail->addStringTuple(GA_ATTRIB_PRIMITIVE, primAttrName, 1));
					break;
				}
				default:
					break;
			} // switch type
		} // for rule attribute
	} // for each initial shape

	for (size_t isIdx = 0; isIdx < shapeData.mRuleAttributeBuilders.size(); isIdx++) {
		const auto& pv = shapeData.mPrimitiveMapping[isIdx];
		const auto& defaultRuleAttributes = defaultRuleAttributeMaps[isIdx];

		for (auto& prim: pv) {
			const GA_Offset &off = prim->getMapOffset();
			clsAttrNameH.set(off, primCls.name);
			clsTypeH.set(off, primCls.type);

			// put main attributes
			// TODO: factor this out into a MainAttributeHandler or such
			rpkH.set(off, mRPK.string().c_str());
			ruleFileH.set(off, toOSNarrowFromUTF16(mRuleFile).c_str());
			startRuleH.set(off, toOSNarrowFromUTF16(mStartRule).c_str());
			styleH.set(off, toOSNarrowFromUTF16(mStyle).c_str());
			seedH.set(off, mSeed);

			size_t keyCount = 0;
			const wchar_t *const *cKeys = defaultRuleAttributes->getKeys(&keyCount);
			for (size_t k = 0; k < keyCount; k++) {
				const wchar_t *const key = cKeys[k];
				const std::string nKey = toOSNarrowFromUTF16(key);

				GA_RWAttributeRef attrRef = mAttrRefs.at(nKey);
				switch (defaultRuleAttributes->getType(key)) {
					case prt::AttributeMap::PT_FLOAT: {
						GA_RWHandleD av(attrRef);
						if (av.isValid()) {
							const double defVal = defaultRuleAttributes->getFloat(key);
							av.set(off, defVal);
						}
						break;
					}
					case prt::AttributeMap::PT_BOOL: {
						GA_RWHandleI av(attrRef);
						const bool defVal = defaultRuleAttributes->getBool(key);
						av.set(off, defVal ? 1 : 0);
						break;
					}
					case prt::AttributeMap::PT_STRING: {
						GA_RWHandleS av(attrRef);
						const wchar_t* const defVal = defaultRuleAttributes->getString(key);
						const std::string nDefVal = toOSNarrowFromUTF16(defVal); // !!!
						av.set(off, nDefVal.c_str());
						break;
					}
					default: {
						LOG_ERR << "Array attribute support not implemented yet";
					}
				} // switch type
			} // for default rule attribute keys
		} // for all primitives in initial shape
	} // for all initial shapes
}

RuleFileInfoUPtr ShapeConverter::getRuleFileInfo(const ResolveMapUPtr& resolveMap, prt::Cache* prtCache) const {
	const auto cgbURI = resolveMap->getString(mRuleFile.c_str());
	if (cgbURI == nullptr)
		return {};

	prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
	return RuleFileInfoUPtr(prt::createRuleFileInfo(cgbURI, prtCache, &status));
}

std::wstring ShapeConverter::getFullyQualifiedStartRule() const {
	return mStyle + L'$' + mStartRule;
}
