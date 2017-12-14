#include "ModelConverter.h"
#include "ShapeConverter.h"
#include "LogHandler.h"
#include "utils.h"

#include "GU/GU_HoleInfo.h"

#include "boost/variant.hpp"

#include <chrono>


namespace {

constexpr bool DBG = false;

/**
 * attribute type conversion from PRT to Houdini:
 * wstring -> narrow string
 * int32_t -> int32_t
 * bool    -> int8_t
 * double  -> float (single precision!)
 */
using NoHandle   = int8_t;
using HandleType = boost::variant<NoHandle, GA_RWBatchHandleS, GA_RWHandleI, GA_RWHandleC, GA_RWHandleF>;

// bound to life time of PRT attribute map
struct ProtoHandle {
	HandleType handleType;
	std::vector<std::wstring> keys;
	prt::AttributeMap::PrimitiveType type; // original PRT type
};

using HandleMap = std::map<UT_String, ProtoHandle>;

void createAttributeHandles(GU_Detail* detail, HandleMap& handleMap) {
	for (auto& hm: handleMap) {
		const auto& utKey = hm.first;
		const auto& type = hm.second.type;
		const size_t tupleSize = hm.second.keys.size();

		HandleType handle; // set to NoHandle by default
		assert(handle.which() == 0);
		switch (type) {
			case prt::Attributable::PT_BOOL: {
				GA_RWHandleC h(
				detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize, GA_Defaults(0), nullptr, nullptr, GA_STORE_INT8));
				if (h.isValid())
					handle = h;
				break;
			}
			case prt::Attributable::PT_FLOAT: {
				GA_RWHandleF h(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize));
				if (h.isValid())
					handle = h;
				break;
			}
			case prt::Attributable::PT_INT: {
				GA_RWHandleI h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize));
				if (h.isValid())
					handle = h;
				break;
			}
			case prt::Attributable::PT_STRING: {
				GA_RWBatchHandleS h(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, utKey, tupleSize));
				if (h.isValid())
					handle = h;
				break;
			}

			// TODO: add support for array attributes

			default:
				if (DBG) LOG_DBG << "ignored: " << utKey;
				break;
		}

		if (handle.which() != 0) {
			hm.second.handleType = handle;
			if (DBG) LOG_DBG << "added attr handle " << utKey << " of type " << handle.type().name();
		}
		else if (DBG) LOG_DBG << "could not update handle for primitive attribute " << utKey;
	}
}

void extractAttributeNames(HandleMap& handleMap, const prt::AttributeMap* attrMap) {
	std::map<std::wstring, std::vector<std::wstring>> candidates;

	// split keys with a dot
	size_t keyCount = 0;
	wchar_t const* const* keys = attrMap->getKeys(&keyCount);
	for(size_t k = 0; k < keyCount; k++) {
		std::wstring key(keys[k]);

		auto p = key.find_first_of(L'.');

		std::wstring primary;
		std::wstring secondary;
		if (p != std::wstring::npos) {
			primary = key.substr(0, p);
			secondary = key.substr(p+1);
		}
		else
			primary = key;

		auto r = candidates.emplace(primary, std::vector<std::wstring>());
		r.first->second.emplace_back(secondary); // allow empty strings
	}

	// detect keys which match the "foo.{r,g,b}" pattern
	for (auto& c: candidates) {
		const auto& primary = c.first;

		bool isGroupable = false;
		{
			if (c.second.size() == 3) {
				std::bitset<3> flags; // r g b found

				// check for identical types
				const auto firstType = attrMap->getType((primary + L"." + c.second.front()).c_str());
				const auto r = std::count_if(c.second.begin(), c.second.end(), [&primary,&firstType,&attrMap](const std::wstring& secondary) {
					const auto t = attrMap->getType((primary + L"." + secondary).c_str());
					return (t == firstType);
				});
				const bool matchingTypes = (r == c.second.size());

				// check for secondary name pattern
				std::for_each(c.second.begin(), c.second.end(), [&flags](const std::wstring& secondary){
					if (secondary == L"r") flags[0] = true;
					if (secondary == L"g") flags[1] = true;
					if (secondary == L"b") flags[2] = true;
				});

				isGroupable = matchingTypes && flags.all();
			}
		}

		if (isGroupable) {
			ProtoHandle ph;
			ph.type = attrMap->getType((primary + L"." + c.second.front()).c_str()); // use first type

			// naive way to get the r,g,b order
			std::sort(c.second.begin(), c.second.end());
			std::reverse(c.second.begin(), c.second.end());
			for (const auto& k: c.second)
				ph.keys.emplace_back(primary + L"." + k);

			const std::string nKey = toOSNarrowFromUTF16(primary);
			UT_String utKey = NameConversion::toPrimAttr(nKey);
			if (DBG) LOG_DBG << "name conversion: nKey = " << nKey << ", utKey = " << utKey;
			handleMap.emplace(utKey, std::move(ph));
		}
		else {
			for (const auto& secondary: c.second) {
				const std::wstring fullKey = [&primary,&secondary](){
					return secondary.empty() ? primary : (primary + L"." + secondary);
				}();

				ProtoHandle ph;
				ph.type = attrMap->getType(fullKey.c_str());
				ph.keys.emplace_back(fullKey);

				const std::string nKey = toOSNarrowFromUTF16(fullKey);
				UT_String utKey = NameConversion::toPrimAttr(nKey);
				if (DBG) LOG_DBG << "name conversion: nKey = " << nKey << ", utKey = " << utKey;
				handleMap.emplace(utKey, std::move(ph));
			}
		}
	}
}

