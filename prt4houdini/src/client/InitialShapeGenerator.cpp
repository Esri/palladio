#include "client/InitialShapeGenerator.h"
#include "client/SOPAssign.h"

#include "GU/GU_Detail.h"
#include "GU/GU_PrimPoly.h"
#include "GU/GU_HoleInfo.h"
#include "OP/OP_Operator.h"
#include "OP/OP_OperatorTable.h"
#include "OP/OP_Director.h"

#include "boost/dynamic_bitset.hpp"
#include "boost/variant.hpp"
#include "boost/algorithm/string.hpp"


namespace {

const bool PDBG = false;
const int32 INVALID_CLS_VALUE{-1};

struct PrimitivePartition {
	typedef boost::variant<UT_String, fpreal32, int32> ClassifierValueType;
	typedef std::vector<const GA_Primitive*> PrimitiveVector;
	typedef std::map<ClassifierValueType, PrimitiveVector> PartitionMap;

	PrimitivePartition(GA_ROAttributeRef clsAttrNameRef, GA_ROAttributeRef clsAttrTypeRef)
			: mClsAttrNameRef(clsAttrNameRef), mClsAttrTypeRef(clsAttrTypeRef) {
		if (PDBG)
			LOG_DBG << "   -- prim part: partition attr = " << (mClsAttrNameRef.isValid() ? mClsAttrTypeRef->getName() : "<invalid>");
	}

	void add(const GA_Detail* detail, const GA_Primitive* p) {
		if (PDBG) LOG_DBG << "      adding prim: " << detail->primitiveIndex(p->getMapOffset());

		GA_ROHandleS clsAttrNameH(mClsAttrNameRef);
		UT_String clsAttrName = clsAttrNameH.get(p->getMapOffset());

		GA_ROHandleI clsAttrTypeH(mClsAttrTypeRef);
		GA_StorageClass clsAttrType = static_cast<GA_StorageClass>(clsAttrTypeH.get(p->getMapOffset()));

		GA_ROAttributeRef clsNameRef;
		GA_ROAttributeRef r(detail->findPrimitiveAttribute(clsAttrName.buffer()));
		if (r.isValid() && r->getStorageClass() == clsAttrType)
			clsNameRef = r;

		if (clsNameRef.isInvalid()) {
			mPrimitives[INVALID_CLS_VALUE].push_back(p);
			if (PDBG) LOG_DBG << "       missing cls name: adding prim to fallback shape!";
		}
		else if ((clsAttrType == GA_STORECLASS_FLOAT) && clsNameRef.isFloat()) {
			GA_ROHandleF av(clsNameRef);
			if (av.isValid()) {
				fpreal32 v = av.get(p->getMapOffset());
				if (PDBG) LOG_DBG << "        got float classifier value: " << v;
				mPrimitives[v].push_back(p);
			}
			else {
				if (PDBG) LOG_DBG << "        float: invalid handle!";
			}
		}
		else if ((clsAttrType == GA_STORECLASS_INT) && clsNameRef.isInt()) {
			GA_ROHandleI av(clsNameRef);
			if (av.isValid()) {
				int32 v = av.get(p->getMapOffset());
				if (PDBG) LOG_DBG << "        got int classifier value: " << v;
				mPrimitives[v].push_back(p);
			}
			else {
				if (PDBG) LOG_DBG << "        int: invalid handle!";
			}
		}
		else if ((clsAttrType == GA_STORECLASS_STRING) && clsNameRef.isString()) {
			GA_ROHandleS av(clsNameRef);
			if (av.isValid()) {
				const char* v = av.get(p->getMapOffset());
				if (v) {
					if (PDBG) LOG_DBG << "        got string classifier value: " << v;
					mPrimitives[UT_String(v)].push_back(p);
				}
				else {
					mPrimitives[INVALID_CLS_VALUE].push_back(p);
					LOG_WRN << "shape classifier attribute has empty string value -> fallback shape";
				}
			}
			else {
				if (PDBG) LOG_DBG << "        string: invalid handle!";
			}
		}
		else {
			if (PDBG) LOG_DBG << "        wrong type!";
		}
	}

	const PartitionMap& get() const {
		return mPrimitives;
	}

	GA_ROAttributeRef mClsAttrNameRef;
	GA_ROAttributeRef mClsAttrTypeRef;
	PartitionMap mPrimitives;
};

} // namespace


