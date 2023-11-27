// STYLOCONDUIT.H
// Part of StyloConduit project
// Copyright (c) A.Sobolev 2005, 2006, 2007, 2009, 2010, 2011, 2020, 2023
//
#ifndef __STYLOCONDUIT_H
#define __STYLOCONDUIT_H

#ifndef _PAPYRUS
	#ifndef  __GENERIC_MAIN_CONDUIT__
		#define __GENERIC_MAIN_CONDUIT__
	#endif
	#define GENERIC_CONDUIT_VERSION 0x00000100
	#include <crtdbg.h>
	/* (replaced with CXX_EXPORT) #ifndef ExportFunc
		#define ExportFunc __declspec( dllexport )
	#endif*/
#else
	#define SYNC_API
#endif

#include <slib.h>
#include <snet.h>
#include <dbf.h>
// Palm CDK includes (papyrus\src\include\palmcdk\403) {

// @v11.8.12 #include <palmcdk\403\syncmgr.h>

// @v10.9.6 #include <palmcdk\403\condapi.h>
// @v10.9.6 #include <palmcdk\403\logstrng.h>
// @v10.9.6 #include <palmcdk\403\hslog.h>
// @v10.9.6 #include <palmcdk\403\palm_cmn.H>
// @v11.8.12 #include <palmcdk\403\userdata.h>
// } Palm CDK includes

#include <stylopalm.h>
//
// �������:
//   host file: sp_cli.dbf, sp_cliad.dbf
//   palm file: Client.tbl
//   index: {ClID.idx: ID}, {ClNam.idx: Name}
// ����� �� ��������:
//   host file: sp_clidb.dbf
//   palm file: CliDebt.tbl
//   index: {CdIdDtC.idx: ClientID, Dt, Code}
// �������:
//   host filt: sp_sell.dbf
//   palm file: CliSell.tbl -- Sorted {ClientID, ClientAddrID, GoodsID}
//   index: {SellCAG.idx: ClientID, ClientAddrID, GoodsID}
//
// ������ �������:
//   host file: sp_ggrp.dbf
//   palm file: GoodsGrp.tbl
//   index: {GroupID.idx:ID}, {GroupNam.idx: Name}
// ���� ���������:
//   host file: sp_quotk.dbf
//   palm file: QuotKind.tbl (order:QuotKind::Name)
// ������:
//   host file: sp_goods.dbf
//   palm file: Goods.tbl
//   index: {GdsID.idx:ID}, {GdsName.idx:Name}, {GdsGNam.idx:GrpID, Name},
//            {GdsGID.idx:GrpID, ID}, {GdsCode.idx:Code}
// ��������� ����������:
//   host file: sp_bill.dbf
//   palm file: OrdHdr.tbl
// ������ ����������:
//   host file: sp_bitem.dbf
//   palm file: OrdLine.tbl
// ������:
//   host file: sp_todo.dbf
//   palm file:
//     ToDoDb (property of std application TODO)
//     ToDoSPII.dat (������������ ����� ���������������� ����� �� ��������� ���������� �
//       ���������������� ��������������� ������� �� Palm'�)
// ��������� ��������������:
//   host file: sp_invh.dbf
//   palm file: InvHdr.tbl
// �������������� �� ��������:
//   host file: sp_invl.dbf
//   palm file: InvLine.tbl
//
#ifndef __GENERIC_MAIN_CONDUIT__
extern "C" {
	typedef  long (*PROGRESSFN) (char *);
}
#else
extern "C" {
	typedef  long (*PROGRESSFN) (char *);
	CXX_EXPORT long OpenConduit(PROGRESSFN, CSyncProperties&);
	CXX_EXPORT long GetConduitName(char *,WORD);
	CXX_EXPORT DWORD GetConduitVersion();
	CXX_EXPORT long ConfigureConduit(CSyncPreference& pref);
	CXX_EXPORT long GetConduitInfo(ConduitInfoEnum infoType, void *pInArgs, void *pOut, DWORD *dwOutSize);
}
#endif

class CSyncProperties; // @v11.8.12 @stub
class CDbCreateDB; // @v11.8.12 @stub
class CUserIDInfo; // @v11.8.12 @stub

#define DB_CREATOR 'SPII'
#define DB_TYPE    'DATA'

#define EXPIMP_EXPORT 0x0001
#define EXPIMP_IMPORT 0x0002

