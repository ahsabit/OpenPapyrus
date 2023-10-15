// WSCTL.H
// Copyright (c) A.Sobolev 2023
//
//
// Descr: ���� ����-�������������. 
//
class WsCtl_SelfIdentityBlock {
public:
	WsCtl_SelfIdentityBlock();
	int    GetOwnUuid();

	S_GUID Uuid;
	PPID   PrcID; // ������������� ���������� �� �������. ������������ ������� �� �������.
	SString PrcName; // ������������ ���������� �� �������. ������������ ������� �� �������.
};
//
// 
// 
class WsCtl_Config {
public:
	WsCtl_Config();
	WsCtl_Config & Z();
	//
	// Descr: ��������� ������������ �� win-�������
	//
	int    Read();
	//
	// Descr: ���������� ������������ � win-������
	//
	int    Write();
	SJson * ToJsonObj() const;
	int    FromJsonObj(const SJson * pJsObj);

	SString Server;
	int    Port;
	int    Timeout;
	//
	SString DbSymb;
	SString User;
	SString Password;
};
/*
{
	"account": {
		"login": "abc",
		"password": "",
	},
	"app": {
		"enable": [
			"abc",
			"123"
		],
		"disable" : [
			"def",
			"456"
		]
	}
}
*/
class WsCtl_ClientPolicy {
public:
	WsCtl_ClientPolicy();
	WsCtl_ClientPolicy(const WsCtl_ClientPolicy & rS);
	WsCtl_ClientPolicy & FASTCALL Copy(const WsCtl_ClientPolicy & rS);
	WsCtl_ClientPolicy & FASTCALL operator = (const WsCtl_ClientPolicy & rS) { return Copy(rS); }
	bool FASTCALL operator == (const WsCtl_ClientPolicy & rS) { return IsEq(rS); }
	bool FASTCALL operator != (const WsCtl_ClientPolicy & rS) { return !IsEq(rS); }
	WsCtl_ClientPolicy & Z();
	bool   FASTCALL IsEq(const WsCtl_ClientPolicy & rS) const;
	SJson * ToJsonObj() const;
	int    FromJsonObj(const SJson * pJsObj);
	int    Resolve();
	int    Apply();

	SString SysUser;
	SString SysPassword;
	StringSet SsAppEnabled;
	StringSet SsAppDisabled;
	StringSet SsAppPaths; // @v11.8.1

	struct AllowedPath {
		AllowedPath() : Flags(0)
		{
		}
		bool  FASTCALL operator == (const AllowedPath & rS) const
		{
			return (Flags == rS.Flags && Path == rS.Path);
		}
		uint  Flags;   // SFile::accsfXXX
		SString Path;
		SString ResolvedPath; // ��� ��� path ����� ���� ����� � ����������������� ����, ����� ������������ �������������� �������� //
			// �� ���������� ��������. ������������� ��������� ��������� � ResolvedPath.
	};
	struct AllowedRegistryEntry {
		AllowedRegistryEntry() : RegKeyType(0), Flags(0)
		{
		}
		bool   FASTCALL operator == (const AllowedRegistryEntry & rS) const
		{
			return (RegKeyType == rS.RegKeyType && Flags == rS.Flags && Branch == rS.Branch);
		}
		int   RegKeyType; // WinRegKey::regkeytypXXX general || wow64_32 || wow64_64
		uint  Flags;       
		SString Branch;
	};
	TSCollection <AllowedPath> AllowedPathList;
	TSCollection <AllowedRegistryEntry> AllowedRegList;
};
//
// Descr: ���������� ����������� ���������, ������������ � ���������� ����.
//
class WsCtl_ProgramEntry {
public:
	WsCtl_ProgramEntry();
	WsCtl_ProgramEntry & Z();
	//
	// Descr: see descr of WsCtl_ProgramEntry::IsEq
	//
	bool   FASTCALL operator == (const WsCtl_ProgramEntry & rS) const { return IsEq(rS); }
	//
	// Descr: see descr of WsCtl_ProgramEntry::IsEq
	//
	bool   FASTCALL operator != (const WsCtl_ProgramEntry & rS) const { return !IsEq(rS); }
	//
	// Descr: �������� ��������������� ���������� this � ����������� rS.
	//   ��������������� ������������ ��� ����� ����� FullResolvedPath ���������
	//   �� ����������� �������� �� ��������� ����������� ����������.
	// Returns:
	//   true - *this � rS ������������.
	//   false - *this � rS �����������.
	//
	bool   FASTCALL IsEq(const WsCtl_ProgramEntry & rS) const;
	SJson * ToJsonObj(bool withResolvance) const;
	int    FromJsonObj(const SJson * pJsObj);
	SString Category;         // utf8 ��������� ���������
	SString Title;            // utf8 ������������ �� ������ ��������� ���������
	SString ExeFileName;      // utf8 ��� ������������ ����� (� �����������) 
	SString FullResolvedPath; // @transient utf8 ������ ���� � ������������ �����.
	SString PicSymb;          // utf8 ������ ����������� ������ //
};

