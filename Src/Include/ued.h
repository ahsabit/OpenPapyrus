// UED.H
// Copyright (c) A.Sobolev 2023, 2024
//
#ifndef __UED_H
#define __UED_H

#include <ued-id.h>

class PPLogger;

class UED {
public:
	static constexpr bool IsMetaId(uint64 ued) { return (HiDWord(ued) == 1); }
	//
	// Descr: ���������� ���������� ��� ������, ��������� ��� �������� UED-�������� � 
	//   ����������� �� ����-��������������.
	//   ���� �������� meta �� �������� ����-���������������, �� ���������� 0.
	//
	static constexpr uint GetMetaRawDataBits(uint64 meta)
	{
		uint   result = 0;
		if(IsMetaId(meta)) {
			const uint32 meta_lodw = LoDWord(meta);
			if(meta_lodw & 0x80000000U)
				result = 56;
			else if(meta_lodw & 0x40000000U)
				result = 48;
			else 
				result = 32;
		}
		return result;
	}
	//
	// Descr: ���������� ���������� ��� ������, ��������� ��� �������� UED.
	//   ���� �������� �������� meta-��������������� ��� �� �������� �������� UED-���������,
	//   �� ���������� 0.
	//
	static constexpr uint GetRawDataBits(uint64 ued)
	{
		uint   result = 0;
		if(!IsMetaId(ued)) {
			const uint32 hi_dword = HiDWord(ued);
			if(hi_dword & 0x80000000U)
				result = 56;
			else if(hi_dword & 0x40000000)
				result = 48;
			else if(hi_dword)
				result = 32;
		}
		return result;
	}
	static constexpr uint64 GetMeta(uint64 ued)
	{
		uint64 result = 0ULL;
		if(IsMetaId(ued))
			result = UED_META_META; // meta
		else {
			const uint32 dw_hi = HiDWord(ued);
			const uint8  b_hi = static_cast<uint8>(dw_hi >> 24);
			if(b_hi & 0x80)
				result = (0x0000000100000000ULL | static_cast<uint64>(dw_hi & 0xff000000U));
			else if(b_hi & 0x40)
				result = (0x0000000100000000ULL | static_cast<uint64>(dw_hi & 0xffff0000U));
			else if(dw_hi)
				result = (0x0000000100000000ULL | static_cast<uint64>(dw_hi & 0x0fffffffU));
		}
		return result;
	}
	static constexpr bool BelongToMeta(uint64 ued, uint64 meta) { return (IsMetaId(meta) && GetMeta(ued) == meta); }
	static bool   GetRawValue(uint64 ued, uint64 * pRawValue);
	static bool   GetRawValue32(uint64 ued, uint32 * pRawValue);
	static constexpr uint32 GetRawValue32(uint64 ued)
	{
		const  uint  bits = GetRawDataBits(ued);
		assert(oneof4(bits, 56, 48, 32, 0));
		return (bits == 32) ? LoDWord(ued) : 0;
	}
	static uint64 ApplyMetaToRawValue(uint64 meta, uint64 rawValue);
	static uint64 ApplyMetaToRawValue32(uint64 meta, uint32 rawValue);

	static uint64 SetRaw_Int(uint64 meta, int64 val);
	static bool   GetRaw_Int(uint64 ued, int64 & rVal);
	static uint64 SetRaw_GeoLoc(const SGeoPosLL & rGeoPos);
	static bool   GetRaw_GeoLoc(uint64 ued, SGeoPosLL & rGeoPos);
	static uint64 SetRaw_PlanarAngleDeg(double deg);
	static bool   GetRaw_PlanarAngleDeg(uint64 ued, double & rDeg);
	static uint64 SetRaw_Color(const SColor & rC);
	static bool   GetRaw_Color(uint64 ued, SColor & rC);
	static uint64 SetRaw_SphDir(const SphericalDirection & rV);
	static bool   GetRaw_SphDir(uint64 ued, SphericalDirection & rV);