class HandleVisitor : public boost::static_visitor<> {
private:
	const ProtoHandle&       protoHandle;
	const prt::AttributeMap* attrMap;
	const GA_IndexMap&       primIndexMap;
	GA_Offset                rangeStart;
	GA_Size                  rangeSize;

public:
	HandleVisitor(const ProtoHandle& ph, const prt::AttributeMap* m, const GA_IndexMap& pim, GA_Offset rStart, GA_Size rSize)
		: protoHandle(ph), attrMap(m), primIndexMap(pim), rangeStart(rStart), rangeSize(rSize) { }

    void operator()(const NoHandle& handle) const { }

    void operator()(GA_RWBatchHandleS& handle) const {
		assert(protoHandle.keys.size() == handle.getTupleSize());
	    assert(protoHandle.keys.size() == 1);
	    const wchar_t* v = attrMap->getString(protoHandle.keys.front().c_str());
		if (v && std::wcslen(v) > 0) {
			const std::string nv = toOSNarrowFromUTF16(v);
			const UT_StringHolder hv(nv.c_str());
			const GA_Range range(primIndexMap, rangeStart, rangeStart+rangeSize);
			handle.set(range, 0, hv);
			if (DBG) LOG_DBG << "string attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
			                 << handle.getAttribute()->getName() << " = " << nv;
		}
    }

    void operator()(const GA_RWHandleI& handle) const {
		assert(protoHandle.keys.size() == handle.getTupleSize());
		assert(protoHandle.keys.size() == 1);
		const int32_t v = attrMap->getInt(protoHandle.keys.front().c_str());
	    handle.setBlock(rangeStart, rangeSize, &v, 0, 0);
		if (DBG) LOG_DBG << "int attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
		                 << handle.getAttribute()->getName() << " = " << v;
    }

    void operator()(const GA_RWHandleC& handle) const {
		constexpr int8_t valFalse = 0;
		constexpr int8_t valTrue  = 1;

		assert(protoHandle.keys.size() == handle.getTupleSize());
		assert(protoHandle.keys.size() == 1);
	    const bool v = attrMap->getBool(protoHandle.keys.front().c_str());
	    const int8_t hv = v ? valTrue : valFalse;
	    handle.setBlock(rangeStart, rangeSize, &hv, 0, 0);
		if (DBG) LOG_DBG << "bool attr: range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
		                 << handle.getAttribute()->getName() << " = " << v;
    }

    void operator()(const GA_RWHandleF& handle) const {
		assert(protoHandle.keys.size() == handle.getTupleSize());
		for (size_t c = 0; c < protoHandle.keys.size(); c++) {
	        const auto v = attrMap->getFloat(protoHandle.keys[c].c_str());
			const auto hv = static_cast<fpreal32>(v);
		    handle.setBlock(rangeStart, rangeSize, &hv, 0, c); // using stride = 0 to always set the same value
		    if (DBG) LOG_DBG << "float attr: component = " << c << ", range = [" << rangeStart << ", " << rangeStart+rangeSize << "): "
			                 << handle.getAttribute()->getName() << " = " << v;
		}
	}
};

bool hasKeys(const prt::AttributeMap* attrMap, const std::vector<std::wstring>& keys) {
	for (const auto& k: keys)
		if (!attrMap->hasKey(k.c_str()))
			return false;
	return true;
}

void setAttributeValues(HandleMap& handleMap, const prt::AttributeMap* attrMap,
                        const GA_IndexMap& primIndexMap, const GA_Offset rangeStart, const GA_Size rangeSize)
{
	for (auto& h: handleMap) {
		if (hasKeys(attrMap, h.second.keys)) {
			const HandleVisitor hv(h.second, attrMap, primIndexMap, rangeStart, rangeSize);
			boost::apply_visitor(hv, h.second.handleType);
		}
	}
}