#ifndef SETFLAG
	#define SETFLAG(v,f,b) {if(b) {(v)|=(f);} else {(v)&=~(f);}}
#endif
//
// @v11.8.12 (from PalmCDK\403\SYNCMGR.H ) {
//
//  Used by all the Record Oriented API's. Houses the DT_Link version
//  of a database's record layout, specifically that of the remote device.
//  Raw bytes will be formatted into this structure by the DTLinkConverter
//  object which resides inside of each Conduit.DLL.
//
class CRawRecordInfo {
public:
	BYTE	m_FileHandle;		// Supplied by caller
	DWORD	m_RecId;				// Supplied by caller when reading or deleting records by record id; supplied by
	// caller as the resource type when deleting a resource; filled in
	// by HH when reading (unique record id for records and resource type for resources).
	
	WORD	m_RecIndex;			// Supplied by caller when reading records or resources by index; supplied by caller
	// as the resource id when deleting a resource; filled in by handheld as the resource
	// id when reading a resource; filled in by HH when reading a record using Sync API v2.1
	// or later. 
	BYTE	m_Attribs;			// Filled in by HH when reading, and by caller when writing
	short	m_CatId;				// Filled in by HH when reading, and by caller when writing
	int	m_ConduitId;		// Ignore
	DWORD	m_RecSize;			// When reading, filled in by HH with the actual record/resource size,
	// which might be bigger than buffer size m_TotalBytes (in this
	// case, only the first m_TotalBytes of record data will be copied
	// to caller's buffer by Sync API v2.1 or later, and NOTHING will
	// be copied by Sync API before v2.1).  When writing, filled in by
	// caller with record data size (same as m_TotalBytes).

	// ****NOTE that m_TotalBytes is defined as WORD, meaning that only
	// records and resources under 64K may be read or written using this
	// API (the actual maximum is ~63.8K bytes).
	WORD	m_TotalBytes;		// Supplied by caller: buffer size for reading; record data size for writing
	BYTE*	m_pBytes;			// Buffer allocated by caller for reading or writing
	DWORD	m_dwReserved;		// Reserved	- set to NULL
};
// } @v11.8.12

int32  dbltointmoney(double);
int32  dbltopalmintmoney(double);
double palmintmoneytodbl(int32);
long   OpenPalmTable(const char * pName, uint8 & rHandle, CRawRecordInfo * pRecInfo);
//
// ��������� ����������, ���� !0, �� ��������� �������� ������� �������� ��������� ������
// �� Palm. ��� �������.
//
extern int ForceExportObsoleteData; // @defined(StyloConduit.cpp)

#define DB_CLIENT   0
#define DB_GOODS    1
#define DB_GOODSGRP 2
#define DB_CFG      3

int pwr10(int x);

void WaitPercent(PROGRESSFN pFn, long iterN, long itersCount, const char * pMsg);
char * convert_str(const char * pSrcBuf, int shrink, char * pDestBuf, size_t destBufLen);
char * get_str_from_dbfrec(DbfRecord * pRec, int fldN, int shrink, char * pDestBuf, size_t destBufLen);
int ScCreateDbfTable(DbfTable *, int numFlds, const DBFCreateFld * pFlds, const char * pLogFile);
int ScCheckDbfOpening(DbfTable *, const char * pLogFile);
int ScAddDbfRec(DbfTable * pTbl, DbfRecord * pRec, const char * pLogFile);

class PalmTcpExchange;

struct SpiiExchgContext {
	SpiiExchgContext(PalmTcpExchange * pPte) : P_Pte(pPte), TransmitComprFile(0), LastErr(0), PackedFileHdrOffs(0)
	{
		HostCfg.Init();
		PalmCfg.Init();
	}
	PalmTcpExchange * P_Pte;     // @notowned
	int    TransmitComprFile;    // ���������� ��� ��� ������ ����
	long   LastErr;              // ��� ��������� ������
	uint32 PackedFileHdrOffs;    // �������� ������ ���������� ������������ �����
	SString CompressFile;        // ��� ������� ����� �� �����
	SString PalmCompressedFile;  // ��� ������� ����� �� ���
	SString LogFile;             // ��� ��� �����
	PalmConfig HostCfg;          // ������������ �� �����
	PalmConfig PalmCfg;          // ������������ �� ���
};
//
//
//
class SyncTable {
public:
	struct Stat {
		uint32 NumRecs;
		uint32 TotalSize;
		uint32 DataSize;
	};
	int    Find(const char * pName, Stat *, int fromPalmCompressedFile = 0);
	long   GetLastErr() { return P_Ctx ? P_Ctx->LastErr : 0; }
	// @v11.7.8 static int  TransmitCompressedFile(PROGRESSFN, SpiiExchgContext *);
	static int  ReceivePalmCompressedFile(PROGRESSFN, SpiiExchgContext *);
	int  DeleteTable(const char * pName);

