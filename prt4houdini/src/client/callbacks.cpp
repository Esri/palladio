#include "callbacks.h"
#include "utils.h"

#include "GU/GU_HoleInfo.h"

#include <chrono>


namespace {
const bool DBG = false;

typedef std::map<std::wstring, GA_RWHandleS> StringHandles; // prt string
typedef std::map<std::wstring, GA_RWHandleI> IntHandles; // prt int32_t -> int32_t
typedef std::map<std::wstring, GA_RWHandleC> BoolHandles; // prt bool -> int8_t
typedef std::map<std::wstring, GA_RWHandleF> FloatHandles; // prt double -> float !!!

struct HandleMaps {
	StringHandles mStrings;
	IntHandles mInts;
	BoolHandles mBools;
	FloatHandles mFloats;
};

void setupHandles(GU_Detail* detail, const prt::AttributeMap* m, HandleMaps& hm) {
	size_t keyCount = 0;
	wchar_t const* const* keys = m->getKeys(&keyCount);
	for(size_t k = 0; k < keyCount; k++) {
		wchar_t const* const key = keys[k];
		std::string nKey = p4h::utils::toOSNarrowFromUTF16(key);
		std::replace(nKey.begin(), nKey.end(), '.', '_');
		if (DBG) LOG_DBG << "nKey = " << nKey;
		switch(m->getType(key)) {
			case prt::Attributable::PT_BOOL: {
				GA_RWHandleC h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1, GA_Defaults(0), nullptr, nullptr, GA_STORE_INT8));
				hm.mBools.insert(std::make_pair(key, h));
				if (DBG) LOG_DBG << "bool: " << h->getName();
				break;
			}
			case prt::Attributable::PT_FLOAT: {
				GA_RWHandleF h(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
				hm.mFloats.insert(std::make_pair(key, h));
				if (DBG) LOG_DBG << "float: " << h->getName();
				break;
			}
			case prt::Attributable::PT_INT: {
				GA_RWHandleI h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
				hm.mInts.insert(std::make_pair(key, h));
				if (DBG) LOG_DBG << "int: " << h->getName();
				break;
			}
			case prt::Attributable::PT_STRING: {
				GA_RWHandleS h(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, nKey.c_str(), 1));
				hm.mStrings.insert(std::make_pair(key, h));
				if (DBG) LOG_DBG << "strings: " << h->getName();
				break;
			}
//			case prt::Attributable::PT_BOOL_ARRAY: {
//				break;
//			}
//			case prt::Attributable::PT_INT_ARRAY: {
//				break;
//			}
//			case prt::Attributable::PT_FLOAT_ARRAY: {
//				break;
//			}
//			case prt::Attributable::PT_STRING_ARRAY:{
//				break;
//			}
			default:
				if (DBG) LOG_DBG << "ignored: " << key;
				break;
		}
	}
}

std::mutex mDetailMutex; // guard the houdini detail object

} // namespace


namespace p4h {

HoudiniGeometry::HoudiniGeometry(GU_Detail* gdp, prt::AttributeMapBuilder* eab, UT_AutoInterrupt* autoInterrupt)
: mDetail(gdp), mEvalAttrBuilder(eab), mAutoInterrupt(autoInterrupt) { }

void HoudiniGeometry::add(
		const wchar_t* name,
		const double* vtx, size_t vtxSize,
		const double* nrm, size_t nrmSize,
		const double* uvs, size_t uvsSize,
		int32_t* counts, size_t countsSize,
		int32_t* indices, size_t indicesSize,
		uint32_t* uvIndices, size_t uvIndicesSize,
		int32_t* holes, size_t holesSize,
		const prt::AttributeMap** materials, size_t materialsSize,
		const uint32_t* faceRanges
) {
	std::lock_guard<std::mutex> guard(mDetailMutex); // protect all mDetail accesses

	std::vector<UT_Vector3F> utPoints; // fpreal32
	utPoints.reserve(vtxSize / 3);
	for (size_t pi = 0; pi < vtxSize; pi += 3)
		utPoints.emplace_back(vtx[pi], vtx[pi+1], vtx[pi+2]); // double -> float

	GEO_PolyCounts geoPolyCounts;
	for (size_t ci = 0; ci < countsSize; ci++)
		geoPolyCounts.append(counts[ci]);

	std::string nname = p4h::utils::toOSNarrowFromUTF16(name);
	GA_PrimitiveGroup* primGroup = dynamic_cast<GA_PrimitiveGroup*>(mDetail->getElementGroupTable(GA_ATTRIB_PRIMITIVE).newGroup(nname.c_str(), false));

	GA_Detail::OffsetMarker marker(*mDetail);
	GA_Offset primStartOffset = GU_PrimPoly::buildBlock(mDetail, utPoints.data(), utPoints.size(), geoPolyCounts, indices);

	if (nrmSize > 0) {
		uint32_t vi = 0;
		GA_RWHandleV3 nrmh(mDetail->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32));
		for (GA_Iterator it(marker.vertexRange()); !it.atEnd(); ++it, ++vi) {
			auto nrmIdx = indices[vi];
			nrmh.set(it.getOffset(), UT_Vector3F(nrm[nrmIdx*3], nrm[nrmIdx*3+1], nrm[nrmIdx*3+2])); // double -> float
		}
	}

