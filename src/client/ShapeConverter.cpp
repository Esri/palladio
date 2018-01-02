#include "ShapeConverter.h"
#include "PrimitiveClassifier.h"
#include "AttributeConversion.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "GU/GU_Detail.h"
#include "GEO/GEO_PrimPolySoup.h"
#include "UT/UT_String.h"

#include "boost/variant.hpp"


namespace {

constexpr bool DBG = false;

template<typename T>
T safeGet(GA_Offset off, const GA_ROAttributeRef& ref) {
	GA_ROHandleS h(ref);
	const char* s = h.get(off);
	return (s != nullptr) ? T{s} : T{};
}

} // namespace


struct MainAttributeHandles {
	GA_RWHandleS rpk;
	GA_RWHandleS ruleFile;
	GA_RWHandleS startRule;
	GA_RWHandleS style;
	GA_RWHandleI seed;

	void setup(GU_Detail* detail) {
		GA_RWAttributeRef rpkRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_RPK, 1));
		rpk.bind(rpkRef);

		GA_RWAttributeRef ruleFileRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_RULE_FILE, 1));
		ruleFile.bind(ruleFileRef);

		GA_RWAttributeRef startRuleRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_START_RULE, 1));
		startRule.bind(startRuleRef);

		GA_RWAttributeRef styleRef(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_STYLE, 1));
		style.bind(styleRef);

		GA_RWAttributeRef seedRef(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, CE_SHAPE_SEED, 1));
		seed.bind(seedRef);
	}
};


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

void ShapeConverter::put(GU_Detail* detail, PrimitiveClassifier& primCls, const ShapeData& shapeData) const {
	WA("all");

	primCls.setupAttributeHandles(detail);

	MainAttributeHandles mah;
	mah.setup(detail);

	// generate primitive attribute handles from all default rule attribute names from all initial shapes
	AttributeMapVector defaultRuleAttributeMaps;
	defaultRuleAttributeMaps.reserve(shapeData.mRuleAttributeBuilders.size());
	std::map<const wchar_t*, GA_RWAttributeRef> mAttrRefs; // key points to defaultRuleAttributeMaps
	for (auto& amb: shapeData.mRuleAttributeBuilders) {
		defaultRuleAttributeMaps.emplace_back(amb->createAttributeMap());
		const auto& dra = defaultRuleAttributeMaps.back();

		size_t keyCount = 0;
		const wchar_t* const* cKeys = dra->getKeys(&keyCount);
		for (size_t k = 0; k < keyCount; k++) {
			const wchar_t* key = cKeys[k];

			// make sure to only generate an attribute handle once
			if (mAttrRefs.count(key) > 0)
				continue;

			const UT_StringHolder primAttrName = NameConversion::toPrimAttr(key);

			switch (dra->getType(key)) {
				case prt::AttributeMap::PT_FLOAT: {
					GA_RWAttributeRef ar(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, primAttrName, 1));
					if (ar.isValid())
						mAttrRefs.emplace(key, ar);
					break;
				}
				case prt::AttributeMap::PT_BOOL: {
					mAttrRefs.emplace(key, detail->addIntTuple(GA_ATTRIB_PRIMITIVE, primAttrName, 1));
					break;
				}
				case prt::AttributeMap::PT_STRING: {
					mAttrRefs.emplace(key, detail->addStringTuple(GA_ATTRIB_PRIMITIVE, primAttrName, 1));
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
			primCls.put(prim);
			putMainAttributes(mah, prim);

			const GA_Offset& off = prim->getMapOffset();

			size_t keyCount = 0;
			const wchar_t* const* cKeys = defaultRuleAttributes->getKeys(&keyCount);
			for (size_t k = 0; k < keyCount; k++) {
				const wchar_t* const key = cKeys[k];

				const auto& attrRef = mAttrRefs.at(key);
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

void ShapeConverter::getMainAttributes(SOP_Node* node, const OP_Context& context) {
	const fpreal now = context.getTime();

	// -- rule package path
	UT_String utNextRPKStr;
	node->evalString(utNextRPKStr, AssignNodeParams::RPK.getToken(), 0, now);
	mRPK = utNextRPKStr.toStdString();

	// -- rule file
	UT_String utRuleFile;
	node->evalString(utRuleFile, AssignNodeParams::RULE_FILE.getToken(), 0, now);
	mRuleFile = toUTF16FromOSNarrow(utRuleFile.toStdString());

	// -- rule style
	UT_String utStyle;
	node->evalString(utStyle, AssignNodeParams::STYLE.getToken(), 0, now);
	mStyle = toUTF16FromOSNarrow(utStyle.toStdString());

	// -- start rule
	UT_String utStartRule;
	node->evalString(utStartRule, AssignNodeParams::START_RULE.getToken(), 0, now);
	mStartRule = toUTF16FromOSNarrow(utStartRule.toStdString());

	// -- random seed
	mSeed = node->evalInt(AssignNodeParams::SEED.getToken(), 0, now);
}

bool ShapeConverter::getMainAttributes(const GU_Detail* detail, const GA_Primitive* prim) {
	GA_ROAttributeRef rpkRef(detail->findPrimitiveAttribute(CE_SHAPE_RPK));
	if (rpkRef.isInvalid())
		return false;

	GA_ROAttributeRef ruleFileRef(detail->findPrimitiveAttribute(CE_SHAPE_RULE_FILE));
	if (ruleFileRef.isInvalid())
		return false;

	GA_ROAttributeRef startRuleRef(detail->findPrimitiveAttribute(CE_SHAPE_START_RULE));
	if (startRuleRef.isInvalid())
		return false;

	GA_ROAttributeRef styleRef(detail->findPrimitiveAttribute(CE_SHAPE_STYLE));
	if (styleRef.isInvalid())
		return false;

	GA_ROAttributeRef seedRef(detail->findPrimitiveAttribute(CE_SHAPE_SEED));
	if (seedRef.isInvalid())
		return false;

	const GA_Offset firstOffset = prim->getMapOffset();
	mRPK       = safeGet<boost::filesystem::path>(firstOffset, rpkRef);
	mRuleFile  = toUTF16FromOSNarrow(safeGet<std::string>(firstOffset, ruleFileRef));
	mStartRule = toUTF16FromOSNarrow(safeGet<std::string>(firstOffset, startRuleRef));
	mStyle     = toUTF16FromOSNarrow(safeGet<std::string>(firstOffset, styleRef));

	GA_ROHandleI seedH(seedRef);
	mSeed = seedH.get(firstOffset);

	return true;
}

void ShapeConverter::putMainAttributes(MainAttributeHandles& mah, const GA_Primitive* primitive) const {
	const GA_Offset &off = primitive->getMapOffset();
	mah.rpk.set(off, mRPK.string().c_str());
	mah.ruleFile.set(off, toOSNarrowFromUTF16(mRuleFile).c_str());
	mah.startRule.set(off, toOSNarrowFromUTF16(mStartRule).c_str());
	mah.style.set(off, toOSNarrowFromUTF16(mStyle).c_str());
	mah.seed.set(off, mSeed);
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
