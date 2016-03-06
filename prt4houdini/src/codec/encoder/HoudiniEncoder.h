#ifndef HOUDINIENCODER_H_
#define HOUDINIENCODER_H_

#include <string>
#include <iostream>
#include <stdexcept>

#include "prt/ContentType.h"
#include "prt/InitialShape.h"

#include "prtx/ResolveMap.h"
#include "prtx/Encoder.h"
#include "prtx/EncoderFactory.h"
#include "prtx/EncoderInfoBuilder.h"
#include "prtx/PRTUtils.h"
#include "prtx/Singleton.h"


class HoudiniCallbacks;


class HoudiniEncoder : public prtx::GeometryEncoder {
public:
	static const std::wstring ID;
	static const std::wstring NAME;
	static const std::wstring DESCRIPTION;

public:
	HoudiniEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks);
	virtual ~HoudiniEncoder();

public:
	virtual void init(prtx::GenerateContext& context);
	virtual void encode(prtx::GenerateContext& context, size_t initialShapeIndex);
	virtual void finish(prtx::GenerateContext& context);

private:
	void convertGeometry(
		const prtx::InitialShape& initialShape,
		const prtx::GeometryPtrVector& geometries,
		const std::vector<prtx::MaterialPtrVector>& mat,
		HoudiniCallbacks* callbacks
	);
};


class HoudiniEncoderFactory : public prtx::EncoderFactory, public prtx::Singleton<HoudiniEncoderFactory> {
public:
	static HoudiniEncoderFactory* createInstance() {
		prtx::EncoderInfoBuilder encoderInfoBuilder;

		encoderInfoBuilder.setID(HoudiniEncoder::ID);
		encoderInfoBuilder.setName(HoudiniEncoder::NAME);
		encoderInfoBuilder.setDescription(HoudiniEncoder::DESCRIPTION);
		encoderInfoBuilder.setType(prt::CT_GEOMETRY);

		return new HoudiniEncoderFactory(encoderInfoBuilder.create());
	}

	HoudiniEncoderFactory(const prt::EncoderInfo* info) : prtx::EncoderFactory(info) { }
	virtual ~HoudiniEncoderFactory() { }

	virtual HoudiniEncoder* create(const prt::AttributeMap* options, prt::Callbacks* callbacks) const {
		return new HoudiniEncoder(getID(), options, callbacks);
	}
};


#endif /* HOUDINIENCODER_H_ */
