#include "BitArray.h"

FBitArray::FBitArray()
    : BitLength(0)
    , BitSize(0)
    , BitData(nullptr)
{
}

FBitArray::FBitArray(int32 Len)
    : BitLength(Len)
    , BitSize((Len + WordSize - 1) / WordSize)
{
    BitData = new WordType[BitSize];
    FMemory::Memset(BitData, 0, sizeof(WordType) * BitSize);
}

FBitArray::FBitArray(const FBitArray& Other)
{
    BitLength = Other.BitLength;
    BitSize = Other.BitSize;
    BitData = new WordType[BitSize];

    FMemory::Memcpy(BitData, Other.BitData, sizeof(WordType) * BitSize);
}

FBitArray::FBitArray(FBitArray&& Other)
{
    *this = MoveTemp(Other);
}

FBitArray::~FBitArray()
{
    delete[] BitData;
}

bool FBitArray::Add(int32 Index)
{
    ensure(Index < BitLength);
    int32 BitIndex = Index / WordSize;
    if (BitIndex < BitSize)
    {
        BitData[BitIndex] |= (1 << (Index % WordSize));
        return true;
    }

    return false;
}

bool FBitArray::AddRange(int32 BeginIndex, int32 EndIndex)
{
    ensure(BeginIndex >= 0 && EndIndex < BitLength && BeginIndex <= EndIndex);
    if (BeginIndex == EndIndex)
    {
        return Add(BeginIndex);
    }
    
    const int32 BitIndexStart = BeginIndex / WordSize;
    const int32 BitIndexEnd = EndIndex / WordSize;
    for (int32 i = BitIndexStart + 1; i < BitIndexEnd; ++i)
    {
        BitData[i] = FullWordMask;
    }
    if (BitIndexStart < BitIndexEnd)
    {
        if (BitIndexStart < BitSize)
        {
            BitData[BitIndexStart] |= FullWordMask ^ ((1 << (BeginIndex % WordSize)) - 1);
        }

        if (BitIndexEnd < BitSize)
        {
            BitData[BitIndexEnd] |= (1ull << ((EndIndex % WordSize) + 1)) - 1;
        }
    }
    else
    {
        WordType BeginMark = FullWordMask ^ ((1 << (BeginIndex % WordSize)) - 1);
        WordType EndMark = (1ull << ((EndIndex % WordSize) + 1)) - 1;
        BitData[BitIndexStart] |= BeginMark & EndMark;
    }

    return true;
}

bool FBitArray::Remove(int32 Index)
{
    ensure(Index < BitLength);
    int32 BitIndex = Index / WordSize;
    if (BitIndex < BitSize)
    {
        BitData[BitIndex] &= ~(1 << (Index % WordSize));
        return true;
    }

    return false;
}

bool FBitArray::RemoveRange(int32 BeginIndex, int32 EndIndex)
{
    ensure(BeginIndex >= 0 && EndIndex < BitLength && BeginIndex <= EndIndex);
    if (BeginIndex == EndIndex)
    {
        return Remove(BeginIndex);
    }
    
    const int32 BitIndexStart = BeginIndex / WordSize;
    const int32 BitIndexEnd = EndIndex / WordSize;
    for (int32 i = BitIndexStart + 1; i < BitIndexEnd; ++i)
    {
        BitData[i] = 0;
    }
    if (BitIndexStart < BitIndexEnd)
    {
        if (BitIndexStart < BitSize)
        {
            BitData[BitIndexStart] &= (1 << (BeginIndex % WordSize)) - 1;
        }
        if (BitIndexEnd < BitSize)
        {
            BitData[BitIndexEnd] &= ~((1 << (EndIndex % WordSize)) - 1);
        }
    }
    else
    {
        WordType BeginMark = FullWordMask ^ ((1 << (BeginIndex % WordSize)) - 1);
        WordType EndMark = (1ull << ((EndIndex % WordSize) + 1)) - 1;
        BitData[BitIndexStart] &= ~(BeginMark & EndMark);
    }

    return true;
}

bool FBitArray::Clear()
{
    if (BitData)
    {
        FMemory::Memset(BitData, 0, sizeof(WordType) * BitSize);
        return true;
    }
    return false;
}

void FBitArray::Reset(int32 Len)
{
    delete[] BitData;
	BitLength = Len;
	BitSize = (Len + WordSize - 1) / WordSize;
	BitData = new WordType[BitSize];
	FMemory::Memset(BitData, 0, sizeof(WordType)* BitSize);
}

bool FBitArray::MarkAll()
{
    if (BitSize <= 0)
    {
        return false;
    }
    for (int32 Index = 0; Index < BitSize - 1; ++Index)
    {
        BitData[Index] = FullWordMask;
    }

    BitData[BitSize - 1] = (1ULL << (((BitLength - 1) & UnitWordMask) + 1)) - 1;
    return true;
}

bool FBitArray::IsEmpty() const
{
    for (int32 Index = 0; Index < BitSize; ++Index)
    {
        if (BitData[Index])
        {
            return false;
        }
    }

    return true;
}

int32 FBitArray::GetSize() const
{
    return BitSize;
}

FBitArray& FBitArray::operator=(const FBitArray& Other)
{
    if (BitSize != Other.BitSize)
    {
        BitSize = Other.BitSize;
        delete[] BitData;

        BitData = new WordType[BitSize];
        FMemory::Memset(BitData, 0, sizeof(WordType) * BitSize);
    }

    BitLength = Other.BitLength;
    FMemory::Memcpy(BitData, Other.BitData, sizeof(WordType) * BitSize);
    
    return *this;
}

FBitArray& FBitArray::operator=(FBitArray&& Other)
{
    BitLength = Other.BitLength;
    BitSize = Other.BitSize;
    BitData = Other.BitData;
    Other.BitData = nullptr;

    return *this;
}

FBitArray& FBitArray::operator &= (const FBitArray& Other)
{
    BitLength = FMath::Max(BitLength, Other.BitLength);
    if (BitSize < Other.BitSize)
    {
        auto NewBitData = new WordType[Other.BitSize];
        FMemory::Memcpy(NewBitData, BitData, BitSize);
        FMemory::Memset(NewBitData + BitSize, 0, sizeof(WordType) * (Other.BitSize - BitSize));

        delete[] BitData;
        BitData = NewBitData;
        BitSize = Other.BitSize;
    }

    int32 MinSize = Other.BitSize;
    for (int32 Index = 0; Index < MinSize; ++Index)
    {
        BitData[Index] &= Other.BitData[Index];
    }

    for (int32 Index = MinSize; Index < BitSize; ++Index)
    {
        BitData[Index] = 0;
    }

    return *this;
}

FBitArray& FBitArray::operator |= (const FBitArray& Other)
{
    BitLength = FMath::Max(BitLength, Other.BitLength);

    if (BitSize < Other.BitSize)
    {
        auto NewBitData = new WordType[Other.BitSize];
        FMemory::Memcpy(NewBitData, BitData, BitSize);
        FMemory::Memset(NewBitData + BitSize, 0, sizeof(WordType) * (Other.BitSize - BitSize));

        delete[] BitData;
        BitData = NewBitData;
        BitSize = Other.BitSize;
    }

    int32 MinSize = Other.BitSize;
    for (int32 Index = 0; Index < MinSize; ++Index)
    {
        BitData[Index] |= Other.BitData[Index];
    }
    
    return *this;
}

bool FBitArray::operator [] (int32 Index)
{
	int32 BitIndex = Index / WordSize;
	if (BitIndex < BitSize)
	{
		return BitData[BitIndex] & (1 << (Index % WordSize));
	}

	return false;
}