	enum {
		oReadOnly = 0x0001,
		oCreate   = 0x0002
	};
	SyncTable(int compress, int palmCompressed, SpiiExchgContext *);
	~SyncTable();
	int Open(const char * pName, long flags /* SyncTable::oXXX */);
	//
	// ������� ������� pName � ��������� �� � ������ ������/������
	//
	int Create(const char * pName);
	int Close();
	int Reopen(long delta, long recsCount); // if delta < 0, then delta = SyncTable::DefReopenDelta
	int ReadRecByIdx(uint16 idx, uint32 * pRecId, void * pBuf, size_t * pBufLen /* IN/OUT */);
	int ReadRecByID(uint32 recId, void * pBuf, size_t * pBufLen  /* IN/OUT */);
	int AddRec(uint32 * pRecId, void * pBuf, size_t bufLen);
	int UpdateRec(uint32 recId, void * pBuf, size_t bufLen);
	int PurgeAll();

	static void LogMessage(const char * pLogFile, const char * pMsg);
protected:
	void   LogError(const char * pFuncName);
	int    StartTableInTempFile();
	int    FinishTableInTempFile();
	int    WriteRecToTempFile(void * pBuf, size_t bufLen);
	int    ReadRecFromTempFile(void * pBuf, size_t * pBufLen);

	long   DefReopenDelta;
	char   TblName[64];
	SpiiDbHandler Handle;
	CRawRecordInfo RecInfo;
	int    Compress;
	int    PalmCompressed;  // ������ �� ����� ������� � ������ ����, ��������� ����������
	uint32 TempFileHdrOffs; // �������� �� ������ ���������� ����� �� ��������� �������.
	uint32 NumRecs; // ���������� �������, ����������� �������� AddRec
	SpiiExchgContext * P_Ctx;
};
//
// SCDB - StyloConduit Database
// �� ������ ������� � Palm'� - ���� ������ SCDBObject
//
//
struct SCDBTblEntry {
	const  char * P_Name;
	uint   NumIndexes;
	int    TimeStampOffs; // �������� �� ���� LDATETIME � ��������� PalmConfig, ������������� ����� �����������
		// ����������� �� Palm ������� ������. -1 - �� ����������
		// @v8.9.8 size_t-->int
	long   AcceptMask;    // �����, ������������ ������������� �������� ������ �� Palm (PalmConfig::Flags)
		// ���� 0, �� �� �����������.
	CompFunc Cf; // ������� ��������� ��� ������������ �������
};

class SCDBObject {
public:
	SCDBObject(SpiiExchgContext * pCtx) : P_Ctx(pCtx)
	{
	}
	virtual ~SCDBObject() {}
	virtual int Init(const char * pExpPath, const char * pImpPath) { return -1; }
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps) { return -1; }
	virtual int Import(PROGRESSFN pFn, CSyncProperties * pProps) { return -1; }
	virtual int Finish() { return -1; }
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const { return 0; }
	virtual const char * GetUserMnemonic() const { return ""; }
	virtual int PutHdrRec(void * pRec) { return -1; }
	virtual int PutLineRec(void * pRec) { return -1; }

	int    ExportIndex(PROGRESSFN pFn, const char * pDbName, SVector * pAry, CompFunc cf); // @v9.8.4 SArray-->SVector
	int    InitTable(SyncTable *);
protected:
	SyncTable * OpenTable();
	int    ExportIndexes(PROGRESSFN pFn, SVector * pAry); // @v9.8.4 SArray-->SVector
	//static const PalmConfig * P_HostCfg;
	//static const PalmConfig * P_PalmCfg;
	SpiiExchgContext * P_Ctx;
};

class SCDBObjConfig : public SCDBObject {
public:
	SCDBObjConfig(SpiiExchgContext * pCtx);
	~SCDBObjConfig();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "CONFIG"; }

	const PalmConfig * GetPalmConfig() const { return &PalmData; }
	const PalmConfig * GetHostConfig() const { return &HostData; }
