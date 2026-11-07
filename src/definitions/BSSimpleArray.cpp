#include <algorithm>
#include <cstdint>
#include "Bethesda/BSSimpleArray.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Bethesda/WeaponData.hpp"
#include "nvse/PluginAPI.h"

namespace CommonLib {

	template <typename T, std::uint32_t N>
	BSSimpleArray<T, N>::BSSimpleArray(unsigned int aiInitialSize) {
		_InitialSetup(aiInitialSize, aiInitialSize);
	}

	template <typename T, std::uint32_t N>
	BSSimpleArray<T, N>::BSSimpleArray(unsigned int aiReserveSize, unsigned int aiInitialSize) {
		_InitialSetup(aiReserveSize, aiInitialSize);
	}

	template <typename T, std::uint32_t N>
	BSSimpleArray<T, N>::~BSSimpleArray() {
		Clear(1);
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::_InitialSetup(unsigned int aiReserveSize, unsigned int aiInitialSize) {
		pBuffer = nullptr;
		iSize = 0;
		iReservedSize = 0;

		if (aiInitialSize > aiReserveSize) {
			aiReserveSize = aiInitialSize;
		}

		if (aiReserveSize) {
			pBuffer = _Allocate(aiReserveSize);
			iReservedSize = aiReserveSize;
		}
		if (aiInitialSize) {
			_ConstructItems(this->pBuffer, aiInitialSize);
			iSize = aiInitialSize;
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::_ConstructItems(T* apFirstAddr, unsigned int aiCount) {
		for (unsigned int i = 0; i < aiCount; i++) {
			new (&apFirstAddr[i]) T{};
		}
	}

	template <typename T, std::uint32_t N>
	T* BSSimpleArray<T, N>::_Allocate(unsigned int aiSizeInItem) {
		return CdeclCall<T*>(0x00401000, aiSizeInItem);
	}

	template <typename T, std::uint32_t N>
	unsigned int BSSimpleArray<T, N>::Add(const T& aValue) {
		unsigned int uiIndex = AddUninitialized();
		_ConstructItems(&pBuffer[uiIndex], 1);
		pBuffer[uiIndex] = aValue;
		return uiIndex;
	}

	template <typename T, std::uint32_t N>
	unsigned int BSSimpleArray<T, N>::AddUninitialized() {
		if (IsFull()) {
			if (iReservedSize) {
				unsigned int uiIdealSize = GetGrowthSize();
				_ReallocateBuffer(uiIdealSize, iSize);
				iReservedSize = uiIdealSize;
			}
			else {
				pBuffer = _Allocate(4);
				iReservedSize = 4;
			}
		}

		return this->iSize++;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::Insert(unsigned int aiIndex, const T& aValue) {
		if (aiIndex == iSize) {
			Add(aValue);
		}
		else {
			if (iSize == iReservedSize) {
				unsigned int uiAllocateSize = GetGrowthSize();
				T* pNewBuffer = _Allocate(uiAllocateSize);
				_MoveItems(pNewBuffer, pBuffer, aiIndex);
				_ConstructItems(&pNewBuffer[aiIndex], 1);
				_MoveItems(&pNewBuffer[aiIndex + 1], &pBuffer[aiIndex], iSize - aiIndex);
				DeallocateBuffer();
				pBuffer = pNewBuffer;
				iReservedSize = uiAllocateSize;
			}
			else {
				_MoveItems(&pBuffer[aiIndex + 1], &pBuffer[aiIndex], iSize - aiIndex);
				_ConstructItems(&pBuffer[aiIndex], 1);
			}

			++iSize;
			pBuffer[aiIndex] = aValue;
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::SortedInsert(const T& aValue, int (*apFunc)(const void* apVal1, const void* apVal2)) {
		unsigned int iLowerBound = 0;
		unsigned int iUpperBound = iSize - 1;

		while (iLowerBound <= iUpperBound && iUpperBound != -1) {
			unsigned int iMidPoint = iLowerBound + ((iUpperBound - iLowerBound) >> 1);
			int iCompare = apFunc(&aValue, &pBuffer[iMidPoint]);
			if (iCompare == -1) {
				iUpperBound = iMidPoint - 1;
			}
			else if (iCompare == 1) {
				iLowerBound = iMidPoint + 1;
			}
			else {
				iLowerBound = iMidPoint;
				iUpperBound = -1;
			}
		}

		Insert(iLowerBound, aValue);
	}

	template <typename T, std::uint32_t N>
	unsigned int BSSimpleArray<T, N>::GetGrowthSize() const {
		if (iReservedSize <= N) {
			return 2 * iReservedSize;
		}
		else {
			return iReservedSize + N;
		}
	}


	template <typename T, std::uint32_t N>
	unsigned int BSSimpleArray<T, N>::GetShrinkSize() const {
		if (iReservedSize <= 2*N) {
			return iReservedSize >> 1;
		}
		else {
			return iReservedSize - N;
		}
	}


	template <typename T, std::uint32_t N>
	bool BSSimpleArray<T, N>::IsFull() const {
		return iSize == iReservedSize;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::_MoveItems(T* apDestAddr, T* apSrcAddr, unsigned int aiCount) {
		if (aiCount) {
			if (apDestAddr >= apSrcAddr) {
				if (apDestAddr > apSrcAddr) {
					for (unsigned int i = aiCount - 1; i != -1; --i) {
						memcpy(&apDestAddr[i], &apSrcAddr[i], sizeof(T));
					}
				}
			}
			else {
				for (unsigned int i = 0; i < aiCount; ++i) {
					memcpy(&apDestAddr[i], &apSrcAddr[i], sizeof(T));
				}
			}
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::DeallocateBuffer() {
		_Deallocate(pBuffer);
		pBuffer = nullptr;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::_Deallocate(T* apBuffer) {
		CdeclCall<void>(0x00401030, apBuffer);
	}

	template <typename T, std::uint32_t N>
	T* BSSimpleArray<T, N>::_Reallocate(T* apBuffer, unsigned int aiNewSizeInItem) {
		return CdeclCall<T*>(0x0098F280, apBuffer, aiNewSizeInItem);
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::_ReallocateBuffer(unsigned int aiNewSize, unsigned int aiCopyCount) {
		if (pBuffer) {
			if (aiCopyCount == iReservedSize) {
				pBuffer = _Reallocate(pBuffer, aiNewSize);
			}
			else {
				T* pNewBuffer = _Allocate(aiNewSize);
				_MoveItems(pNewBuffer, pBuffer, aiCopyCount);
				DeallocateBuffer();
				pBuffer = pNewBuffer;
			}
		}
		else {
			pBuffer = _Allocate(aiNewSize);
			iReservedSize = aiNewSize;
		}
	}

	template <typename T, std::uint32_t N>
	T& BSSimpleArray<T, N>::Get(unsigned int aiIndex) const {
		return pBuffer[aiIndex];
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::_DestructItems(T* apFirstAddr, unsigned int aiCount) {
		for (unsigned int i = 0; i < aiCount; i++) {
			apFirstAddr[i].~T();
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::RemoveFast(unsigned int aiIndex, unsigned int aiHowMany) {
		unsigned int iLeftover = (std::min)(iSize - aiIndex - aiHowMany, aiHowMany);
		_DestructItems(&pBuffer[aiIndex], aiHowMany);
		_MoveItems(&pBuffer[aiIndex], &pBuffer[iSize - iLeftover], iLeftover);
		iSize -= aiHowMany;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::Clear(bool abFreeMemory) {
		if (pBuffer) {
			_DestructItems(pBuffer, iSize);
			if (abFreeMemory) {
				DeallocateBuffer();
				pBuffer = nullptr;
				iReservedSize = 0;
			}
			iSize = 0;
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::SetReservedSize(unsigned int aiReserveSize) {
		if (aiReserveSize != iReservedSize) {
			if (aiReserveSize < iSize) {
				_DestructItems(&pBuffer[aiReserveSize], iSize - aiReserveSize);
				iSize = aiReserveSize;
			}
			_ReallocateBuffer(aiReserveSize, iSize);
			iReservedSize = aiReserveSize;
		}
	}

	template <typename T, std::uint32_t N>
	bool BSSimpleArray<T, N>::HasExcessCapacity() const {
		if (iReservedSize <= N) {
			if (iSize <= iReservedSize >> 2) {
				return true;
			}
		}
		else if (iSize <= iReservedSize - (2 * N)) {
			return true;
		}

		return false;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::Remove(unsigned int aiIndex, bool abShrink) {
		if (abShrink && HasExcessCapacity()) {
			unsigned int uiShrinkSize = GetShrinkSize();
			T* pNewBuffer= _Allocate(uiShrinkSize);
			_MoveItems(pNewBuffer, pBuffer, aiIndex);
			_DestructItems(&pBuffer[aiIndex], 1);
			// Vanilla bug, count should but iSize - aiIndex - 1 but is iSize - 1
			_MoveItems(&pNewBuffer[aiIndex], &pBuffer[aiIndex + 1], iSize - aiIndex - 1);
			DeallocateBuffer();
			pBuffer = pNewBuffer;
			iReservedSize = uiShrinkSize;
		}
		else {
			_DestructItems(&pBuffer[aiIndex], 1);
			_MoveItems(&pBuffer[aiIndex], &pBuffer[aiIndex + 1], iSize - aiIndex - 1);
		}
		--iSize;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::SetSize(unsigned int aiNewSize, bool abFreeMemory) {
		if (aiNewSize) {
			if (aiNewSize <= iReservedSize) {
				if (aiNewSize >= iSize) {
					_ConstructItems(&pBuffer[iSize], aiNewSize - iSize);
					iSize = aiNewSize;
				}
				else {
					_DestructItems(&pBuffer[aiNewSize], iSize - aiNewSize);
					iSize = aiNewSize;
					if (abFreeMemory && aiNewSize <= iReservedSize >> 2) {
						_ReallocateBuffer(aiNewSize, aiNewSize);
						iReservedSize = aiNewSize;
					}
				}
			}
			else
			{
				if (iReservedSize) {
					_ReallocateBuffer(aiNewSize, iSize);
				}
				else {
					pBuffer = _Allocate(aiNewSize);
				}
				iReservedSize = aiNewSize;
				_ConstructItems(&pBuffer[iSize], aiNewSize - iSize);
				iSize = aiNewSize;
			}
		}
		else
		{
			Clear(abFreeMemory);
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::RemoveRange(unsigned int aiIndex, unsigned int aiHowMany, bool abShrink) {
		if (aiHowMany < iSize) {
			if (abShrink && HasExcessCapacity()) {
				unsigned int uiShrinkSize = GetShrinkSize();
				T* pNewBuffer = _Allocate(uiShrinkSize);
				_MoveItems(pNewBuffer, pBuffer, aiIndex);
				_DestructItems(&pBuffer[aiIndex], aiHowMany);
				// Vanilla bug, count should but iSize - aiIndex - aiHowMany but is iSize - aiHowMany
				_MoveItems(&pNewBuffer[aiIndex], &pBuffer[aiIndex + aiHowMany], iSize - aiIndex - aiHowMany); 
				DeallocateBuffer();
				pBuffer = pNewBuffer;
				iReservedSize = uiShrinkSize;
			}
			else {
				_DestructItems(&pBuffer[aiIndex], aiHowMany);
				_MoveItems(&pBuffer[aiIndex], &pBuffer[aiIndex + aiHowMany], iSize - aiIndex - aiHowMany);
			}
			iSize -= aiHowMany;
		}
		else {
			Clear(abShrink);
		}
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::Reverse() {
		if (iSize) {
			unsigned int iStart = 0;
			for (unsigned int iEnd = iSize - 1; iStart < iEnd; --iEnd) {
				T temp = pBuffer[iStart];
				pBuffer[iStart] = pBuffer[iEnd];
				pBuffer[iEnd] = temp;
				++iStart;
			}
		}
	}

	template <typename T, std::uint32_t N>
	bool BSSimpleArray<T, N>::CompareBuffer(BSSimpleArray<T, N>* aDestArray) const {
		if (aDestArray->iSize != iSize) {
			return 0;
		}

		for (unsigned int iIndex = 0; iIndex < iSize; ++iIndex) {
			if (aDestArray->Get(iIndex) != Get(iIndex)) {
				return 0;
			}
		}

		return 1;
	}

	template <typename T, std::uint32_t N>
	void BSSimpleArray<T, N>::CopyTo(BSSimpleArray<T, N>* aDestArray) const {
		aDestArray->SetSize(iSize, 1);
		for (unsigned int iIndex = 0; iIndex < iSize; ++iIndex) {
			T* pSourceItem = &pBuffer[iIndex];
			aDestArray->Get(iIndex) = *pSourceItem;
		}
	}

	template <typename T, std::uint32_t N>
	template <typename K>
	bool BSSimpleArray<T, N>::DefaultEqualFunction(const T& aOne, const K& aTwo) {
		return aOne == aTwo;
	}

	template <typename T, std::uint32_t N>
	int BSSimpleArray<T, N>::QuickSortFunction(const void* apPtr1, const void* apPtr2) {
		auto result = apPtr1 <=> apPtr2;
		if (result > 0) {
			return 1;
		}
		else if (result < 0) {
			return -1;
		}

		return 0;
	}

	template <typename T, std::uint32_t N>
	template <typename K>
	unsigned int BSSimpleArray<T, N>::Find(const T& aValue, unsigned int aiStartIndex, bool (*apFunc)(const T& aOne, const K& aTwo)) const {
		while (aiStartIndex < iSize) {
			if (pBuffer && apFunc(pBuffer[aiStartIndex], aValue)) {
				return aiStartIndex;
			}
			++aiStartIndex;
		}
		return -1;
	}

	template <typename T, std::uint32_t N>
	bool BSSimpleArray<T, N>::Contains(const T& aValue) const {
		return Find<T>(aValue, 0, DefaultEqualFunction);
	}
}

template class CommonLib::BSSimpleArray<CommonLib::ItemChange, 1024>;
template class CommonLib::BSSimpleArray<CommonLib::WeaponData, 1024>;