	static uint64 SetRaw_Ru_INN(const char * pT);
	static bool   GetRaw_Ru_INN(uint64 ued, SString & rT);
	static uint64 SetRaw_Ru_KPP(const char * pT);
	static bool   GetRaw_Ru_KPP(uint64 ued, SString & rT);
	static uint64 SetRaw_Ru_SNILS(const char * pT);
	static bool   GetRaw_Ru_SNILS(uint64 ued, SString & rT);
	static uint64 SetRaw_Ar_DNI(const char * pT);
	static bool   GetRaw_Ar_DNI(uint64 ued, SString & rT);
	static uint64 SetRaw_Cl_RUT(const char * pT);
	static bool   GetRaw_Cl_RUT(uint64 ued, SString & rT);
	static uint64 SetRaw_EAN13(const char * pT);
	static bool   GetRaw_EAN13(uint64 ued, SString & rT);
	static uint64 SetRaw_EAN8(const char * pT);
	static bool   GetRaw_EAN8(uint64 ued, SString & rT);
	static uint64 SetRaw_UPCA(const char * pT);
	static bool   GetRaw_UPCA(uint64 ued, SString & rT);
	static uint64 SetRaw_GLN(const char * pT);
	static bool   GetRaw_GLN(uint64 ued, SString & rT);

	static bool   _GetRaw_Time(uint64 ued, SUniTime_Internal & rT);
	static uint64 _SetRaw_Time(uint64 meta, const SUniTime_Internal & rT);
	static bool   _GetRaw_RangeDate(uint64 ued, DateRange & rT);
	static uint64 _SetRaw_RangeDate(const DateRange & rT);
private:
	enum {
		decstrf_LZ1           = 0x01,
		decstrf_LZ2           = 0x02,
		decstrf_LZ3           = 0x03,
		decstrf_RuInn_InvCheckDigit = 0x08,
		decstrf_ClRut_CtlK    = 0x08, // ��� ���������� RUT � �������� ������������ ������� - K
	};
	static uint64 Helper_SetRaw_PlanarAngleDeg(uint64 meta, double deg);
	static bool   Helper_GetRaw_PlanarAngleDeg(uint64 meta, uint64 ued, double & rDeg);
	static uint64 Helper_SetRaw_DecimalString(uint64 meta, const char * pT, uint flagsBits, uint flags);
	static bool   Helper_GetRaw_DecimalString(uint64 meta, uint64 ued, SString & rT, uint flagsBits, uint * pFlags);
};
//
// Descr: ������� �����, ����������� ������������� ����� ued-���������������
//
class UedSetBase : private SBaseBuffer {
public:
	DECL_INVARIANT_C();
	UedSetBase();
	UedSetBase(const UedSetBase & rS);
	~UedSetBase();
	UedSetBase & FASTCALL operator = (const UedSetBase & rS);
	bool   FASTCALL operator == (const UedSetBase & rS) const;
	UedSetBase & Z();
	int    FASTCALL Copy(const UedSetBase & rS);
	bool   FASTCALL IsEq(const UedSetBase & rS) const;
	uint   GetLimbCount() const { return LimbCount; }
	uint64 Get(uint idx) const;
	bool   Add(const uint64 * pUed, uint count, uint * pIdx);
	bool   Add(uint64 ued, uint * pIdx);
private:
	uint   LimbCount; // ����������� ���������� 64-������ ���������
};
//
// Descr: ������� ����� ���������� UED-��������
//
class SrUedContainer_Base : public SStrGroup {
public:
	//
	// Descr: ��������������� ���������, ���������� �������� UED �������������� � ��������������� ������.
	//   ����� ����������� ��� ������� �������� ������.
	//
	struct UedLocaleEntry {
		UedLocaleEntry() : Ued(0ULL), Locale(0U)
		{
		}
		UedLocaleEntry(uint64 ued, uint locale) : Ued(ued), Locale(locale)
		{
		}
		UedLocaleEntry & Z()
		{
			Ued = 0ULL;
			Locale = 0U;
			return *this;
		}
		uint64 Ued;
		uint32 Locale;
	};
	struct BaseEntry {
		uint64 Id;
		uint32 SymbHashId;
		uint32 LineNo; // ����� ������ ��������� �����, � ������� ���������� �����������.
	};
	struct TextEntry : public UedLocaleEntry {
		uint32 TextP;
		uint32 LineNo; // ����� ������ ��������� �����, � ������� ���������� �����������.
	};
	enum {
		canonfnIds = 1,
		canonfnProps = 2
	};
	static void MakeUedCanonicalName(int canonfn, SString & rResult, long ver);
	static long SearchLastCanonicalFile(int canonfn, const char * pPath, SString & rFileName);
	//
	// Descr: ������������ UED-���� ������ ver � ����������� � �������� pPath 
	//   �� ������� ������� � ������������ ����, ����������� � ��������� ����� � ��� �� ��������.
	//
	int    Verify(const char * pPath, long ver, SBinaryChunk * pHash) const;
	uint64 SearchSymb(const char * pSymb, uint64 meta) const;
	//
	// Descr: ����� ������� Recognize
	//
	enum {
		rfDraft       = 0x0001, // �� ������ ������������ ��������
		rfPrefixSharp = 0x0002  // ������� ��������� ������ ������������ ������ '#'
	};