private:
	void   PalmToHost(PalmConfig * pCfg);
	void   HostToPalm(PalmConfig * pCfg);

	PalmConfig HostData;
	PalmConfig PalmData; // ����������� �������� Init
};

class SCDBObjClient : public SCDBObject {
public:
	struct IdxRec {
		DWORD  RecID;
		uint32 ID;
		char   Name[50];
	};
	SCDBObjClient(SpiiExchgContext * pCtx);
	~SCDBObjClient();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const;
	virtual const char * GetUserMnemonic() const { return "CLIENT"; }
private:
	struct ClientAddr {
		int32  ID;
		char   Loc[64];
	};
	struct PalmRec {
		uint32 ID;
		uint32 QuotKindID; // -> QuotKind.ID
		int32  Debt;       // ����� ���� �������
		char   Code[16];
		char   Name[48];
		int16  AddrCount;
		//ClientAddr P_Addrs[AddrCount]; variable tail
	};
	struct PalmRec700 {
		uint32 ID;
		uint32 QuotKindID; // -> QuotKind.ID
		int32  Debt;       // ����� ���� �������
		char   Code[16];
		char   Name[48];
		int32  Flags;
		int16  AddrCount;
		//ClientAddr P_Addrs[AddrCount]; variable tail
	};
	struct AddrItem {
		long   CliID;
		long   AdrID;
		char   Addr[64];
	};
	int    LoadAddrList();
	int    GetAddressList(long cliID, TSVector <ClientAddr> *);
	SCDBObjClient::PalmRec * AllocPalmRec(uint addrCount, size_t * pBufLen);
	SCDBObjClient::PalmRec700 * AllocPalmRec700(uint addrCount, size_t * pBufLen);

	DbfTable * P_CliTbl;
	DbfTable * P_AdrTbl;
	TSVector <AddrItem> AddrList;
	TSVector <IdxRec> IdxList;
};

class SCDBObjClientDebt : public SCDBObject {
public:
	struct IdxRec {
		DWORD  RecID;
		int32  CliID;
		int32  Dt;
		char   Code[10];
	};
	struct TempRec {
		int32  ClientID;
		uint16 Dt; // since 31/12/1995
		char   Code[10];
		int32  Amount;
		int32  Debt;
	};

	SCDBObjClientDebt(SpiiExchgContext * pCtx);
	~SCDBObjClientDebt();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "CLIENT DEBT"; }
private:
	struct PalmRec {
		int32  ClientID;
		uint16 Dt; // since 31/12/1995
		char   Code[10];
		int32  Amount;
		int32  Debt;
	};
	SCDBObjClientDebt::PalmRec * AllocPalmRec(size_t * pBufLen);
	int ReadData();

	DbfTable * P_CliDebtTbl;
	TSVector <TempRec> TempList;
	TSVector <IdxRec> IdxList;
};

class SCDBObjSell : public SCDBObject {
public:
	struct IdxRec {
		DWORD  RecID;
		int32  CliID;
		int32  AddrID;
		int32  GoodsID;
	};
	struct TempRec {
		int32  ClientID;     // -> Client.ID
		int32  ClientAddrID; // -> ClientAddr.ID
		int32  GoodsID;      // -> GoodsID
		int16  Date;
		int32  Qtty;
	};

	SCDBObjSell(SpiiExchgContext * pCtx);
	~SCDBObjSell();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "SELL"; }
private:
	struct SalesItem {
		uint16 Date; // since 31/12/1995
		int32  Qtty;
	};
	struct PalmRec {
		int32  ClientID;     // -> Client.ID
		int32  ClientAddrID; // -> ClientAddr.ID
		int32  GoodsID;      // -> GoodsID
		int16  ItemsCount;
		//SalesItem Items[1];
	};
	SCDBObjSell::PalmRec * AllocPalmRec(const TSVector <TempRec> * pPool, size_t * pBufLen);
	int    GetNextPool(uint * pCurPos, TSVector <TempRec> * pPool);
	int    ReadData();
	int    LogErrRec(const PalmRec *);

	DbfTable * P_Tbl;
	TSVector <TempRec> TempList;
	TSVector <IdxRec> IdxList;
};

