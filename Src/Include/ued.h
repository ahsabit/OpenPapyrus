// UED.H
// Copyright (c) A.Sobolev 2023
//
#ifndef __UED_H
#define __UED_H

class PPLogger;

class UED {
public:
	static bool IsMetaId(uint64 ued) { return (HiDWord(ued) == 1); }
	//
	// Descr: ���������� ���������� ��� ������, ��������� ��� �������� UED-�������� � 
	//   ����������� �� ����-��������������.
	//   ���� �������� meta �� �������� ����-���������������, �� ���������� 0.
	//
	static uint GetMetaRawDataBits(uint64 meta);
	//
	// Descr: ���������� ���������� ��� ������, ��������� ��� �������� UED.
	//   ���� �������� �������� meta-��������������� ��� �� �������� �������� UED-���������,
	//   �� ���������� 0.
	//
	static uint GetRawDataBits(uint64 ued);
	static bool BelongToMeta(uint64 ued, uint64 meta)
	{
		//return (IsMetaId(meta) && ((ued >> 32) & 0x00000000ffffffffULL) == (meta & 0x00000000ffffffffULL));
		return (IsMetaId(meta) && GetMeta(ued) == meta);
	}
	static uint64 GetMeta(uint64 ued);
	static bool   GetRawValue(uint64 ued, uint64 * pRawValue);
	static bool   GetRawValue32(uint64 ued, uint32 * pRawValue);
	static uint64 ApplyMetaToRawValue(uint64 meta, uint64 rawValue);
	static uint64 ApplyMetaToRawValue32(uint64 meta, uint32 rawValue);
	static uint64 SetRaw_GeoLoc(const SGeoPosLL & rGeoPos);
	static bool   GetRaw_GeoLoc(uint64 ued, SGeoPosLL & rGeoPos);
	static uint64 SetRaw_PlanarAngleDeg(double deg);
	static bool   GetRaw_PlanarAngleDeg(uint64 ued, double & rDeg);
	static uint64 SetRaw_Color(const SColor & rC);
	static bool   GetRaw_Color(uint64 ued, SColor & rC);

	static uint64 SetRaw_Ru_INN(const char * pT);
	static bool   GetRaw_Ru_INN(uint64 ued, SString & rT);
	static uint64 SetRaw_Ru_KPP(const char * pT);
	static bool   GetRaw_Ru_KPP(uint64 ued, SString & rT);
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
private:
	static uint64 Helper_SetRaw_PlanarAngleDeg(uint64 meta, double deg);
	static bool   Helper_GetRaw_PlanarAngleDeg(uint64 meta, uint64 ued, double & rDeg);
};
//
// Descr: ������� ����� ���������� UED-��������
//
class SrUedContainer_Base : public SStrGroup {
public:
	struct BaseEntry {
		uint64 Id;
		uint32 SymbHashId;
		uint32 LineNo; // @v11.7.8 ����� ������ ��������� �����, � ������� ���������� �����������.
	};
	struct TextEntry {
		uint64 Id;
		uint32 Locale;
		uint32 TextP;
		uint32 LineNo; // @v11.7.8 ����� ������ ��������� �����, � ������� ���������� �����������.
	};
	static void MakeUedCanonicalName(SString & rResult, long ver);
	static long SearchLastCanonicalFile(const char * pPath, SString & rFileName);
	//
	// Descr: ������������ UED-���� ������ ver � ����������� � �������� pPath 
	//   �� ������� ������� � ������������ ����, ����������� � ��������� ����� � ��� �� ��������.
	//
	int    Verify(const char * pPath, long ver, SBinaryChunk * pHash) const;
protected:
	SrUedContainer_Base();
	~SrUedContainer_Base();
	int    ReadSource(const char * pFileName, PPLogger * pLogger);
	int    WriteSource(const char * pFileName, const SBinaryChunk * pPrevHash, SBinaryChunk * pHash);
	uint64 SearchBaseSymb(const char * pSymb, uint64 meta) const;
	bool   SearchBaseId(uint64 id, SString & rSymb) const;
	bool   SearchSymbHashId(uint32 symbHashId, SString & rSymb) const;

	TSVector <BaseEntry> BL;
	TSVector <TextEntry> TL;
private:
	uint64 SearchBaseIdBySymbId(uint symbId, uint64 meta) const;
	int    ReplaceSurrogateLocaleIds(const SymbHashTable & rT, PPLogger * pLogger);
	uint64 LinguaLocusMeta;
	SymbHashTable Ht; // ���-������� �������� �� ������ BL
	uint   LastSymbHashId;
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
	uint64 SearchSymb(const char * pSymb, uint64 meta) const;
	bool   GetSymb(uint64 ued, SString & rSymb) const;
};

enum {
	prcssuedfForceUpdatePlDecl = 0x0001,
	prcssuedfTolerant          = 0x0002
};

int ProcessUed(const char * pSrcFileName, const char * pOutPath, const char * pRtOutPath, const char * pCPath, const char * pJavaPath, uint flags, PPLogger * pLogger); // @v11.7.10

#endif // __UED_H