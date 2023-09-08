// TEST-PROCESS.CPP
// Copyright (c) A.Sobolev 2023
//
#include <pp.h>
#pragma hdrstop

SLTEST_R(Evnt)
{
	class EvntThread : public SlThread {
	public:
		struct InitBlock {
			uint   Id;
			const  char * P_EvntNameStart;
			const  char * P_EvntNameFinish;
			volatile int * P_SignalVar;
			volatile int * P_Result;
		};
		EvntThread(InitBlock * pBlk) : SlThread(pBlk)
		{
			Id = pBlk->Id;
			EvntNameStart  = pBlk->P_EvntNameStart;
			EvntNameFinish = pBlk->P_EvntNameFinish;
			P_SignalVar = pBlk->P_SignalVar;
			P_Result = pBlk->P_Result;
		}
		virtual void Run()
		{
			assert(SLS.GetConstTLA().Id == GetThreadID());
			*P_Result = 1;
			Evnt evnt_start(EvntNameStart, Evnt::modeOpen);
			Evnt evnt_finish(EvntNameFinish, Evnt::modeCreate);
#ifndef NDEBUG
			assert((HANDLE)evnt_start);
			assert((HANDLE)evnt_finish);
			assert(evnt_start.Wait(-1) > 0);
#else
			if(!(HANDLE)evnt_start)
				*P_Result = 0;
			if(!(HANDLE)evnt_finish)
				*P_Result = 0;
			if(evnt_start.Wait(-1) < 0)
				*P_Result = 0;
#endif
			*P_SignalVar = Id;
			SDelay(500);
			*P_SignalVar = -(int)Id; // @v9.6.3
			evnt_finish.Signal();
			// @v9.6.3 *P_SignalVar = -(int)Id;
		}
	private:
		uint   Id;
		volatile int * P_SignalVar;
		volatile int * P_Result;
		SString EvntNameStart;
		SString EvntNameFinish;
	};

	int    ok = 1;
	static volatile int signal_var = 0;
	static volatile int result = 0;
	SString evnt_name_start("TestEvnt_Start");
	SString evnt_name_finish("TestEvnt_Finish");
	Evnt ev_start(evnt_name_start, Evnt::modeCreate);
	SLCHECK_NZ((HANDLE)ev_start);
	{
		int    ok = 1;
		for(uint i = 1; i <= 10; i++) {
			const  uint tid = i;
			EvntThread::InitBlock blk;
			blk.Id = tid;
			blk.P_EvntNameStart  = evnt_name_start;
			blk.P_EvntNameFinish = evnt_name_finish;
			blk.P_SignalVar = &signal_var;
			blk.P_Result = &result;
			EvntThread * p_thread = new EvntThread(&blk);
			p_thread->Start();
			SDelay(500);
			{
				Evnt ev_finish(evnt_name_finish, Evnt::modeOpen);
				SLCHECK_NZ((HANDLE)ev_finish);
#ifndef NDEBUG
				assert(signal_var == 0);
#else
				SLCHECK_NZ(signal_var == 0);
#endif
				ev_start.Signal();
				SDelay(10);
#ifndef NDEBUG
				assert(signal_var == tid);
				assert(ev_finish.Wait(-1) > 0);
				assert(signal_var == -(int)tid);
#else
				SLCHECK_NZ(signal_var == tid);
				SLCHECK_NZ(ev_finish.Wait(-1) > 0);
				SLCHECK_NZ(signal_var == -(int)tid);
#endif
			}
			WaitForSingleObject((HANDLE)*p_thread, INFINITE);
			signal_var = 0;
			ev_start.Reset();
		}
	}
	return CurrentStatus;
}