	uint64 Recognize(SStrScan & rScan, uint64 implicitMeta, uint flags) const;
protected:
	SrUedContainer_Base();
	~SrUedContainer_Base();
	//
	// Descr: ����� ������� ReadSource
	//
	enum {
		rsfDebug       = 0x0001,
		rsfCompileTime = 0x0002  // ����������� �������� ������ � ������ ��������������� ����������. ���� �� ����������, ��
			// ���������� �������������� � run-time-������.
	};

	int    ReadSource(const char * pFileName, uint flags, PPLogger * pLogger);
	int    ReadProps(const char * pFileName);
	int    WriteSource(const char * pFileName, const SBinaryChunk * pPrevHash, SBinaryChunk * pHash);
	int    WriteProps(const char * pFileName, const SBinaryChunk * pPrevHash, SBinaryChunk * pHash);
	uint64 SearchBaseSymb(const char * pSymb, uint64 meta) const;
	bool   SearchBaseId(uint64 id, SString & rSymb) const;
	bool   SearchSymbHashId(uint32 symbHashId, SString & rSymb) const;
	//
	// Descr: ���������� ������� ����������� ������� �������� � ����� �� ���� ���������:
	//   symb ��� meta.symb
	// Note: ������� �� �������� ���������� ������������� ��������. �� ����
	//   �� ���� �� ����� �����������.
	// Returns:
	//   0 - �� ������� ���������� ������
	//   1 - ��������� ��������� ������
	//   2 - ��������� ������� meta.symb
	//
	int    Helper_RecognizeSymb(SStrScan & rScan, uint flags, SString & rMeta, SString & rSymb) const;
	//
	// Descr: ������������ �������� ��������, ����������� � ����������������� ����
	//   ��� ��� �������� ��������� ������� ������ � ���� ������� ���������.
	//
	struct ProtoProp : public UedLocaleEntry {
		ProtoProp() : LineNo(0)
		{
		}
		// ���� �������� ������ ��� ��������� ����������� ��������, �� UedLocaleEntry::Locale - ������������� ���������������� ������.
		uint32 LineNo; // ����� ������, �� ������� ���������� ����������� ��������.
		StringSet Prop; // The first item - property symb, other - arguments
	};
	class ProtoPropList_SingleUed : public UedLocaleEntry, public TSCollection <ProtoProp> {
	public:
		ProtoPropList_SingleUed(uint64 ued, uint localeId);
		void Init(uint64 ued, uint localeId);
		ProtoProp * Create();
	};
	class PropertyListParsingBlock {
	public:
		PropertyListParsingBlock();
		bool    IsStarted() const { return (Status > 0); }
		bool    Start(uint64 ued, int localeId);
		//
		// Returns:
		//   >0 - ������ �������� (���������� ����������� ������ '}')
		//   <0 - ������ �� ��������
		//    0 - error
		//
		int    Do(SrUedContainer_Base & rC, SStrScan & rScan);
		const  ProtoPropList_SingleUed & GetResult() const { return PL; }
	private:
		int    ScanProp(SrUedContainer_Base & rC, SStrScan & rScan);
		int    ScanArg(SrUedContainer_Base & rC, SStrScan & rScan, bool isFirst/*@debug*/);
		int    Status; // 0 - idle, 1 - started, 2 - in work (the first '{' was occured and processed)
		int    State;  // ���������� ������������� ��������� �������.
		ProtoPropList_SingleUed PL;
	};
	struct PropIdxEntry : public UedLocaleEntry {
		const void * GetHashKey(const void * pCtx, uint * pSize) const // hash-table support
		{
			ASSIGN_PTR(pSize, sizeof(Ued)+sizeof(Locale));
			return this;
		}
		LAssocArray RefList; // ������ ������� � PropertySet
	};
	//
	// Descr: �������� ��� �������, �� ������� ��������� �������� UED-�������.
	//
	class PropertySet : private UedSetBase {
	public:
		PropertySet();
		int    Add(const uint64 * pPropChunk, uint count, uint * pPos);
		uint   GetCount() const;
		int    Get(uint idx, uint count, UedSetBase & rList) const;
	private:
		LAssocArray PosIdx; // ������ �������: Key - ����� �������, Val - ���������� ��������� 
	};
	TSVector <BaseEntry> BL;
	TSVector <TextEntry> TL;
	TSCollection <ProtoProp> ProtoPropList; // compile-time
	TSHashCollection <PropIdxEntry> PropIdx;

