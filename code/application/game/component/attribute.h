#pragma once
#include "core/types.h"
#include "util/fourcc.h"
#include "game/attr/accessmode.h"
#include "game/attr/valuetype.h"

namespace Attr
{

// template <typename 

template<typename T>
constexpr ValueType TypeToValueType()
{
	static_assert(false, "Type is not implemented!");
	return ValueType::NONE;
}

template<> constexpr ValueType TypeToValueType<bool>()				{ return ValueType::BoolType; }
template<> constexpr ValueType TypeToValueType<int>()				{ return ValueType::IntType; }
template<> constexpr ValueType TypeToValueType<uint>()				{ return ValueType::UIntType; }
template<> constexpr ValueType TypeToValueType<float>()				{ return ValueType::FloatType; }
template<> constexpr ValueType TypeToValueType<double>()			{ return ValueType::DoubleType; }
template<> constexpr ValueType TypeToValueType<Math::matrix44>()	{ return ValueType::Matrix44Type; }
template<> constexpr ValueType TypeToValueType<Math::float4>()		{ return ValueType::Float4Type; }
template<> constexpr ValueType TypeToValueType<Math::quaternion>()	{ return ValueType::QuaternionType; }
template<> constexpr ValueType TypeToValueType<Util::String>()		{ return ValueType::StringType; }
template<> constexpr ValueType TypeToValueType<Util::Guid>()		{ return ValueType::GuidType; }
template<> constexpr ValueType TypeToValueType<Game::Entity>()		{ return ValueType::EntityType; }

class Attribute
{
public:
	Attribute() :
		attrIndex(-1),
		fourcc(-1),
		name(""),
		type(ValueType::VoidType),
		accessMode(AccessMode::ReadOnly),
		defaultValue()
	{
		// empty
	}

	Attribute(uint index, Util::FourCC fourcc, const char* name, ValueType type, AccessMode accessMode, Util::Variant const& defaultValue) :
		attrIndex(index),
		fourcc(fourcc),
		name(name),
		type(type),
		accessMode(accessMode),
		defaultValue(defaultValue)
	{
		// empty
	}

	using InnerType = uint;

	uint attrIndex;
	Util::FourCC fourcc;
	Util::String name;
	ValueType type;
	Util::Variant defaultValue;
	AccessMode accessMode;

	// static constexpr Util::FourCC FourCC()
	// {
	// 	return Util::FourCC('FOUR');
	// }
	// static constexpr const char* Name()
	// {
	// 	return "AttributeName";
	// }
	// static constexpr const char* TypeName()
	// {
	// 	return "uint";
	// }
	// static constexpr ValueType Type()
	// {
	// 	return TypeToValueType<uint>();
	// }
	// static constexpr uint DefaultValue()
	// {
	// 	return uint(-1);
	// }
};

//------------------------------------------------------------------------------
/**
	@note	Make sure to send an explicit type as default value (ex. uint(10), Math::matrix44::identity(), etc.)
*/
#define __DeclareAttribute(ATTRIBUTENAME, TYPE, FOURCC, ACCESSMODE, DEFAULTVALUE) \
class ATTRIBUTENAME : public Attr::Attribute\
{\
public:\
	ATTRIBUTENAME(uint index) : Attribute(index, FOURCC, #ATTRIBUTENAME, Attr::TypeToValueType<TYPE>(), ACCESSMODE, Util::Variant(DEFAULTVALUE)) {};\
	using InnerType = TYPE;\
	static constexpr uint FourCC()\
	{\
		return FOURCC;\
	}\
	static constexpr const char* Name()\
	{\
		return #ATTRIBUTENAME;\
	}\
	static constexpr const char* TypeName()\
	{\
		return #TYPE;\
	}\
	static constexpr Attr::ValueType Type()\
	{\
		return Attr::TypeToValueType<TYPE>();\
	}\
	static const TYPE DefaultValue()\
	{\
		return TYPE(DEFAULTVALUE);\
	}\
};

__DeclareAttribute(TestAttr, int, 'TEST', AccessMode::ReadWrite, int(12));
__DeclareAttribute(TestFloatAttr, float, 'TFLT', AccessMode::ReadOnly, float(20));

} // namespace Attr