std::mutex mDetailMutex; // guard the houdini detail object

using UTVector3FVector = std::vector<UT_Vector3F>;

UTVector3FVector convertVertices(const double* vtx, size_t vtxSize) {
	UTVector3FVector utPoints;
	utPoints.reserve(vtxSize / 3);
	for (size_t pi = 0; pi < vtxSize; pi += 3) {
		const auto x = static_cast<fpreal32>(vtx[pi + 0]);
		const auto y = static_cast<fpreal32>(vtx[pi + 1]);
		const auto z = static_cast<fpreal32>(vtx[pi + 2]);
		utPoints.emplace_back(x, y, z);
	}
	return utPoints;
}

void setVertexNormals(
	GA_RWHandleV3& handle, const GA_Detail::OffsetMarker& marker,
	const double* nrm, size_t nrmSize, const uint32_t* indices
) {
	uint32_t vi = 0;
	for (GA_Iterator it(marker.vertexRange()); !it.atEnd(); ++it, ++vi) {
		const auto nrmIdx = indices[vi];
		const auto nrmPos = nrmIdx*3;
		assert(nrmPos + 2 < nrmSize);
		const auto nx = static_cast<fpreal32>(nrm[nrmPos + 0]);
		const auto ny = static_cast<fpreal32>(nrm[nrmPos + 1]);
		const auto nz = static_cast<fpreal32>(nrm[nrmPos + 2]);
		handle.set(it.getOffset(), UT_Vector3F(nx, ny, nz));
	}
}

constexpr auto UV_IDX_NO_VALUE = uint32_t(-1);

} // namespace


namespace ModelConversion {

void getUVSet(std::vector<uint32_t>& uvIndicesPerSet,
              const uint32_t* counts, size_t countsSize,
              const uint32_t* uvCounts, size_t uvCountsSize,
              uint32_t uvSet, uint32_t uvSets,
              const uint32_t* uvIndices, size_t uvIndicesSize)
{
	assert(uvSet < uvSets);

	uint32_t uvIdxBase = 0;
	for (size_t c = 0; c < countsSize; c++) {
		const uint32_t vtxCnt = counts[c];

		assert(uvSets * c + uvSet < uvCountsSize);
		const uint32_t uvCnt = uvCounts[uvSets * c + uvSet]; // 0 or vtxCnt
		assert(uvCnt == 0 || uvCnt == vtxCnt);
		if (uvCnt == vtxCnt) {

			// skip to desired uv set index range
			uint32_t uvIdxOff = 0;
			for (size_t uvi = 0; uvi < uvSet; uvi++)
				uvIdxOff += uvCounts[c*uvSets + uvi];

			const uint32_t uvIdxPos = uvIdxBase + uvIdxOff;
			for (size_t vi = 0; vi < uvCnt; vi++) {
				assert(uvIdxPos + vi < uvIndicesSize);
				const uint32_t uvIdx = uvIndices[uvIdxPos + vi];
				uvIndicesPerSet.push_back(uvIdx);
			}
		} else
			uvIndicesPerSet.insert(uvIndicesPerSet.end(), vtxCnt, UV_IDX_NO_VALUE);

		// skip to next face
		for (size_t uvSet = 0; uvSet < uvSets; uvSet++)
			uvIdxBase += uvCounts[c*uvSets + uvSet];
	}
}

void setUVs(GA_RWHandleV3& handle, const GA_Detail::OffsetMarker& marker,
            const uint32_t* counts, size_t countsSize,
            const uint32_t* uvCounts, size_t uvCountsSize,
            uint32_t uvSet, uint32_t uvSets,
            const uint32_t* uvIndices, size_t uvIndicesSize,
            const double* uvs, size_t uvsSize)
{
	std::vector<uint32_t> uvIndicesPerSet;
	getUVSet(uvIndicesPerSet, counts, countsSize, uvCounts, uvCountsSize, uvSet, uvSets, uvIndices, uvIndicesSize);

	uint32_t vi = 0;
	for (GA_Iterator it(marker.vertexRange()); !it.atEnd(); ++it, vi++) {
		const uint32_t uvIdx = uvIndicesPerSet[vi];
		if (uvIdx == UV_IDX_NO_VALUE)
			continue;
		assert(uvIdx * 2 + 1 < uvsSize);
		const auto du = uvs[uvIdx * 2 + 0];
		const auto dv = uvs[uvIdx * 2 + 1];
		const auto u = static_cast<fpreal32>(du);
		const auto v = static_cast<fpreal32>(dv);
		handle.set(it.getOffset(), UT_Vector3F(u, v, 0.0f));
	}
}

} // namespace ModelConversion