class WsCtl_ProgramCollection : public TSCollection <WsCtl_ProgramEntry> {
public:
	WsCtl_ProgramCollection();
	WsCtl_ProgramCollection(const WsCtl_ProgramCollection & rS);
	WsCtl_ProgramCollection & FASTCALL Copy(const WsCtl_ProgramCollection & rS);
	WsCtl_ProgramCollection & FASTCALL operator = (const WsCtl_ProgramCollection & rS) { return Copy(rS); }
	bool   FASTCALL operator == (const WsCtl_ProgramCollection & rS) const { return IsEq(rS); }
	bool   FASTCALL operator != (const WsCtl_ProgramCollection & rS) const { return !IsEq(rS); }
	//
	// Descr: ������� ���������� ��������������� ���������� this � ����������� rS.
	//   ��������������� ������������ ��� ����� ������ CatList � SelectedCatSurrogateId.
	//   ����� ����, see desr of WsCtl_ProgramEntry::IsEq.
	// Returns:
	//   true - *this � rS ������������.
	//   false - *this � rS �����������.// 
	//
	bool   FASTCALL IsEq(const WsCtl_ProgramCollection & rS) const;
	bool   IsResolved() const { return Resolved; }
	long   GetSelectedCatSurrogateId() const { return SelectedCatSurrogateId; }
	void   SetSelectedCatSurrogateId(long id) { SelectedCatSurrogateId = id; }
	const StrAssocArray & GetCatList() const { return CatList; }
	SJson * ToJsonObj(bool withResolvance) const;
	int    FromJsonObj(const SJson * pJsObj);
	int    MakeCatList();
	int    Resolve(const WsCtl_ClientPolicy & rPolicy);

	mutable SMtLock Lck; // ����������, ������������ ��� ������������ ������� ��������� ���������� ����� � ����������.
private:
	StrAssocArray CatList; // @transient
	long   SelectedCatSurrogateId; // @transient
	bool   Resolved; // @transient
};
//
// Descr: �����, ���������� �� ��������� ������������� ������:
//   -- ����������� � ������������ �������
//   -- ������ � ���������� ���������
//   -- ���������� �������������
//   -- ������������ ��������� � �������
//
class WsCtl_SessionFrame {
public:
	WsCtl_SessionFrame() : State(0)
	{
	}
	~WsCtl_SessionFrame()
	{
	}
	void   SetClientPolicy(const WsCtl_ClientPolicy & rP)
	{
		Policy = rP;
	}
	int    Start();
	int    Finish();
	int    LaunchProcess(const WsCtl_ProgramEntry * pPgmEntry);
private:
	struct Process : public SlProcess::Result {
		SString Name;
		SString Path;
	};
	enum {
		stRunning = 0x0001
	};
	WsCtl_ClientPolicy Policy;
	TSCollection <Process> RunningProcessList; // ������ ��������� ���������� ���������
	SPtrHandle H_UserToken;
	uint   State;
};
//
// Descr: ����, ���������� �� �������������� ��������� ������ � ������� WsCtl
//
class WsCtlSrvBlock {
public:
	struct AuthBlock {
		AuthBlock();
		AuthBlock & Z();
		//
		// Descr: ���������� true ���� �������� ��������� ���������������� � ����������� ������� ��� ������� ���������� �������
		//
		bool   IsEmpty() const;
		bool   FromJsonObj(const SJson * pJs);

		S_GUID  WsCtlUuid;
		SString LoginText;
		SString Pw;
		//
		// Results:
		//
		PPID   SCardID;
		PPID   PsnID;
	};
	struct StartSessBlock {
		StartSessBlock();
		//
		// Descr: ���������� true ���� �������� ��������� ���������������� � ����������� ������� ��� ������� ���������� �������
		//
		bool   IsEmpty() const { return (!SCardID || !GoodsID); }
		StartSessBlock & Z();
		bool   FromJsonObj(const SJson * pJs);

		S_GUID WsCtlUuid;
		PPID   SCardID;
		PPID   GoodsID;
		PPID   TechID;
		double Amount;
		//
		// Results:
		//
		PPID   TSessID;
		PPID   RetTechID;
		PPID   RetSCardID;
		PPID   RetGoodsID;
		LDATETIME ScOpDtm; // ����� �������� �������� ����� //
		double WrOffAmount; // ��������� ����� //
		STimeChunk SessTimeRange;
	};

	WsCtlSrvBlock();
	int    SearchPrcByWsCtlUuid(const S_GUID & rWsCtlUuid, PPID * pPrcID, SString * pPrcNameUtf8);
	int    Init(const S_GUID & rUuid);
	//
	// Descr: ���������� ������������� ������ ����� ���������, ������� ����� ���� ��������� ��� ��������� ��� ��� ��������������� ������.
	//
	int    GetRawQuotKindList(PPIDArray & rList);
	int    GetQuotList(PPID goodsID, PPID locID, const PPIDArray & rRawQkList, PPQuotArray & rQList);
	int    StartSess(StartSessBlock & rBlk);
	int    Auth(AuthBlock & rBlk);
	int    SendClientPolicy(SString & rResult);
	int    SendProgramList(SString & rResult);

	PPObjTSession TSesObj;
	PPObjQuotKind QkObj;
	PPObjGoods GObj;
	PPObjPerson PsnObj;
	PPObjSCard ScObj;
	PPObjArticle ArObj;
	S_GUID WsUUID; // UUID ����������� ������� �������
	PPID   PrcID;  // �� ����������, ���������������� ������� �������
	SString PrcNameUtf8;
private:
	PPSCardConfig ScCfg;
	PPID   ScSerID; // ����� ���� (�� ������������)
	PPID   PsnKindID; // ��� ����������, ��������� � ������ ���� ScSerID
};