	int    GetPropList(TSCollection <PropIdxEntry> & rList) const;
private:
	uint64 SearchBaseIdBySymbId(uint symbId, uint64 meta) const;
	int    GetLocaleIdBySymb(const char * pSymb, uint32 * pLocaleId, uint srcLineNo, PPLogger * pLogger) const;
	int    ReplaceSurrogateLocaleId(const SymbHashTable & rT, uint32 surrLocaleId, uint32 * pRealLocaleId, uint lineNo, PPLogger * pLogger);
	int    ReplaceSurrogateLocaleIds(const SymbHashTable & rT, PPLogger * pLogger);
	int    RegisterProtoPropList(const ProtoPropList_SingleUed & rList);
	int    ProcessProperties();
	int    Helper_PutProperties(const UedLocaleEntry & rUedEntry, const TSVector <uint64> & rRawPropList);
	//
	// Descr: ������, ����������� ���� �������� �� ����� standalone-�������� �������
	//
	bool   ReadSingleProp(SStrScan & rScan);
	uint64 LinguaLocusMeta;
	SymbHashTable Ht; // ���-������� �������� �� ������ BL
	uint   LastSymbHashId;
	PropertySet PropS;
};
//
// Descr: ����� ���������� UED-��������, ����������� ���������� ���������� � ������.
//
class SrUedContainer_Ct : public SrUedContainer_Base {
public:
	SrUedContainer_Ct();
	~SrUedContainer_Ct();
	int    Read(const char * pFileName, PPLogger * pLogger);
	int    Write(const char * pFileName, const SBinaryChunk * pPrevHash, SBinaryChunk * pHash);
	int    WriteProps(const char * pFileName, const SBinaryChunk * pPrevHash, SBinaryChunk * pHash);
	bool   GenerateSourceDecl_C(const char * pFileName, uint versionN, const SBinaryChunk & rHash);
	bool   GenerateSourceDecl_Java(const char * pFileName, uint versionN, const SBinaryChunk & rHash);
	//
	// Descr: ������������ this-��������� �� ������������������ �, ���� ������ ���������
	//   ���������� ������ (pPrevC != 0), �� ��������� ����������.
	//
	int    VerifyByPreviousVersion(const SrUedContainer_Ct * pPrevC, bool tolerant, PPLogger * pLogger);
};
//
// Descr: ����� ���������� UED-��������, ����������� ���������� ���������� � run-time'�
//
class SrUedContainer_Rt : public SrUedContainer_Base {
public:
	SrUedContainer_Rt();
	~SrUedContainer_Rt();
	int    Read(const char * pFileName);
	bool   GetSymb(uint64 ued, SString & rSymb) const;
};

enum {
	prcssuedfForceUpdatePlDecl = 0x0001,
	prcssuedfTolerant          = 0x0002,
	prcssuedfDebug             = 0x0004, // @v11.8.10
};

int ProcessUed(const char * pSrcFileName, const char * pOutPath, const char * pRtOutPath, const char * pCPath, const char * pJavaPath, uint flags, PPLogger * pLogger); // @v11.7.10

#endif // __UED_H