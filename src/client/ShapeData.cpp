#include "ShapeData.h"
#include "PrimitivePartition.h"

#include "GU/GU_Detail.h"

#include "boost/variant.hpp"
#include "boost/algorithm/string.hpp"


namespace {

constexpr bool DBG = false;

const UT_String CE_SHAPE_RPK        = "ceShapeRPK";
const UT_String CE_SHAPE_RULE_FILE  = "ceShapeRuleFile";
const UT_String CE_SHAPE_START_RULE = "ceShapeStartRule";
const UT_String CE_SHAPE_STYLE      = "ceShapeStyle";
const UT_String CE_SHAPE_SEED       = "ceShapeSeed";

} // namespace


namespace p4h {

SharedShapeData::SharedShapeData() : mSeed{0} { }

void SharedShapeData::get(const GU_Detail* detail, ShapeData& shapeData, const PRTContextUPtr& prtCtx) {
	// partition primitives into initial shapes by shape classifier values
	PrimitivePartition primPart(detail, mShapeClsAttrName, mShapeClsType);
	const PrimitivePartition::PartitionMap& shapePartitions = primPart.get();

	// loop over all primitive partitions and create proto shapes
	shapeData.mInitialShapeBuilders.reserve(shapePartitions.size());
	uint32_t isIdx = 0;
	for (auto pIt = shapePartitions.begin(); pIt != shapePartitions.end(); ++pIt, ++isIdx) {
		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pIt->second.size();

		// merge primitive geometry inside partition (potential multi-polygon initial shape)
		std::vector<double> vtx;
		std::vector<uint32_t> idx, faceCounts, holes;
		for (const auto& prim: pIt->second) {
			if (DBG) {
				LOG_DBG << "   -- prim index " << prim->getMapIndex();
				LOG_DBG << prim->getTypeName() << ", id = " << prim->getTypeId().get();
			}

			if (prim->getTypeId() != GA_PRIMPOLYSOUP && prim->getTypeId() != GA_PRIMPOLY)
				continue;

			// naively copying vertices
			// prt does not support re-using a vertex index
			for (GA_Size i = prim->getVertexCount() - 1; i >= 0; i--) {
				idx.push_back(static_cast<uint32_t>(vtx.size() / 3));
				UT_Vector3 p = detail->getPos3(prim->getPointOffset(i));
				vtx.push_back(static_cast<double>(p.x()));
				vtx.push_back(static_cast<double>(p.y()));
				vtx.push_back(static_cast<double>(p.z()));
				if (DBG) LOG_DBG << "      vtx idx " << i << ": " << idx.back();
			}
			faceCounts.push_back(static_cast<uint32_t>(prim->getVertexCount()));
		} // for each polygon

		InitialShapeBuilderUPtr isb(prt::InitialShapeBuilder::create());

		isb->setGeometry(
			vtx.data(), vtx.size(),
			idx.data(), idx.size(),
			faceCounts.data(), faceCounts.size(),
			holes.data(), holes.size());

		// store the builder and its corresponding Houdini detail primitives
		shapeData.mInitialShapeBuilders.emplace_back(std::move(isb));
		shapeData.mPrimitiveMapping.emplace_back(pIt->second);
	} // for each primitive partition
}

void SharedShapeData::put(GU_Detail* detail, ShapeData& shapeData) {
	// setup main attributes handles (potentially overwrite existing attributes)
    GA_RWAttributeRef clsAttrNameRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_CLS_NAME, 1));
	GA_RWHandleS clsAttrNameH(clsAttrNameRef);

	GA_RWAttributeRef clsTypeRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_CLS_TYPE, 1));
	GA_RWHandleI clsTypeH(clsTypeRef);

	GA_RWAttributeRef clsNameRef;
	using HandleType = boost::variant<GA_RWHandleF, GA_RWHandleI, GA_RWHandleS>;
	HandleType clsNameH;
	switch (mShapeClsType) {
		case GA_STORECLASS_FLOAT:
			clsNameRef = GA_RWAttributeRef(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, mShapeClsAttrName.c_str(), 1));
			clsNameH = GA_RWHandleF(clsNameRef);
			break;
		case GA_STORECLASS_INT:
			clsNameRef = GA_RWAttributeRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, mShapeClsAttrName.c_str(), 1));
			clsNameH = GA_RWHandleI(clsNameRef);
			break;
		case GA_STORECLASS_STRING:
			clsNameRef = GA_RWAttributeRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, mShapeClsAttrName.c_str(), 1));
			clsNameH = GA_RWHandleS(clsNameRef);
			break;
		default:
			break;
	}

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
	std::map<std::wstring, GA_RWAttributeRef> mAttrRefs;
	std::set<std::string> defaultRuleAttributeNames;
	AttributeMapVector defaultRuleAttributeMaps;
	defaultRuleAttributeMaps.reserve(shapeData.mRuleAttributeBuilders.size());
	for (auto& amb: shapeData.mRuleAttributeBuilders) {
		defaultRuleAttributeMaps.emplace_back(amb->createAttributeMap());
		const auto& dra = defaultRuleAttributeMaps.back();

		size_t keyCount = 0;
		const wchar_t* const* cKeys = dra->getKeys(&keyCount);
		for (size_t k = 0; k < keyCount; k++) {
			const wchar_t* key = cKeys[k];
			std::string nKey = utils::toOSNarrowFromUTF16(key);

			// strip away style prefix
	        auto styleDelimPos = nKey.find('$');
	        if (styleDelimPos != std::string::npos)
	            nKey.erase(0, styleDelimPos+1);

			// dots are not allowed in houdini primitive attribute names
			boost::replace_all(nKey, ".", "_dot_"); // TODO: make this robust

			// make sure to only generate an attribute handle once
			if (!defaultRuleAttributeNames.insert(nKey).second)
				continue;

			switch (dra->getType(key)) {
				case prt::AttributeMap::PT_FLOAT: {
					GA_RWAttributeRef ar(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
					if (ar.isValid())
						mAttrRefs.emplace(key, ar);
					break;
				}
				case prt::AttributeMap::PT_BOOL: {
					mAttrRefs.emplace(key, detail->addIntTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
					break;
				}
				case prt::AttributeMap::PT_STRING: {
					mAttrRefs.emplace(key, detail->addStringTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
					break;
				}
				default:
					break;
			} // switch type
		} // for rule attribute
	} // for each initial shape

	for (size_t isIdx = 0; isIdx < shapeData.mInitialShapeBuilders.size(); isIdx++) {
		const auto& pv = shapeData.mPrimitiveMapping[isIdx];
		const auto& defaultRuleAttributes = defaultRuleAttributeMaps[isIdx];

		for (auto& prim: pv) {
			const GA_Offset &off = prim->getMapOffset();
			clsAttrNameH.set(off, mShapeClsAttrName.c_str());
			clsTypeH.set(off, mShapeClsType);

			rpkH.set(off, mRPK.string().c_str());
			ruleFileH.set(off, utils::toOSNarrowFromUTF16(mRuleFile).c_str());
			startRuleH.set(off, utils::toOSNarrowFromUTF16(mStartRule).c_str());
			styleH.set(off, utils::toOSNarrowFromUTF16(mStyle).c_str());
			seedH.set(off, mSeed);

			size_t keyCount = 0;
			const wchar_t *const *cKeys = defaultRuleAttributes->getKeys(&keyCount);
			for (size_t k = 0; k < keyCount; k++) {
				const wchar_t *const key = cKeys[k];
				std::string nKey = utils::toOSNarrowFromUTF16(key);

				// strip away style prefix
				const auto styleDelimPos = nKey.find('$');
				if (styleDelimPos != std::string::npos)
					nKey.erase(0, styleDelimPos + 1);

				switch (defaultRuleAttributes->getType(key)) {
					case prt::AttributeMap::PT_FLOAT: {
						GA_RWHandleF av(mAttrRefs.at(key)); // TODO: we should stay in double precision here
						if (av.isValid()) {
							const double defVal = defaultRuleAttributes->getFloat(key);
							av.set(off, (fpreal32) defVal); // TODO: again, stay in double precision
						}
						break;
					}
					case prt::AttributeMap::PT_BOOL: {
						GA_RWHandleI av(mAttrRefs.at(key));
						const bool defVal = defaultRuleAttributes->getBool(key);
						av.set(off, defVal ? 1 : 0);
						break;
					}
					case prt::AttributeMap::PT_STRING: {
						GA_RWHandleS av(mAttrRefs.at(key));
						const wchar_t *const defVal = defaultRuleAttributes->getString(key);
						const std::string nDefVal = utils::toOSNarrowFromUTF16(defVal); // !!!
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

} // namespace p4h