class SCDBObjGoodsGrp : public SCDBObject {
public:
	struct IdxRec {
		DWORD  RecID;
		int32  ID;
		char   Name[64];
	};
	SCDBObjGoodsGrp(SpiiExchgContext * pCtx);
	~SCDBObjGoodsGrp();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const;
	virtual const char * GetUserMnemonic() const { return "GOODSGROUP"; }
private:
	struct PalmRec {
		int32  ID;
		int16  Code;
		char   Name[48];
	};
	struct PalmRec156 {
		int32  ID;
		int16  Code;
		char   Name[64];
	};
	SCDBObjGoodsGrp::PalmRec * AllocPalmRec(size_t * pBufLen);
	SCDBObjGoodsGrp::PalmRec156 * AllocPalmRec156(size_t * pBufLen);

	DbfTable * P_GGrpTbl;
	TSVector <IdxRec> IdxList;
};

class SCDBObjGoods : public SCDBObject {
public:
	struct IdxRec {
		DWORD  RecID;    // Palm format
		int32  ID;       // Host format
		int32  GrpID;    // Host format
		char   Name[64]; // Host format
		char   Code[16]; // Host format
	};
	struct QuotKind { // Host format
		int32  ID;
		int32  N;
		char   Name[32];
	};

	SCDBObjGoods(SpiiExchgContext * pCtx);
	~SCDBObjGoods();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const;
	virtual const char * GetUserMnemonic() const { return "GOODS"; }
private:
	struct PalmRec { // Palm format
		int32  ID;
		char   Code[16];
		int32  GoodsGrpID; // -> GoodsGroup.ID
		int32  Pack;       // ������� ��������
		int32  Price;      // ������� ����
		int32  Rest;       // �������
		char   Name[48];
		int16  QuotCount;
	};
	struct PalmRec156 { // Palm format
		int32  ID;
		char   Code[16];
		int32  GoodsGrpID; // -> GoodsGroup.ID
		int32  Pack;       // ������� ��������
		int32  Price;      // ������� ����
		int32  Rest;       // �������
		char   Name[64];
		int16  QuotCount;
	};
	struct PalmRec500 { // Palm format
		int32  ID;
		char   Code[16];
		int32  GoodsGrpID;   // -> GoodsGroup.ID
		int32  Pack;         // ������� ��������
		int32  Price;        // ������� ����
		int32  Rest;         // �������
		char   Name[64];
		int32  BrandID;      // �����
		int32  BrandOwnerID; // �������� ������
		int16  QuotCount;
	};
	struct PalmRec800 { // Palm format
		int32  ID;
		char   Code[16];
		int32  GoodsGrpID;    // -> GoodsGroup.ID
		int32  Pack;          // ������� ��������
		int32  Price;         // ������� ����
		int32  Rest;          // �������
		char   Name[64];
		int32  BrandID;       // �����
		int32  BrandOwnerID;  // �������� ������
		int32  MinOrd; // ����������� �����
		int16  QuotCount;
	};
	struct PalmRec900 { // Palm format
		int32  ID;
		char   Code[16];
		int32  GoodsGrpID;    // -> GoodsGroup.ID
		int32  Pack;          // ������� ��������
		int32  Price;         // ������� ����
		int32  Rest;          // �������
		char   Name[64];
		int32  BrandID;       // �����
		int32  BrandOwnerID;  // �������� ������
		int32  MinOrd;        // ����������� �����
		int16  MultMinOrd;    // ������������ ��������� ������������ ������
		int16  QuotCount;
	};
	struct Quot { // Palm format
		int32  QuotKindID;
		int32  Price;
	};
	int    SendQuotKindList();
	int    LoadQuotKindList();
	int    GetQuotList(DbfRecord * pRec, int pFldnList[], TSVector <Quot> & rList);
	SCDBObjGoods::PalmRec * AllocPalmRec(const TSVector <Quot> *, size_t * pBufLen);
	SCDBObjGoods::PalmRec156 * AllocPalmRec156(const TSVector <Quot> *, size_t * pBufLen);
	SCDBObjGoods::PalmRec500 * AllocPalmRec500(const TSVector <Quot> *, size_t * pBufLen);
	SCDBObjGoods::PalmRec800 * AllocPalmRec800(const TSVector <Quot> *, size_t * pBufLen);
	SCDBObjGoods::PalmRec900 * AllocPalmRec900(const TSVector <Quot> *, size_t * pBufLen);

	DbfTable * P_GoodsTbl;
	DbfTable * P_QkTbl;
	TSVector <QuotKind> QkList;
	TSVector <IdxRec> IdxList;
};