namespace p4h {

InitialShapeGenerator::InitialShapeGenerator(const PRTContextUPtr& prtCtx, const GU_Detail* detail)
: mISB(prt::InitialShapeBuilder::create()) {
	createInitialShapes(prtCtx, detail);
}

InitialShapeGenerator::~InitialShapeGenerator() {
	std::for_each(mIS.begin(), mIS.end(), [](const prt::InitialShape* is) { is->destroy(); });
	std::for_each(mISAttrs.begin(), mISAttrs.end(), [](const prt::AttributeMap* am) { am->destroy(); });
}

void InitialShapeGenerator::createInitialShapes(const PRTContextUPtr& prtCtx, const GU_Detail* detail) {
	static const bool DBG = true;
	if (DBG) LOG_DBG << "-- createInitialShapes";

	GA_ROAttributeRef clsAttrName = InitialShapeContext::getClsName(detail);
	GA_ROAttributeRef clsAttrType = InitialShapeContext::getClsType(detail);

	// partition primitives by shapeClsAttrRef value
	// in case shapeClsAttrRef is invalid, all prims end up in the same initial shape
	PrimitivePartition primPart(clsAttrName, clsAttrType);
	const GA_Primitive* prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(detail, prim) {
		primPart.add(detail, prim);
	}

	// treat all primitive attributes as InitialShape attributes
	std::vector<std::pair<GA_ROAttributeRef, std::wstring>> attributes;
	{
		GA_Attribute* a;
		GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(detail, a) {
			attributes.emplace_back(GA_ROAttributeRef(a), utils::toUTF16FromOSNarrow(a->getName().toStdString()));
		}
	}

	// create initial shape for each primitive partition
	boost::dynamic_bitset<> attributeTracker(attributes.size());
	const PrimitivePartition::PartitionMap& pm = primPart.get();
	size_t isIdx = 0;
	for (auto pIt = pm.begin(); pIt != pm.end(); ++pIt, ++isIdx) {
		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pIt->second.size();

		// get main generation attributes
		GA_ROAttributeRef rpkRef(detail->findPrimitiveAttribute("ceShapeRPK"));
		if (rpkRef.isInvalid()) continue;
		GA_ROAttributeRef ruleFileRef(detail->findPrimitiveAttribute("ceShapeRuleFile"));
		if (ruleFileRef.isInvalid()) continue;
		GA_ROAttributeRef startRuleRef(detail->findPrimitiveAttribute("ceShapeStartRule"));
		if (startRuleRef.isInvalid()) continue;
		GA_ROAttributeRef styleRef(detail->findPrimitiveAttribute("ceShapeStyle"));
		if (styleRef.isInvalid()) continue;
		GA_ROAttributeRef seedRef(detail->findPrimitiveAttribute("ceShapeSeed"));
		if (seedRef.isInvalid()) continue;

		GA_Offset firstOffset = pIt->second.front()->getMapOffset();

		GA_ROHandleS rpkH(rpkRef);
		const std::string rpk = rpkH.get(firstOffset);
		const std::wstring wrpk = utils::toUTF16FromOSNarrow(rpk);

		GA_ROHandleS ruleFileH(ruleFileRef);
		const std::string ruleFile = ruleFileH.get(firstOffset);
		const std::wstring wRuleFile = utils::toUTF16FromOSNarrow(ruleFile);

		GA_ROHandleS startRuleH(startRuleRef);
		const std::string startRule = startRuleH.get(firstOffset);

		GA_ROHandleS styleH(styleRef);
		const std::string style = styleH.get(firstOffset);
		const std::wstring wStyle = utils::toUTF16FromOSNarrow(style);

		GA_ROHandleI seedH(seedRef);
		const int32_t seed = seedH.get(firstOffset);

		const std::wstring shapeName = L"shape_" + std::to_wstring(isIdx);
		const std::wstring wStartRule = utils::toUTF16FromOSNarrow(style + "$" + startRule);

		const ResolveMapUPtr& assetsMap = prtCtx->getResolveMap(utils::toFileURI(wrpk));
		if (!assetsMap)
			continue;

		AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());

		// evaluate default attribute values:
		// only incoming attributes which differ from default must be set
		getDefaultRuleAttributeValues(amb, prtCtx->mPRTCache, assetsMap, wRuleFile, wStartRule);
		AttributeMapPtr defaultAttributes(amb->createAttributeMapAndReset());
		LOG_DBG << utils::objectToXML(defaultAttributes.get());

		// get geometry
		std::vector<double> vtx;
		std::vector<uint32_t> idx, faceCounts, holes;

		// merge primitive geometry inside partition
		attributeTracker.reset();
		for (const auto& p: pIt->second) {
			if (DBG) {
				LOG_DBG << "   -- prim index " << p->getMapIndex();
				LOG_DBG << p->getTypeName() << ", id = " << p->getTypeId().get();
			}

			if (p->getTypeId() != GA_PRIMPOLYSOUP && p->getTypeId() != GA_PRIMPOLY)
				continue;

			// naively copying vertices
			// prt does not support re-using a vertex index (revisit this with prt 1.6)
			const GU_PrimPoly* face = static_cast<const GU_PrimPoly*>(p);
			for (GA_Size i = face->getVertexCount()-1; i >= 0; i--) {
				idx.push_back(static_cast<uint32_t>(vtx.size()/3));
				UT_Vector3 p = detail->getPos3(face->getPointOffset(i));
				vtx.push_back(static_cast<double>(p.x()));
				vtx.push_back(static_cast<double>(p.y()));
				vtx.push_back(static_cast<double>(p.z()));
				if (DBG) LOG_DBG << "      vtx idx " << i << ": " << idx.back();
			}
			faceCounts.push_back(static_cast<uint32_t>(face->getFastVertexCount()));

			// extract primitive attributes
			// if multi-prim initial shape: the first encountered value per attribute wins
			for (size_t k = 0; k < attributes.size(); k++) {
				if (!attributeTracker[k]) {
					const GA_ROAttributeRef& ar = attributes[k].first;
					const std::wstring key      = boost::replace_all_copy(attributes[k].second, L"_dot_", L"."); // TODO
					const std::wstring fqKey    = [&wStyle,&key](){ std::wstring w = wStyle;  w += '$'; w += key; return w; }();

					if (ar.isInvalid())
						continue;

					switch (ar.getStorageClass()) {
						case GA_STORECLASS_FLOAT: {
							GA_ROHandleD av(ar);
							if (av.isValid()) {
								double v = av.get(p->getMapOffset());


								if (defaultAttributes->hasKey(fqKey.c_str())) {
									double defVal = defaultAttributes->getFloat(fqKey.c_str());
									if ( (std::abs(v - defVal) < 1e-6) || (std::isnan(v) && std::isnan(defVal)) )
										continue;
								}

								if (DBG) LOG_DBG << "   prim float attr: " << ar->getName() << " = " << v;
								amb->setFloat(key.c_str(), v);
								attributeTracker.set(k);
							}
							break;
						}
						case GA_STORECLASS_STRING: {
							GA_ROHandleS av(ar);
							if (av.isValid()) {
								const char* v = av.get(p->getMapOffset());
								std::wstring wv = utils::toUTF16FromOSNarrow(v);

								if (defaultAttributes->hasKey(fqKey.c_str()) && (wv.compare(defaultAttributes->getString(fqKey.c_str())) == 0))
									continue;

								if (DBG) LOG_DBG << "   prim string attr: " << ar->getName() << " = " << v;
								amb->setString(key.c_str(), wv.c_str());
								attributeTracker.set(k);
							}
							break;
						}
						case GA_STORECLASS_INT: {
							GA_ROHandleI av(ar);
							if (av.isValid()) {
								int v = av.get(p->getMapOffset());
								bool bv = (v > 0);

								if (defaultAttributes->hasKey(fqKey.c_str()) && (bv == defaultAttributes->getBool(fqKey.c_str())))
									continue;

								if (DBG) LOG_DBG << "   prim bool attr: " << ar->getName() << " = " << v;
								amb->setBool(key.c_str(), bv);
								attributeTracker.set(k);
							}
							break;
						}
						default: {
							LOG_WRN << "prim attr " << ar->getName() << ": unsupported storage class";
							break;
						}
					} // switch key type
				} // multi-face inital shape
			} // for each user attr
		} // for each prim inside partition

		if (vtx.empty() || idx.empty() || faceCounts.empty())
			continue;

		mISB->setGeometry(
				vtx.data(), vtx.size(),
				idx.data(), idx.size(),
				faceCounts.data(), faceCounts.size(),
				holes.data(), holes.size()
		);

		const prt::AttributeMap* initialShapeAttrs = amb->createAttributeMap();

		mISB->setAttributes(
				wRuleFile.c_str(),
				wStartRule.c_str(),
				seed,
				shapeName.c_str(),
				initialShapeAttrs,
				assetsMap.get()
		);

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = mISB->createInitialShapeAndReset(&status);
		if (status != prt::STATUS_OK) {
			LOG_WRN << "failed to create initial shape: " << prt::getStatusDescription(status);
			initialShapeAttrs->destroy();
			return;
		}
		mIS.push_back(initialShape);
		mISAttrs.push_back(initialShapeAttrs);

		if (DBG) LOG_DBG << p4h::utils::objectToXML(initialShape);
	} // for each partition
}

} // namespace p4h
