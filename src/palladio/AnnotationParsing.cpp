/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "AnnotationParsing.h"
#include "RuleAttributes.h"

#include <cmath>
#include <sstream>

namespace {

constexpr const wchar_t* NULL_KEY = L"#NULL#";
constexpr const wchar_t* MIN_KEY = L"min";
constexpr const wchar_t* MAX_KEY = L"max";

constexpr const wchar_t* RESTRICTED_KEY = L"restricted";
constexpr const wchar_t* VALUES_ATTR_KEY = L"valuesAttr";

enum class RangeType { RANGE, ENUM, INVALID };
RangeType GetRangeType(const prt::Annotation& annotation) {
	const size_t numArgs = annotation.getNumArguments();

	if (numArgs == 0)
		return RangeType::INVALID;

	prt::AnnotationArgumentType commonType = annotation.getArgument(0)->getType();

	bool hasMin = false;
	bool hasMax = false;
	bool hasKey = false;
	bool onlyOneTypeInUse = true;

	for (int argIdx = 0; argIdx < numArgs; argIdx++) {
		const prt::AnnotationArgument* arg = annotation.getArgument(argIdx);
		if (arg->getType() != commonType)
			onlyOneTypeInUse = false;

		const wchar_t* key = arg->getKey();
		if (std::wcscmp(key, MIN_KEY) == 0)
			hasMin = true;
		else if (std::wcscmp(key, MAX_KEY) == 0)
			hasMax = true;
		if (std::wcscmp(key, NULL_KEY) != 0)
			hasKey = true;
	}

	// old Range
	if ((numArgs == 2) && (commonType == prt::AnnotationArgumentType::AAT_FLOAT))
		return RangeType::RANGE;

	// new Range
	if ((numArgs >= 2) && (hasMin && hasMax))
		return RangeType::RANGE;

	// legacy Enum
	if (!hasKey && onlyOneTypeInUse)
		return RangeType::ENUM;

	return RangeType::INVALID;
}

int fromHex(wchar_t c) {
	// clang-format off
	switch (c) {
		case '0': return 0;	case '1': return 1;	case '2': return 2;	case '3': return 3;	case '4': return 4;
		case '5': return 5;	case '6': return 6;	case '7': return 7;	case '8': return 8;	case '9': return 9;
		case 'a': case 'A': return 0xa;
		case 'b': case 'B': return 0xb;
		case 'c': case 'C': return 0xc;
		case 'd': case 'D': return 0xd;
		case 'e': case 'E': return 0xe;
		case 'f': case 'F': return 0xf;
		default:
			return 0;
	}
	// clang-format on
}

constexpr const wchar_t* HEXTAB = L"0123456789ABCDEF";

wchar_t toHex(int i) {
	return HEXTAB[i & 0xF];
}
} // namespace

