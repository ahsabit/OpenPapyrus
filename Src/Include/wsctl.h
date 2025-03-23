// WSCTL.H
// Copyright (c) A.Sobolev 2023, 2024, 2025
//
//
// Descr: ���� ����-�������������. 
//
class WsCtl_SelfIdentityBlock {
public:
	WsCtl_SelfIdentityBlock();
	int    GetOwnIdentifiers();

	S_GUID Uuid;
	MACAddrArray MacAdrList; // @v12.0.1
	S_IPAddr IpAdr; // @v12.0.1
	PPID   PrcID; // ������������� ���������� �� �������. ������������ ������� �� �������.
	PPID   ComputerID; // @v12.0.3 ������������� ���������� �� �������.
	PPID   CompCatID; // @v12.0.3 ������������� ��������� ����������.
	SString PrcName; // ������������ ���������� �� �������. ������������ ������� �� �������.
	SString CompCatName; // @v12.0.3 ������������ ��������� ����������
};
//
// 
// 
class WsCtl_Config {
public:
	WsCtl_Config();
	WsCtl_Config & Z();
	bool   IsEq(const WsCtl_Config & rS) const
	{
		return (Server == rS.Server && Port == rS.Port && Timeout == rS.Timeout && 
			DbSymb == rS.DbSymb && User == rS.User && Password == rS.Password);
	}
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

class WsCtl_LoginBlock {
public:
	WsCtl_LoginBlock();
	~WsCtl_LoginBlock();
	WsCtl_LoginBlock & Z();

	char   LoginText[256];
	char   PwText[128];	
};

class WsCtl_RegistrationBlock {
public:
	WsCtl_RegistrationBlock();
	~WsCtl_RegistrationBlock();
	WsCtl_RegistrationBlock & Z();

	char   Name[256];
	char   Phone[32];
	char   PwText[128];
	char   PwRepeatText[128];
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
	//
	// Descr: ��������� ������ ���� � �������� �������� ���������� ������ ��� ����������� � ��������������.
	//
	SString & MakeBaseSystemImagePath(SString & rBuf) const;
	//
	// Descr: ������� �������� ���������� �� ��������� �����.
	//
	bool   IsThereSystemImage() const;
	int    CreateSystemImage();
	int    RestoreSystemImage();

	SString SysUser;
	SString SysPassword;
	StringSet SsAppEnabled;
	StringSet SsAppDisabled;
	StringSet SsAppPaths; // @v11.8.1

	struct AllowedPath {
		AllowedPath() : Flags(0)
		{
		}
		bool   FASTCALL operator == (const AllowedPath & rS) const { return (Flags == rS.Flags && Path == rS.Path); }
		uint   Flags;   // SFile::accsfXXX
		SString Path;
		SString ResolvedPath; // ��� ��� path ����� ���� ����� � ����������������� ����, ����� ������������ �������������� �������� //
			// �� ���������� ��������. ������������� ��������� ��������� � ResolvedPath.
	};
	struct AllowedRegistryEntry {
		AllowedRegistryEntry() : RegKeyType(0), Flags(0)
		{
		}
		bool   FASTCALL operator == (const AllowedRegistryEntry & rS) const { return (RegKeyType == rS.RegKeyType && Flags == rS.Flags && Branch == rS.Branch); }
		int    RegKeyType; // WinRegKey::regkeytypXXX general || wow64_32 || wow64_64
		uint   Flags;       
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

	enum {
		fResolving_FileNFound = 0x0001, // ����������� ���� ��������� �� ������
		fResolving_ByCache    = 0x0002  // ������ ���� � ��������� �������� ����������� ���� 
	};

	PPID   ID;                 // @v12.0.6 ������������� ������� � ���� ������ �������
	PPID   CategoryID;         // @v12.0.6 ������������� ��������� � ���� ������ �������
	SString Category;          // utf8 ��������� ���������
	SString Title;             // utf8 ������������ �� ������ ��������� ���������
	SString ExeFileName;       // utf8 ��� ������������ ����� (� �����������) 
	SString FullResolvedPath;  // @transient utf8 ������ ���� � ������������ �����.
	SString PicSymb;           // utf8 ������ ����������� ������ //
	int   PicHashAlg;          // @v12.0.6 �������� ���� ����������� ����������� // 
	SBinaryChunk PicHash;      // @v12.0.6 ��� ����������� ����������� //
	uint64 UedTime_Resolution; // @v12.1.9 (cache) ����� ���������� ������� ���� ���������
	uint  Flags;               // @v12.1.9 @flags (� ��� ����� ����� ����, ������������� � ���������� ���������� ������� ����) 
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
	WsCtl_ProgramEntry * SearchByID(PPID id) const;