	if (uvsSize > 0) {
		GA_RWHandleV3 txth(mDetail->addTextureAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32));
		uint32_t vi = 0;
		for (GA_Iterator it(marker.vertexRange()); !it.atEnd(); ++it, ++vi) {
			const uint32_t uvIdx = uvIndices[vi];
			if (uvIdx < uint32_t(-1)) {
				txth.set(it.getOffset(), UT_Vector3F(uvs[uvIdx*2], uvs[uvIdx*2+1], 0.0f)); // double -> float
			}
		}
		// TODO: multiple UV sets
	}

#if 0 // keep disabled until hole support for initial shapes is completed
	static const int32_t HOLE_DELIM = std::numeric_limits<int32_t>::max();
	std::vector<int32_t> holeFaceIndices;
	int32_t faceIdx = 0;
	for (size_t hi = 0; hi < mHoles.size(); hi++) {
		if (mHoles[hi] == HOLE_DELIM) {
			GEO_Primitive* holeOwner = mDetail->getGEOPrimitiveByIndex(faceIdx);
			for (int32_t hfi: holeFaceIndices) {
				GEO_Primitive* holePrim = mDetail->getGEOPrimitiveByIndex(hfi);
				GU_HoleInfo holeInfo(holePrim);
				holeInfo.setPromotedFace(static_cast<GEO_Face*>(holeOwner));
				holeInfo.setReversed();
			}
			holeFaceIndices.clear();
			faceIdx++;
			continue;
		}
		holeFaceIndices.push_back(mHoles[hi]);
	}
#endif

	primGroup->addRange(marker.primitiveRange());

	if (DBG) LOG_DBG << "materials: materialsSize = " << materialsSize;
	if (materialsSize > 0) {

		// setup houdini attribute handles based on first material keys & types
		HandleMaps hm;
		setupHandles(mDetail, materials[0], hm); // TODO: bad strategy, either scan for all handles or lazily create handles

		// assign attribute values per face range
		for (size_t r = 0; r < materialsSize; r++) {
			GA_Offset rangeStart = primStartOffset + faceRanges[r];
			GA_Offset rangePastEnd = primStartOffset + faceRanges[r + 1]; // faceRanges contains faceRangeCount+1 values
			const prt::AttributeMap* m = materials[r];

			for (auto& h: hm.mStrings) {
				std::string nv;
				const wchar_t* v = m->getString(h.first.c_str());
				if (v && std::wcslen(v) > 0) {
					nv = utils::toOSNarrowFromUTF16(v);
				}
				for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
					h.second.set(off, nv.c_str());
				if (DBG)
					LOG_DBG << "string attr: range = [" << rangeStart << ", " << rangePastEnd << "): " <<
					h.second.getAttribute()->getName() << " = " << nv;
			}

			for (auto& h: hm.mFloats) {
				float v = m->getFloat(h.first.c_str());
				for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
					h.second.set(off, v);
				if (DBG)
					LOG_DBG << "float attr: range = [" << rangeStart << ", " << rangePastEnd << "): " <<
					h.second.getAttribute()->getName() << " = " << v;
			}

			for (auto& h: hm.mInts) {
				int32_t v = m->getInt(h.first.c_str());
				for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
					h.second.set(off, v);
				if (DBG)
					LOG_DBG << "float attr: range = [" << rangeStart << ", " << rangePastEnd << "): " <<
					h.second.getAttribute()->getName() << " = " << v;
			}

			for (auto& h: hm.mBools) {
				bool v = m->getBool(h.first.c_str());
				for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
					h.second.set(off, v ? 1 : 0);
				if (DBG)
					LOG_DBG << "bool attr: range = [" << rangeStart << ", " << rangePastEnd << "): " <<
					h.second.getAttribute()->getName() << " = " << v;
			}
		}
	}
}

prt::Status HoudiniGeometry::generateError(size_t isIndex, prt::Status status, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) {
	//LOG_WRN << key << L": " << message;
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) {
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	return prt::STATUS_OK;
}
prt::Status HoudiniGeometry::cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	return prt::STATUS_OK;
}

prt::Status HoudiniGeometry::attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	if (mEvalAttrBuilder != nullptr) {
		mEvalAttrBuilder->setBool(key, value);
	}
	return prt::STATUS_OK;
}

prt::Status HoudiniGeometry::attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	if (mEvalAttrBuilder != nullptr) {
		mEvalAttrBuilder->setFloat(key, value);
	}
	return prt::STATUS_OK;
}

prt::Status HoudiniGeometry::attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	if (mEvalAttrBuilder != nullptr) {
		mEvalAttrBuilder->setString(key, value);
	}
	return prt::STATUS_OK;
}

}