class SCDBObjBrand : public SCDBObject {
public:
	struct IdxRec {
		DWORD  ID;
		int32  OwnerID;
	};
	SCDBObjBrand(SpiiExchgContext * pCtx);
	~SCDBObjBrand();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const;
	virtual const char * GetUserMnemonic() const { return "BRANDS"; }
private:
	struct PalmRec { // Palm format
		int32  ID;
		int32  OwnerID;
		char   Name[64];
		char   OwnerName[64];
	};
	SCDBObjBrand::PalmRec * AllocPalmRec(size_t * pBufLen);

	DbfTable * P_Tbl;
	TSVector <IdxRec> IdxList;
};

class SCDBObjLoc : public SCDBObject {
public:
	struct IdxRec {
		DWORD  ID;
	};
	SCDBObjLoc(SpiiExchgContext * pCtx);
	~SCDBObjLoc();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const;
	virtual const char * GetUserMnemonic() const { return "LOCATIONS"; }
private:
	struct PalmRec { // Palm format
		int32  ID;
		char   Name[48];
	};
	SCDBObjLoc::PalmRec * AllocPalmRec(size_t * pBufLen);

	DbfTable * P_Tbl;
	TSVector <IdxRec> IdxList;
};


class SCDBObjOrder : public SCDBObject {
public:
	SCDBObjOrder(SpiiExchgContext * pCtx);
	~SCDBObjOrder();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Import(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "ORDER"; }

	virtual int PutHdrRec(void * pRec);
	virtual int PutLineRec(void * pRec);
private:
	struct OrdHdr {
		int32  ID;
		int16  Date;
		char   Code[10];
		int32  ClientID;   // -> Client.ID
		int32  DlvrAddrID; // -> ClientAddr.ID
		int32  QuotKindID; // -> QuotKind.ID
		int32  Amount;
		int32  PctDis;     // ������ �� ���� �������� � %%
		char   Memo[160];
	};
	struct OrdHdr700 {
		int32  ID;
		int16  Date;
		char   Code[10];
		int32  ClientID;   // -> Client.ID
		int32  DlvrAddrID; // -> ClientAddr.ID
		int32  QuotKindID; // -> QuotKind.ID
		int32  Amount;
		int32  PctDis;     // ������ �� ���� �������� � %%
		char   Memo[160];
		int32  LocID;
	};
	struct OrdLine {
		int32  OrderID;
		int32  GoodsID;
		int32  Price;
		int32  Qtty;
	};
	struct OrdLine158 {
		int32  ID;
		int32  OrderID;
		int32  GoodsID;
		int32  Price;
		int32  Qtty;
	};

	int CreateHdrTbl(const char * pFileName);
	int CreateLineTbl(const char * pFileName);
	int ImportHdr(PROGRESSFN pFn);
	int ImportLine(PROGRESSFN pFn);

	DbfTable * P_HdrTbl;
	DbfTable * P_LineTbl;
	int32 LastID;
	LAssocArray IdAsscList;
};

class SCDBObjCliInv : public SCDBObject {
public:
	SCDBObjCliInv(SpiiExchgContext * pCtx);
	~SCDBObjCliInv();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Import(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "CLIENT INVENTORY"; }

	virtual int PutHdrRec(void * pRec);
	virtual int PutLineRec(void * pRec);
private:
	struct OrdHdr {
		int32  ID;
		int16  Date;
		char   Code[10];
		int32  ClientID;     // -> Client.ID
		int32  DlvrAddrID;   // -> ClientAddr.ID
		char   Memo[160];
	};
	struct OrdLine {
		int32  InvID;
		int32  GoodsID;
		int32  Qtty;
	};

	int CreateHdrTbl(const char * pFileName);
	int CreateLineTbl(const char * pFileName);
	int ImportHdr(PROGRESSFN pFn);
	int ImportLine(PROGRESSFN pFn);

	DbfTable * P_HdrTbl;
	DbfTable * P_LineTbl;
	int32 LastID;
	LAssocArray IdAsscList;
};