	enum {
		catsurrogateidAll = 10000
	};
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
	static int Test();

	struct RegistrationBlock { // @v11.9.10
		RegistrationBlock();
		RegistrationBlock & Z();
		bool   IsValid() const;
		bool   FromJsonObj(const SJson * pJs);

		S_GUID  WsCtlUuid;
		SString Name;
		SString Phone;
		SString PwText;
		// Results:
		int    Status;  // 0 - error, 1 - success
		PPID   SCardID;
		PPID   PsnID;
	};
	struct ComputerRegistrationBlock { // @v12.0.1
		ComputerRegistrationBlock();
		ComputerRegistrationBlock & Z();
		SJson * ToJsonObj(bool asServerReply) const;
		bool   FromJsonObj(const SJson * pJs);

		S_GUID WsCtlUuid; // IN/OUT
		MACAddrArray MacAdrList;
		SString Name;
		SString CompCatName; // @v12.0.4 ������������ ��������� ����������
		// Results:
		int    Status;  // 0 - error, 1 - success
		PPID   PrcID;
		PPID   ComputerID;
		PPID   CompCatID; // @v12.0.4 ������������� ��������� ����������
	};
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
		// Results:
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
	int    SearchPrcByWsCtlUuid(const S_GUID & rWsCtlUuid, PPID * pPrcID, SString * pPrcNameUtf8, ProcessorTbl::Rec * pRec); // @v12.2.4 (ProcessorTbl::Rec * pRec)
	int    Init(const S_GUID & rUuid);
	//
	// Descr: ���������� ������������� ������ ����� ���������, ������� ����� ���� ��������� ��� ��������� ��� ��� ��������������� ������.
	//
	int    GetRawQuotKindList(PPIDArray & rList);
	int    GetQuotList(PPID goodsID, PPID locID, const PPIDArray & rRawQkList, PPQuotArray & rQList);
	int    StartSess(StartSessBlock & rBlk);
	int    Registration(RegistrationBlock & rBlk);
	int    RegisterComputer(ComputerRegistrationBlock & rBlk);
	int    Auth(AuthBlock & rBlk);
	int    SendClientPolicy(const S_GUID & rComputerUuid, SString & rResult);
	//
	// Descr: ���������� ������� ������ ������� ��� ������� � json-�������
	// ARG(mock IN): ���� true, �� ������������ �������� "��������" workspace/wsctl/wsctl-program.json. � ��������� ������ ������ 
	//   ������� ����������� �� ������� ������ PPObjSwProgram
	// ARG(rResult OUT): ����� ��� ����������
	//
	int    SendProgramList(bool mock, const S_GUID & rComputerUuid, SString & rResult);

	PPObjTSession TSesObj;
	PPObjQuotKind QkObj;
	PPObjGoods GObj;
	PPObjPerson PsnObj;
	PPObjSCard ScObj;
	PPObjArticle ArObj;
	PPObjComputer CompObj;
	S_GUID WsUUID; // UUID ����������� ������� �������
	PPID   PrcID;  // �� ����������, ���������������� ������� �������
	PPID   ComputerID;  // @v12.2.4 ������������� ���������� (� �����, ������ ����������� �������: 
		// ProcessorRec(PrcID).LinkObjType == PPOBJ_COMPUTER && ProcessorRec(PrcID).LinkObjID == ComputerID)
	PPID   CompCatID;   // @v12.2.4 ������������� ��������� ����������
	SString PrcNameUtf8;
private:
	PPSCardConfig ScCfg;
	PPID   ScSerID; // ����� ���� (�� ������������)
	PPID   PsnKindID; // ��� ����������, ��������� � ������ ���� ScSerID
};
//
// Descr: ������������ ����� ����������, ���� � �������� ������ ��������������� ������� � ������
//
class WsCtlApp {
public:
	WsCtlApp();
	static bool GetLocalCachePath(SString & rPath);
	static int  GetProgramListFromCache(WsCtl_ProgramCollection * pPgmL, bool resolvedPrmListCache, WsCtl_ClientPolicy * pPolicyL);
	static int  StoreProgramListResolvedCache(const WsCtl_ProgramCollection & rPgmL);
private:
};