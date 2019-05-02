#pragma once
//------------------------------------------------------------------------------
/**
    @class Util::BitField
    
    Implements large bit field with multiple of 32 bits.
    
    (C) 2009 Radon Labs GmbH
    (C) 2013-2018 Individual contributors, see AUTHORS file
*/
#include "core/types.h"

//------------------------------------------------------------------------------
namespace Util
{
template<unsigned int NUMBITS> class BitField
{
public:
    /// constructor
    BitField();
	/// constructs a bitfield based on multiple values
	constexpr BitField(std::initializer_list<unsigned int> list);
    /// copy constructor
    BitField(const BitField<NUMBITS>& rhs);
    
    /// assignment operator
    void operator=(const BitField<NUMBITS>& rhs);
    /// equality operator
	constexpr bool operator==(const BitField<NUMBITS>& rhs) const;
    /// inequality operator
	constexpr bool operator!=(const BitField<NUMBITS>& rhs) const;
    
	/// Check if single bit is set
	constexpr bool IsSet(const IndexT bitIndex) const;
	/// Check if single bit is set
	template<unsigned int bitIndex>
	constexpr bool IsSet() const;
    /// clear content
    void Clear();
    /// return true if all bits are 0
    bool IsNull() const;
    /// set a bit by index
    constexpr void SetBit(const IndexT bitIndex);
	/// set a bit by index
	template<unsigned int bitIndex>
	constexpr void SetBit();
    /// clear a bit by index
    void ClearBit(const IndexT bitIndex);
    
    /// set bitfield to OR combination
    static constexpr BitField<NUMBITS> Or(const BitField<NUMBITS>& b0, const BitField<NUMBITS>& b1);
    /// set bitfield to AND combination
    static constexpr BitField<NUMBITS> And(const BitField<NUMBITS>& b0, const BitField<NUMBITS>& b1);

private:
    static const int size = ((NUMBITS + 31) / 32);
    uint bits[size];
};

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS>
BitField<NUMBITS>::BitField()
{
    IndexT i;
    for (i = 0; i < size; i++)
    {
        this->bits[i] = 0;
    }
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS>
constexpr
BitField<NUMBITS>::BitField(std::initializer_list<unsigned int> list)
{
	IndexT i;
	for (i = 0; i < size; i++)
	{
		this->bits[i] = 0;
	}

	for (auto bit : list)
	{
		this->SetBit(bit);
	}
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS>
BitField<NUMBITS>::BitField(const BitField<NUMBITS>& rhs)
{
    IndexT i;
    for (i = 0; i < size; i++)
    {
        this->bits[i] = rhs.bits[i];
    }
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> void
BitField<NUMBITS>::operator=(const BitField<NUMBITS>& rhs)
{
    IndexT i;
    for (i = 0; i < size; i++)
    {
        this->bits[i] = rhs.bits[i];
    }
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> constexpr bool
BitField<NUMBITS>::operator==(const BitField<NUMBITS>& rhs) const
{
    IndexT i;
    for (i = 0; i < size; i++)
    {
        if (this->bits[i] != rhs.bits[i])
        {
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> constexpr bool
BitField<NUMBITS>::operator!=(const BitField<NUMBITS>& rhs) const
{
    return !(*this == rhs);
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> constexpr bool
BitField<NUMBITS>::IsSet(const IndexT bitIndex) const
 {
	n_assert(bitIndex < NUMBITS);
	const uint i = (1 << (bitIndex % 32));
	const uint index = bitIndex / 32;
	return (this->bits[index] & i) == i;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS>
template<unsigned int bitIndex>
constexpr bool
BitField<NUMBITS>::IsSet() const
{
	n_assert(bitIndex < NUMBITS);
	constexpr uint i = (1 << (bitIndex % 32));
	constexpr uint index = bitIndex / 32;
	return (this->bits[index] & i) == i;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> void
BitField<NUMBITS>::Clear()
{
    IndexT i;
    for (i = 0; i < size; i++)
    {
        this->bits[i] = 0;
    }
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> bool
BitField<NUMBITS>::IsNull() const
{
    IndexT i;
    for (i = 0; i < size; i++)
    {
        if (this->bits[i] != 0)
        {
            return false;
        }
    }
    return true;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> constexpr void
BitField<NUMBITS>::SetBit(const IndexT i)
{
    n_assert(i < NUMBITS);
	const uint index = i / 32;
	const uint bit = (1 << (i % 32));
    this->bits[index] |= bit;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS>
template<unsigned int i>
constexpr void
BitField<NUMBITS>::SetBit()
{
	n_assert(i < NUMBITS);
	constexpr uint index = i / 32;
	constexpr uint bit = (1 << (i % 32));
	this->bits[index] |= bit;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> void
BitField<NUMBITS>::ClearBit(const IndexT i)
{
    n_assert(i < NUMBITS);
	constexpr uint index = i / 32;
	constexpr uint bit = ~(1 << (i % 32));
    this->bits[index] &= bit;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> constexpr BitField<NUMBITS>
BitField<NUMBITS>::Or(const BitField<NUMBITS>& b0, const BitField<NUMBITS>& b1)
{
    BitField<NUMBITS> res;
    IndexT i;
    for (i = 0; i < size; i++)
    {
        res.bits[i] = b0.bits[i] | b1.bits[i];
    }
    return res;
}

//------------------------------------------------------------------------------
/**
*/
template<unsigned int NUMBITS> constexpr BitField<NUMBITS>
BitField<NUMBITS>::And(const BitField<NUMBITS>& b0, const BitField<NUMBITS>& b1)
{
    BitField<NUMBITS> res;
    IndexT i;
    for (i = 0; i < size; i++)
    {
        res.bits[i] = b0.bits[i] & b1.bits[i];
    }
    return res;
}

} // namespace Util
//------------------------------------------------------------------------------

    