class SCDBObjToDo : public SCDBObject {
public:
	SCDBObjToDo(SpiiExchgContext * pCtx);
	~SCDBObjToDo();
	virtual int Init(const char * pExpPath, const char * pImpPath);
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual int Import(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "TODO"; }
private:
	struct PalmRec {
		uint16 DueDate;
		uint8  Priority;
		// ZSTRING Descr
		// ZSTRING Note
	};
	struct HostRec {
		DWORD  RecID;      // RecordID from Palm
		int32  ID;
		LDATE  DueDate;
		int16  Priority;
		int16  Completed;
		char   Descr[256];
		char   Note[256];
	};
	SCDBObjToDo::PalmRec * AllocPalmRec(HostRec * pRec, size_t * pBufLen);
	//
	// ������� ����, � ������� ������������ ������ �� Palm'�
	//
	int CreateImpTbl(const char * pFileName);
	int RecPalmToHost(PalmRec * pPalmRec, HostRec * pHostRec);
	int WriteIdAssoc(PROGRESSFN pFn);
	int ReadIdAssoc(int fromPalmCompressedFile);

	static const char * P_ToDoAsscFileName;   // @global @threadsafe ��� ����� ����������
	static const char * P_ToDoAsscFileName_A; // @global @threadsafe ��� ��������������� ����� ����������
	char * P_ExpPath;
	char * P_ImpPath;
	DbfTable * P_ExpTbl; // ������� ��� �������� ������ � Palm
	DbfTable * P_ImpTbl; // ������� ��� ������� ������ �� Palm'�
	LAssocArray IdAsscList; // ������ ������������ {ID ������ � ����������; RecID ������ �� Palm'�}
};

class SCDBObjProgram : public SCDBObject {
public:
	SCDBObjProgram(SpiiExchgContext * pCtx);
	~SCDBObjProgram();
	virtual int Init();
	virtual int Export(PROGRESSFN pFn, CSyncProperties * pProps);
	virtual const char * GetUserMnemonic() const { return "PROGRAM"; }
	virtual const SCDBTblEntry * GetDefinition(uint * pEntryCount) const;
private:
	int ExportFile(SFile * pFile, PROGRESSFN pFn);
	int GetHostProgramVer(uint32 * pVer);

	SFile ProgramFile;
	SFile DllFile;
};

int SpiiCmd(TcpSocket * pSo, const SpiiCmdBuf * pInBuf, const void * pInParam, SpiiCmdBuf * pOutBuf, void * pOutParam);

class PalmTcpExchange {
public:
	PalmTcpExchange(TcpSocket * pSo, const char * pSpiiPath, const char * pLogPath, const char * pTcpDevListPath, const char * pInstallPath);
	~PalmTcpExchange();

	int TblOpen(const char * pTblName, ulong mode, SpiiDbHandler pH);
	int TblFind(const char * pTblName, SpiiDbHandler pH);
	int TblClose(SpiiDbHandler);
	int GetTblStat(SpiiDbHandler, SpiiTblStatParams *);
	int TblGetPos(SpiiDbHandler, ulong * pPos);
	int TblSetPos(SpiiDbHandler, ulong pos);
	int TblAddRec(SpiiDbHandler, SpiiTblRecParams *, const void *, uint32 bufSize);
	int TblUpdRec(SpiiDbHandler, SpiiTblRecParams *, const void *, uint32 bufSize);
	int TblDelRec(SpiiDbHandler, SpiiTblRecParams *);
	int TblGetRec(SpiiDbHandler, SpiiTblRecParams *, void *, uint32 * pBufSize);
	int TblGetTbl(SpiiDbHandler, void *, uint32 * pBufSize);
	int TblSetTbl(SpiiDbHandler, const void *, uint32 bufSize);
	int TblDelete(const char * pTblName);
	int TblPurgeAllRecs(SpiiDbHandler h);
	int GetDeviceInfo(SpiiDeviceInfoParams *);
	int GetProgramVer(uint32 * pVersion);
	int QuitSess();
	int GetSpiiPath(SString & aPath);
	int GetLogPath(SString & aPath);
	int GetInstallPath(SString & rPath);
	int LogTrafficInfo(const char * pDevice);
	static int LogTrafficInfo(const char * pPath, const char * pLogPath, const char * pDevice, int isReadBytes);
	SString & GetTcpDevListPath(SString & rPath);
	TcpSocket * GetSock() {return P_So;}
private:
	int SpiiCmd(TcpSocket * pSo, const SpiiCmdBuf * pInBuf, const void * pInParam, SpiiCmdBuf * pOutBuf, void * pOutParam, const char * pCallingFuncName);

	TcpSocket * P_So;
	SString SpiiPath;
	SString LogPath;
	SString TcpDevListPath;
	SString InstallPath;
};

int SpiiExchange(PalmTcpExchange *, PROGRESSFN pFn, CSyncProperties * pProps);

#endif
