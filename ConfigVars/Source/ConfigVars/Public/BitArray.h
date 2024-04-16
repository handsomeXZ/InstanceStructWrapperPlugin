#pragma once

#include "CoreMinimal.h"

class FBitArray
{
public:
    typedef uint32 WordType;

    FBitArray();
    FBitArray(int32 Len);
    FBitArray(const FBitArray& Other);
    FBitArray(FBitArray&& Other);
    
    ~FBitArray();

    FORCEINLINE bool Add(int32 Index);
    // Range[BeginIndex, EndIndex] include EndIndex, not Range[BeginIndex, EndIndex)
    bool AddRange(int32 BeginIndex, int32 EndIndex);
    FORCEINLINE bool Remove(int32 Index);
    bool RemoveRange(int32 BeginIndex, int32 EndIndex);
    FORCEINLINE bool Clear();
    FORCEINLINE void Reset(int32 Len);
    FORCEINLINE bool MarkAll();
    FORCEINLINE bool IsEmpty() const;
    FORCEINLINE int32 GetSize() const;

    FORCEINLINE FBitArray& operator = (const FBitArray& Other);
    FORCEINLINE FBitArray& operator = (FBitArray&& Other);
    FORCEINLINE FBitArray& operator &= (const FBitArray& Other);
    FORCEINLINE FBitArray& operator |= (const FBitArray& Other);

    FORCEINLINE bool operator [] (int32 Index);
    
    friend FArchive& operator<<(FArchive& Ar, FBitArray& A)
    {
        int32 RealBitLength = A.BitLength;

        Ar << RealBitLength;

		if (Ar.IsLoading())
		{
			A.Reset(RealBitLength);
		}

		for (int32 Index = 0; Index < A.BitSize; ++Index)
		{
			Ar << A.BitData[Index];
		}

        return Ar;
    }

    class FIterator
    {
    public:
        FORCEINLINE FIterator(const FBitArray& Array)
            : BitIndex(0)
            , MaxIndex(-1)
            , CurrentWord(0)
            , BitArray(Array)
        {
            for (int32 Index = BitArray.BitSize - 1; Index >= 0; --Index)
            {
                if (BitArray.BitData[Index])
                {
                    MaxIndex = Index;
                    break;
                }
            }

            for (int32 Index = 0; Index <= MaxIndex; ++Index)
            {
                if (BitArray.BitData[Index])
                {
                    BitIndex = Index;
                    CurrentWord = BitArray.BitData[Index];
                    break;
                }
            }
        }

        FORCEINLINE operator bool()
        {
            if (BitIndex < MaxIndex)
            {
                return true;
            }
            return CurrentWord != 0;
        }
        
        FORCEINLINE FIterator& operator++()
        {
            CurrentWord &= CurrentWord - 1;
            for (; CurrentWord == 0 && BitIndex < MaxIndex;)
            {
                CurrentWord = BitArray.BitData[++BitIndex];
            }
            return *this;
        }

        // From http://graphics.stanford.edu/~seander/bithacks.html
        FORCEINLINE static int32 Log2(WordType Power2Num)
        {
            //x=2^k
            static const int32 Log2Map[32]={31,0,27,1,28,18,23,2,29,21,19,12,24,9,14,3,30,26,17,22,20,11,8,13,25,16,10,7,15,6,5,4};
            return Log2Map[Power2Num*263572066>>27];
        }

        FORCEINLINE int32 operator * () const
        {
            return BitIndex * WordSize + Log2(CurrentWord ^ (CurrentWord & (CurrentWord - 1)));
        }

    protected:
        int32 BitIndex;
        int32 MaxIndex;
        WordType CurrentWord;
        const FBitArray& BitArray;
    };
    
protected:
    static constexpr WordType FullWordMask = (WordType)-1;
    static constexpr int32 WordSize = sizeof(WordType) * 8;
    static constexpr int32 UnitWordMask = WordSize - 1;

    int32 BitLength;
    int32 BitSize;
    WordType* BitData;
};