SLTEST_R(ReadWriteLock)
{
	//
	// ���� ReadWrite-���������� (ReadWriteLock)
	// ���� ����� � ���, ����� ��������� ����� ����� ������� (128)
	// ��������� ������ ���������� ������ RwlCommonDataList � ����� ����������
	// � ���� ������������� ��������, ��������� �� ������ ������ (RwlThread::Id)
	// � ����������� ��������� ��� �������� � ������������� ��������� ������������.
	//
	// ������������ ����������/��������� �������� �������������� ��������� �������:
	// -- �������������� ����������� ����� RwlDataPatternList ���������� ��� �������
	//   ������ �������� (�� RwlPattersPerThread �� ������ �����).
	// -- ����� �� ���� �������� ���������� � RwlCommonDataList �����-�� ���� �� ���� ����������
	//   �������� ������ � ����������� ���������������.
	// -- �����, �������� RwlCommonDataList ��������� ���� ������ �� ������� ����,
	//   ����� ������ ������� (RwlEntry) ������� �������� ���� {0; 0} (��� �� ����������
	//   ������), ���� {Id; ���� �� ���������� ��� ������ Id �������� }.
	//
	// ��� ��� ���������� ����� � RwlCommonDataList ���� �� ��������� � �����������
	// �������, ��� (������), ������ ����������� ������������, ������ ��������� ������
	// ����� ������ � ��������� ������������� ����������� �������� � ������������� ���������� (assert).
	// ��������� ����������� ����� ����������� ����������� ������� WriteLock()/Unlock() ���
	// ReadLock()/Unlock().
	//
	// ������ ����� ��� ��������� �������� ����� ���� ���� "���������" ���� "���������" �
	// ����������� �� �������� ���������� �����, ����������� �� ���� ��������. ����� ����,
	// ����� ������ �������� �������������� ��������� ��������� �������� SDelay(rn%7)
	//
	struct RwlEntry {
		uint   Id;
		int64  Val;
	};

	static const uint RwlThreadCount = 128;
	static const uint RwlPattersPerThread = 16;
	static RwlEntry RwlCommonDataList[37];
	static int64 RwlDataPatternList[RwlThreadCount*RwlPattersPerThread];
	static const char * P_StartEvntName = "RwlTest_Start";
	static volatile int ResultList[RwlThreadCount];

	class RwlThread : public SlThread {
	public:
		struct InitBlock {
			uint   Id;
			ReadWriteLock * P_Lock;
			volatile int  * P_Result;
		};
		RwlThread(InitBlock * pBlk) : SlThread(pBlk), Id(pBlk->Id), P_Lock(pBlk->P_Lock), P_Result(pBlk->P_Result)
		{
		}
		virtual void Run()
		{
			assert(SLS.GetConstTLA().Id == GetThreadID());
			*P_Result = 1;
			//int    lck = 0;
			assert(Id > 0 && Id <= RwlThreadCount);
			Evnt start_evnt(P_StartEvntName, Evnt::modeOpen);
			start_evnt.Wait(-1);
			for(uint i = 0; i < 10000; i++) {
				const  uint rn = SLS.GetTLA().Rg.GetUniformInt(200000+Id*7+i);
				const  int  writer = BIN((rn % 2) == 0);
				if(writer) {
					uint j = (rn % RwlPattersPerThread);
					int64 pattern = RwlDataPatternList[((Id-1) * RwlPattersPerThread)+j];
					{
						//P_Lock->WriteLock(); 
						SRWLOCKER(*P_Lock, SReadWriteLocker::Write);
						//lck = 1;
						const uint pos = rn%SIZEOFARRAY(RwlCommonDataList);
						RwlCommonDataList[pos].Id = Id;
						RwlCommonDataList[pos].Val = pattern;
						//P_Lock->Unlock(); 
						//lck = 0;
					}
				}
				else {
					//P_Lock->ReadLock(); 
					SRWLOCKER(*P_Lock, SReadWriteLocker::Read);
					//lck = 1;
					for(uint j = 0; j < SIZEOFARRAY(RwlCommonDataList); j++) {
						const RwlEntry & r_entry = RwlCommonDataList[j];
#ifndef NDEBUG
						assert(r_entry.Id != 0 || r_entry.Val == 0);
						assert(r_entry.Id >= 0 && r_entry.Id <= RwlThreadCount);
#else
						THROW(r_entry.Id != 0 || r_entry.Val == 0);
						THROW(r_entry.Id >= 0 && r_entry.Id <= RwlThreadCount);
#endif
						if(r_entry.Id) {
							int    found = 0;
							uint   k;
							for(k = 0; !found && k < RwlPattersPerThread; k++) {
								int64 pattern = RwlDataPatternList[((r_entry.Id-1) * RwlPattersPerThread)+k];
								if(r_entry.Val == pattern)
									found = 1;
							}
							uint   real_id = 0;
							for(k = 0; !real_id && k < SIZEOFARRAY(RwlDataPatternList); k++) {
								if(RwlDataPatternList[k] == r_entry.Val) {
									real_id = (k % RwlPattersPerThread) + 1;
								}
							}
#ifndef NDEBUG
							assert(found);
#else
							THROW(found);
#endif
						}
					}
					//P_Lock->Unlock(); 
					//lck = 0;
				}
				SDelay(rn%7);
			}
			CATCH
				//if(lck)
					//P_Lock->Unlock();
				*P_Result = 0;
			ENDCATCH
		}
		uint   Id;
		ReadWriteLock * P_Lock;
		volatile int * P_Result;
	};
	int    ok = 1;
	{
		int    r1 = 0;
		int    r2 = 0;
		int    r3 = 0;
		int    ur = 0;
		ReadWriteLock lck;
		r1 = lck.WriteLockT_(1000);
		assert(r1 > 0);
		r2 = lck.ReadLockT_(1000);
		assert(r2 < 0);
		ur = lck.Unlock_();
		assert(ur);
		r2 = lck.ReadLockT_(1000);
		assert(r2 > 0);
		r3 = lck.ReadLockT_(1000);
		assert(r3 > 0);
		r1 = lck.WriteLockT_(1000);
		assert(r1 < 0);
		ur = lck.Unlock_();
		assert(ur);
		ur = lck.Unlock_();
		assert(ur);
		r1 = lck.WriteLockT_(1000);
		assert(r1 > 0);
		ur = lck.Unlock_();
		assert(ur);
		r2 = lck.ReadLockT_(1000);
		assert(r2 > 0);
		ur = lck.Unlock_();
		assert(ur);
	}
	{
		static ReadWriteLock _RwLck;
		class AbstractWriter {
			void * P;
		public:
			AbstractWriter(long timeout) : P(0)
			{
				P = (_RwLck.WriteLockT_(timeout) > 0) ? reinterpret_cast<void *>(1) : 0;
			}
			~AbstractWriter()
			{
				if(P) {
					_RwLck.Unlock_();
					P = 0;
				}
				assert(P == 0);
			}
			bool    IsValid() const { return (P != 0); }
		};
		class AbstractReader {
			void * P;
		public:
			AbstractReader(long timeout) : P(0)
			{
				P = (_RwLck.ReadLockT_(timeout) > 0) ? reinterpret_cast<void *>(1) : 0;
			}
			~AbstractReader()
			{
				if(P) {
					_RwLck.Unlock_();
					P = 0;
				}
				assert(P == 0);
			}
			bool   IsValid() const { return (P != 0); }
		};
		class Thread_W : public SlThread_WithStartupSignal {
			long    Timeout;
		public:
			Thread_W(long timeout) : SlThread_WithStartupSignal(0), Timeout(timeout)
			{
				assert(Timeout > 0);
			}
			void Run()
			{
				AbstractWriter o(Timeout*3);
				assert(o.IsValid());
				SDelay(Timeout / 2);
			}
		};
		class Thread_R : public SlThread_WithStartupSignal {
			long    Timeout;
		public:
			Thread_R(long timeout) : SlThread_WithStartupSignal(0), Timeout(timeout)
			{
				assert(Timeout > 0);
			}
			void Run()
			{
				AbstractReader o(Timeout*3);
				assert(o.IsValid());
				SDelay(Timeout / 2);
			}
		};
		//
		int    r1 = 0;
		int    r2 = 0;
		uint   tc = 0;
		HANDLE tl[128];
		MEMSZERO(tl);
		{
			Thread_W * p_w = new Thread_W(8000);
			p_w->Start(1);
			tl[tc++] = *p_w;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_W * p_w = new Thread_W(8000);
			p_w->Start(1);
			tl[tc++] = *p_w;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_W * p_w = new Thread_W(8000);
			p_w->Start(1);
			tl[tc++] = *p_w;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_W * p_w = new Thread_W(8000);
			p_w->Start(1);
			tl[tc++] = *p_w;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_R * p_r = new Thread_R(8000);
			p_r->Start(1);
			tl[tc++] = *p_r;
		}
		{
			Thread_W * p_w = new Thread_W(8000);
			p_w->Start(1);
			tl[tc++] = *p_w;
		}
		//
		::WaitForMultipleObjects(tc, tl, TRUE, INFINITE);
		r1 = _RwLck.WriteLock_();
		assert(r1 > 0);
		r1 = _RwLck.Unlock_();
		assert(r1 > 0);
		r2 = _RwLck.ReadLock_();
		assert(r2 > 0);
		r2 = _RwLck.Unlock_();
		assert(r2 > 0);
	}
	uint   i;
	ReadWriteLock * p_lock = new ReadWriteLock();
	memzero(RwlCommonDataList, sizeof(RwlCommonDataList));
	for(i = 0; i < RwlThreadCount; i++) {
		for(uint j = 0; j < RwlPattersPerThread; j++) {
			int64 pattern = (int64)SLS.GetTLA().Rg.GetUniformInt(2000000000L);
			RwlDataPatternList[(i * RwlPattersPerThread)+j] = pattern;
		}
	}
	{
		size_t h_chunk_count = 0;
		size_t h_count[6];
		HANDLE h_list[6][MAXIMUM_WAIT_OBJECTS];
		memzero(h_count, sizeof(h_count));
		memzero(h_list, sizeof(h_list));
		Evnt start_evnt(P_StartEvntName, Evnt::modeCreate);
		for(i = 0; i < RwlThreadCount; i++) {
			RwlThread::InitBlock blk;
			blk.Id = i+1;
			blk.P_Lock = p_lock;
			blk.P_Result = ResultList+i;
			RwlThread * p_thread = new RwlThread(&blk);
			p_thread->Start(0);
			//
			// WaitForMultipleObjects �� ����� ���������� �����
			// MAXIMUM_WAIT_OBJECTS (64) �������� �� ���� �����
			//
			if(h_count[h_chunk_count] == MAXIMUM_WAIT_OBJECTS) {
				h_chunk_count++;
				assert(h_chunk_count < SIZEOFARRAY(h_count));
			}
			h_list[h_chunk_count][h_count[h_chunk_count]++] = (HANDLE)*p_thread;
		}
		start_evnt.Signal();
		int    r;
		for(i = 0; i <= h_chunk_count; i++) {
			r = WaitForMultipleObjects(h_count[i], h_list[i], 1, INFINITE);
		}
		for(i = 0; ok && i < RwlThreadCount; i++) {
			if(ResultList[i] == 0)
				ok = 0;
		}
	}
	return ok;
}