namespace AnnotationParsing {
RangeAnnotation parseRangeAnnotation(const prt::Annotation& annotation) {
	auto minMax = std::make_pair(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());

	const wchar_t* anName = annotation.getName();

	if (std::wcscmp(anName, ANNOT_RANGE) == 0) {
		const RangeType annotationRangeType = GetRangeType(annotation);
		if (annotationRangeType != RangeType::RANGE)
			return minMax;

		const size_t numArgs = annotation.getNumArguments();

		for (int argIdx = 0; argIdx < numArgs; argIdx++) {
			const prt::AnnotationArgument* arg = annotation.getArgument(argIdx);
			const wchar_t* key = arg->getKey();
			if (std::wcscmp(key, MIN_KEY) == 0) {
				minMax.first = arg->getFloat();
			}
			else if (std::wcscmp(key, MAX_KEY) == 0) {
				minMax.second = arg->getFloat();
			}
		}

		// parse old style range
		if ((std::isnan(minMax.first) || std::isnan(minMax.second)) && (numArgs == 2)) {
			minMax.first = annotation.getArgument(0)->getFloat();
			minMax.second = annotation.getArgument(1)->getFloat();
		}

		return minMax;
	}
	return minMax;
}

EnumAnnotation parseEnumAnnotation(const prt::Annotation& annotation) {
	EnumAnnotation enumAttribute;

	for (size_t arg = 0; arg < annotation.getNumArguments(); arg++) {

		const wchar_t* key = annotation.getArgument(arg)->getKey();
		if (std::wcscmp(key, NULL_KEY) != 0) {
			if (std::wcscmp(key, RESTRICTED_KEY) == 0)
				enumAttribute.mRestricted = annotation.getArgument(arg)->getBool();

			if (std::wcscmp(key, VALUES_ATTR_KEY) == 0) {
				enumAttribute.mValuesAttr = annotation.getArgument(arg)->getStr();
			}
			continue;
		}

		switch (annotation.getArgument(arg)->getType()) {
			case prt::AAT_BOOL: {
				const bool val = annotation.getArgument(arg)->getBool();
				enumAttribute.mOptions.push_back(std::to_wstring(val).c_str());
				break;
			}
			case prt::AAT_FLOAT: {
				const double val = annotation.getArgument(arg)->getFloat();
				enumAttribute.mOptions.push_back(std::to_wstring(val).c_str());
				break;
			}
			case prt::AAT_STR: {
				const wchar_t* val = annotation.getArgument(arg)->getStr();
				enumAttribute.mOptions.push_back(val);
				break;
			}
			default:
				break;
		}
	}
	return enumAttribute;
}

FileAnnotation parseFileAnnotation(const prt::Annotation& annotation) {
	FileAnnotation fileAnnotation;
	for (size_t argIdx = 0; argIdx < annotation.getNumArguments(); argIdx++) {
		auto arg = annotation.getArgument(argIdx);

		if (arg->getType() == prt::AAT_STR) {
			fileAnnotation.push_back(arg->getStr());
		}
	}
	return fileAnnotation;
}

std::wstring parseDescriptionAnnotation(const prt::Annotation& annotation) {
	return annotation.getArgument(0)->getStr();
}

ColorAnnotation parseColor(const std::wstring colorString) {
	ColorAnnotation color{0.0, 0.0, 0.0};
	if (colorString.size() >= 7 && colorString[0] == '#') {
		color[0] = static_cast<double>((fromHex(colorString[1]) << 4) + fromHex(colorString[2])) / 255.0;
		color[1] = static_cast<double>((fromHex(colorString[3]) << 4) + fromHex(colorString[4])) / 255.0;
		color[2] = static_cast<double>((fromHex(colorString[5]) << 4) + fromHex(colorString[6])) / 255.0;
	}
	return color;
}

std::wstring getColorString(const std::array<float, 3>& rgb) {
	std::wstringstream colStr;
	colStr << L'#';
	colStr << toHex(((int)(rgb[0] * 255.0f)) >> 4);
	colStr << toHex((int)(rgb[0] * 255.0f));
	colStr << toHex(((int)(rgb[1] * 255.0f)) >> 4);
	colStr << toHex((int)(rgb[1] * 255));
	colStr << toHex(((int)(rgb[2] * 255.0f)) >> 4);
	colStr << toHex((int)(rgb[2] * 255.0f));
	return colStr.str();
}

AttributeTrait detectAttributeTrait(const prt::Annotation& annotation) {
	const wchar_t* anName = annotation.getName();
	if (std::wcscmp(anName, ANNOT_ENUM) == 0)
		return AttributeTrait::ENUM;
	else if (std::wcscmp(anName, ANNOT_RANGE) == 0) {
		const RangeType annotationRangeType = GetRangeType(annotation);
		switch (annotationRangeType) {
			case RangeType::ENUM:
				return AttributeTrait::ENUM;
			case RangeType::RANGE:
				return AttributeTrait::RANGE;
			case RangeType::INVALID:
				return AttributeTrait::NONE;
		}
	}
	else if (std::wcscmp(anName, ANNOT_COLOR) == 0)
		return AttributeTrait::COLOR;
	else if (std::wcscmp(anName, ANNOT_DIR) == 0)
		return AttributeTrait::DIR;
	else if (std::wcscmp(anName, ANNOT_FILE) == 0)
		return AttributeTrait::FILE;
	else if (std::wcscmp(anName, ANNOT_ANGLE) == 0)
		return AttributeTrait::ANGLE;
	else if (std::wcscmp(anName, ANNOT_PERCENT) == 0)
		return AttributeTrait::PERCENT;
	else if (std::wcscmp(anName, ANNOT_DESCRIPTION) == 0)
		return AttributeTrait::DESCRIPTION;

	return AttributeTrait::NONE;
}

AttributeAnnotationInfo getAttributeAnnotationInfo(const std::wstring& attributeName, const RuleFileInfoUPtr& info) {
	const prt::Annotation* annotation = nullptr;
	AttributeTrait attributeTrait = AttributeTrait::NONE;
	std::wstring description;

	for (size_t attributeIdx = 0, numAttrs = info->getNumAttributes(); attributeIdx < numAttrs; attributeIdx++) {
		const auto* attribute = info->getAttribute(attributeIdx);
		if (std::wcscmp(attributeName.c_str(), attribute->getName()) != 0)
			continue;
		for (size_t annotationIdx = 0; annotationIdx < attribute->getNumAnnotations(); annotationIdx++) {
			const prt::Annotation* currAnnotation = attribute->getAnnotation(annotationIdx);
			AttributeTrait currAttributeTrait = detectAttributeTrait(*currAnnotation);

			const bool skipOverride =
			        (((currAttributeTrait == AttributeTrait::PERCENT) || (currAttributeTrait == AttributeTrait::ANGLE)) &&
			         attributeTrait == AttributeTrait::RANGE);

			if (currAttributeTrait == AttributeTrait::NONE || skipOverride) {
				if (annotation == nullptr || attributeTrait == AttributeTrait::NONE) {
					annotation = currAnnotation;
					attributeTrait = currAttributeTrait;
				}
				continue;
			}
			
			if (currAttributeTrait == AttributeTrait::DESCRIPTION) {
				description = parseDescriptionAnnotation(*currAnnotation);
				if (annotation == nullptr || attributeTrait == AttributeTrait::DESCRIPTION) {
					annotation = currAnnotation;
					attributeTrait = currAttributeTrait;
				}
			}	
			else {
				annotation = currAnnotation;
				attributeTrait = currAttributeTrait;
			}
		}
	}
	if (annotation == nullptr)
		throw std::invalid_argument("attribute not found in ruleFileInfo");

	return AttributeAnnotationInfo(*annotation, attributeTrait, description);
}
} // namespace AnnotationParsing