ModelConverter::ModelConverter(GU_Detail* gdp, UT_AutoInterrupt* autoInterrupt)
: mDetail(gdp), mAutoInterrupt(autoInterrupt) { }

void ModelConverter::add(const wchar_t* name,
                         const double* vtx, size_t vtxSize,
                         const double* nrm, size_t nrmSize,
                         const double* uvs, size_t uvsSize,
                         const uint32_t* counts, size_t countsSize,
                         const uint32_t* indices, size_t indicesSize,
                         const uint32_t* uvCounts, size_t uvCountsSize,
                         const uint32_t* uvIndices, size_t uvIndicesSize,
                         uint32_t uvSets,
                         const uint32_t* faceRanges, size_t faceRangesSize,
                         const prt::AttributeMap** materials,
                         const prt::AttributeMap** reports)
{
	std::lock_guard<std::mutex> guard(mDetailMutex); // protect all mDetail accesses

	// -- create primitives
	const GA_Detail::OffsetMarker marker(*mDetail);
	const UTVector3FVector utPoints = convertVertices(vtx, vtxSize);
	const GEO_PolyCounts geoPolyCounts = [&counts, &countsSize](){
		GEO_PolyCounts pc;
		for (size_t ci = 0; ci < countsSize; ci++) pc.append(counts[ci]);
		return pc;
	}();
	const GA_Offset primStartOffset = GU_PrimPoly::buildBlock(mDetail, utPoints.data(), utPoints.size(),
	                                                          geoPolyCounts, reinterpret_cast<const int*>(indices));

	// -- add vertex normals
	if (nrmSize > 0) {
		GA_RWHandleV3 nrmh(mDetail->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32));
		setVertexNormals(nrmh, marker, nrm, nrmSize, indices);
	}

	// -- add texture coordinates
	for (size_t uvSet = 0; uvSet < uvSets; uvSet++) {
		GA_RWHandleV3 uvh;
		if (uvSet == 0)
		    uvh.bind(mDetail->addTextureAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32)); // adds "uv" vertex attribute
		else {
			const std::string n = "uv" + std::to_string(uvSet);
			uvh.bind(mDetail->addTuple(GA_STORE_REAL32, GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, n.c_str(), 3));
		}
		ModelConversion::setUVs(uvh, marker, counts, countsSize, uvCounts, uvCountsSize,
		                        uvSet, uvSets, uvIndices, uvIndicesSize, uvs, uvsSize);
	}

	// -- add primitives to detail
	const std::string nName = toOSNarrowFromUTF16(name);
	auto& elemGroupTable = mDetail->getElementGroupTable(GA_ATTRIB_PRIMITIVE);
	GA_PrimitiveGroup* primGroup = static_cast<GA_PrimitiveGroup*>(elemGroupTable.newGroup(nName.c_str(), false));
	primGroup->addRange(marker.primitiveRange());

	// -- convert materials/reports into primitive attributes based on face ranges
	if (DBG) LOG_DBG << "got " << faceRangesSize-1 << " face ranges";
	if (faceRangesSize > 1) {
		HandleMap handleMap;
		const GA_IndexMap& primIndexMap = mDetail->getIndexMap(GA_ATTRIB_PRIMITIVE);
		for (size_t fri = 0; fri < faceRangesSize-1; fri++) {
			const GA_Offset rangeStart = primStartOffset + faceRanges[fri];
			const GA_Size   rangeSize  = faceRanges[fri + 1] - faceRanges[fri];

			if (materials != nullptr) {
				const prt::AttributeMap* attrMap = materials[fri];
				extractAttributeNames(handleMap, attrMap);
				createAttributeHandles(mDetail, handleMap);
				setAttributeValues(handleMap, attrMap, primIndexMap, rangeStart, rangeSize);
			}

			if (reports != nullptr) {
				const prt::AttributeMap* attrMap = reports[fri];
				extractAttributeNames(handleMap, attrMap);
				createAttributeHandles(mDetail, handleMap);
				setAttributeValues(handleMap, attrMap, primIndexMap, rangeStart, rangeSize);
			}
		}
	}
}

prt::Status ModelConverter::generateError(size_t isIndex, prt::Status status, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}

prt::Status ModelConverter::assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) {
	LOG_WRN << key << L": " << message;
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	return prt::STATUS_OK;
}
