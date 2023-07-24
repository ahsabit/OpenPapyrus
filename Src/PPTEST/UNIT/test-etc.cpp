// TEST-ETC.CPP
// Copyright (c) A.Sobolev 2023
// @codepage UTF-8
// ������ ������������ ������ �������. � �������� � �������� ����������.
//
#include <pp.h>
#pragma hdrstop
//
//
//
SLTEST_R(ImportPo)
{
	// C:\Papyrus\Src\Rsrc\Data\iso-codes
	// C:\Papyrus\Src\Rsrc\Data\iso-codes\iso_15924
	SString temp_buf;
	SString path_buf;
	PoBlock blk(0);
	SDirEntry de;
	SString base_dir("/Papyrus/Src/Rsrc/Data/iso-codes/iso_15924/");
	(temp_buf = base_dir).Cat("*.po");
	for(SDirec sd(temp_buf); sd.Next(&de) > 0;) {
		if(!de.IsSelf() && !de.IsUpFolder() && (de.IsFolder() || de.IsFile())) {
			de.GetNameA(base_dir, temp_buf);
			SPathStruc::NormalizePath(temp_buf, SPathStruc::npfCompensateDotDot, path_buf);
			blk.Import(path_buf, 0, 0);
		}
	}
	blk.Finish();
	blk.Sort();
	{
		SJson * p_js = blk.ExportToJson();
		if(p_js) {
			p_js->ToStr(temp_buf);
			(path_buf = base_dir).SetLastDSlash().Cat("importpo-out-file.json");
			SFile f_out(path_buf, SFile::mWrite);
			f_out.Write(temp_buf.cptr(), temp_buf.Len());
		}
	}
	return CurrentStatus;
}

SLTEST_R(LayoutFlex)
{
	SString root_path;
	SString temp_buf;
	SLS.QueryPath("testroot", root_path);
	{
		enum {
			loidMain = 1,
			loidUpr,
			loidLwr,
			loidF,
			loidB
		};
		const float f_width = 64.0f;
		SUiLayout lo_main;
		lo_main.SetID(loidMain);
		{
			SUiLayoutParam alb(DIREC_VERT, 0, SUiLayoutParam::alignStretch);
			alb.Flags &= ~SUiLayoutParam::fContainerWrap;
			lo_main.SetLayoutBlock(alb);
		}
		SUiLayout * p_lo_upr = lo_main.InsertItem();
		p_lo_upr->SetID(loidUpr);
		{
			SUiLayoutParam alb;
			alb.SetVariableSizeX(SUiLayoutParam::szByContainer, 1.0f);
			alb.GrowFactor = 1.0f;
			p_lo_upr->SetLayoutBlock(alb);
		}
		SUiLayout * p_lo_lwr = lo_main.InsertItem();
		p_lo_lwr->SetID(loidLwr);
		{
			SUiLayoutParam alb(DIREC_HORZ, 0, SUiLayoutParam::alignStretch);
			alb.SetVariableSizeX(SUiLayoutParam::szByContainer, 1.0f);
			alb.GrowFactor = 1.0f;
			alb.Flags &= ~SUiLayoutParam::fContainerWrap;
			p_lo_lwr->SetLayoutBlock(alb);
		}
		SUiLayout * p_lo_f = p_lo_lwr->InsertItem();
		p_lo_f->SetID(loidF);
		{
			SUiLayoutParam alb;
			alb.SetVariableSizeX(SUiLayoutParam::szByContainer, 1.0f);
			alb.SetVariableSizeY(SUiLayoutParam::szByContainer, 1.0f);
			//alb.ShrinkFactor = 0.0f;
			p_lo_f->SetLayoutBlock(alb);
		}
		SUiLayout * p_lo_b = p_lo_lwr->InsertItem();
		p_lo_b->SetID(loidB);
		{
			SUiLayoutParam alb;
			alb.SetFixedSizeX(f_width);
			alb.SetVariableSizeY(SUiLayoutParam::szByContainer, 1.0f);
			alb.ShrinkFactor = 0.0f;
			p_lo_b->SetLayoutBlock(alb);
		}
		{
			SUiLayout::Param p;
			p.ForceWidth = 360.0f;
			p.ForceHeight = 240.0f;
			lo_main.Evaluate(&p);
			//
			{
				const FRect rc = lo_main.GetFrameAdjustedToParent();
				//SLCHECK_EQ(rc.Width(), p.ForceWidth);
				//SLCHECK_EQ(rc.Height(), p.ForceHeight);
			}
			{
				const SUiLayout * p_lo_found = lo_main.FindByID(loidUpr);
				SLCHECK_EQ(p_lo_found, p_lo_upr);
				const FRect rc = p_lo_upr->GetFrameAdjustedToParent();
				SLCHECK_EQ(rc.Width(), p.ForceWidth);
				SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
			}
			{
				const SUiLayout * p_lo_found = lo_main.FindByID(loidLwr);
				SLCHECK_EQ(p_lo_found, p_lo_lwr);
				const FRect rc = p_lo_lwr->GetFrameAdjustedToParent();
				SLCHECK_EQ(rc.Width(), p.ForceWidth);
				SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
			}
			{
				const SUiLayout * p_lo_found = lo_main.FindByID(loidF);
				SLCHECK_EQ(p_lo_found, p_lo_f);
				const FRect rc = p_lo_f->GetFrameAdjustedToParent();
				SLCHECK_EQ(rc.Width(), p.ForceWidth-f_width);
				SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
			}
			{
				const SUiLayout * p_lo_found = lo_main.FindByID(loidB);
				SLCHECK_EQ(p_lo_found, p_lo_b);
				const FRect rc = p_lo_b->GetFrameAdjustedToParent();
				SLCHECK_EQ(rc.Width(), f_width);
				SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
			}
		}
		{
			SString js_buf;
			(temp_buf = root_path).SetLastSlash().Cat("out").SetLastSlash().Cat("lo-test.json");
			{
				SFile f_out(temp_buf, SFile::mWrite);
				SLCHECK_NZ(f_out.IsValid());
				SJson * p_js_obj = lo_main.ToJsonObj();
				SLCHECK_NZ(p_js_obj);
				p_js_obj->ToStr(temp_buf);
				SJson::FormatText(temp_buf, js_buf);
				f_out.Write(js_buf, js_buf.Len());
				delete p_js_obj;
			}
			//
			{
				(temp_buf = root_path).SetLastSlash().Cat("out").SetLastSlash().Cat("lo-test.json");
				SFile f_in(temp_buf, SFile::mRead);
				size_t actual_size = 0;
				SLCHECK_NZ(f_in.IsValid());
				SUiLayout lo_main2;
				STempBuffer in_buf(SMEGABYTE(1));
				SLCHECK_NZ(in_buf.IsValid());
				SLCHECK_NZ(f_in.ReadAll(in_buf, 0, &actual_size));
				js_buf.Z().CatN(in_buf.cptr(), actual_size);
				SJson * p_js_obj = SJson::Parse(js_buf);
				SLCHECK_NZ(p_js_obj);
				if(p_js_obj) {
					SLCHECK_NZ(lo_main2.FromJsonObj(p_js_obj));
					{
						SUiLayout::Param p;
						p.ForceWidth = 360.0f;
						p.ForceHeight = 240.0f;
						lo_main2.Evaluate(&p);
						//
						{
							const FRect rc = lo_main2.GetFrameAdjustedToParent();
							//SLCHECK_EQ(rc.Width(), p.ForceWidth);
							//SLCHECK_EQ(rc.Height(), p.ForceHeight);
						}
						{
							const SUiLayout * p_lo_found = lo_main2.FindByID(loidUpr);
							SLCHECK_NZ(p_lo_found);
							if(p_lo_found) {
								SLCHECK_NZ(p_lo_found->GetLayoutBlockC().IsEq(p_lo_upr->GetLayoutBlockC()));
								const FRect rc = p_lo_found->GetFrameAdjustedToParent();
								SLCHECK_EQ(rc.Width(), p.ForceWidth);
								SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
							}
						}
						{
							const SUiLayout * p_lo_found = lo_main2.FindByID(loidLwr);
							SLCHECK_NZ(p_lo_found);
							if(p_lo_found) {
								SLCHECK_NZ(p_lo_found->GetLayoutBlockC().IsEq(p_lo_lwr->GetLayoutBlockC()));
								const FRect rc = p_lo_found->GetFrameAdjustedToParent();
								SLCHECK_EQ(rc.Width(), p.ForceWidth);
								SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
							}
						}
						{
							const SUiLayout * p_lo_found = lo_main2.FindByID(loidF);
							SLCHECK_NZ(p_lo_found);
							if(p_lo_found) {
								SLCHECK_NZ(p_lo_found->GetLayoutBlockC().IsEq(p_lo_f->GetLayoutBlockC()));
								const FRect rc = p_lo_found->GetFrameAdjustedToParent();
								SLCHECK_EQ(rc.Width(), p.ForceWidth-f_width);
								SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
							}
						}
						{
							const SUiLayout * p_lo_found = lo_main2.FindByID(loidB);
							SLCHECK_NZ(p_lo_found);
							if(p_lo_found) {
								SLCHECK_NZ(p_lo_found->GetLayoutBlockC().IsEq(p_lo_b->GetLayoutBlockC()));
								const FRect rc = p_lo_found->GetFrameAdjustedToParent();
								SLCHECK_EQ(rc.Width(), f_width);
								SLCHECK_EQ(rc.Height(), p.ForceHeight/2.0f);
							}
						}
					}
				}
			}
		}
	}
	return CurrentStatus;
}

SLTEST_R(HASHTAB)
{
	SString in_buf;
	SString line_buf;
	{
		const uint test_iter_count = 1000000;
		const size_t ht_size_tab[] = { 10, 100, 1000, 100000 };
		for(uint hts_idx = 0; hts_idx < SIZEOFARRAY(ht_size_tab); hts_idx++) {
			size_t ht_size = ht_size_tab[hts_idx];
			uint   _count = 0;
			SStrCollection ptr_collection;
			PtrHashTable ht(ht_size);

			(in_buf = GetSuiteEntry()->InPath).SetLastSlash().Cat("email-list.txt");
			SFile inf(in_buf, SFile::mRead);
			THROW(SLCHECK_NZ(inf.IsValid()));
			while(inf.ReadLine(line_buf, SFile::rlfChomp)) {
				char * p_str = newStr(line_buf);
				THROW(SLCHECK_NZ(ptr_collection.insert(p_str)));
				//
				// �������� ������� ��������� � ���, ������ - ���
				//
				if(_count % 2) {
					THROW(SLCHECK_NZ(ht.Add(p_str, _count+1, 0)));
				}
				else {
					//
				}
				_count++;
			}
			THROW(SLCHECK_EQ(ptr_collection.getCount(), _count));
			for(uint i = 0; i < test_iter_count; i++) {
				uint idx = SLS.GetTLA().Rg.GetUniformInt(_count);
				THROW(SLCHECK_LT((long)idx, (long)_count));
				char * p_str = ptr_collection.at(idx);
				{
					uint val = 0;
					uint pos = 0;
					if(idx % 2) {
						SLCHECK_NZ(ht.Search(p_str, &val, &pos));
						void * ptr = ht.Get(pos);
						SLCHECK_NZ(ptr);
						SLCHECK_EQ(ptr, (const void *)p_str);
						SLCHECK_EQ(val, idx+1);
					}
					else {
						SLCHECK_Z(ht.Search(p_str, &val, &pos));
					}
				}
			}
		}
	}
	{
		//
		// 
		//
		const size_t ht_size_tab[] = { 10, 100, 1000, 100000 };
		for(uint hts_idx = 0; hts_idx < SIZEOFARRAY(ht_size_tab); hts_idx++) {
			size_t ht_size = ht_size_tab[hts_idx];
			uint   _count = 0;
			SStrCollection ptr_collection;
			TokenSymbHashTable tsht(ht_size);

			(in_buf = GetSuiteEntry()->InPath).SetLastSlash().Cat("email-list.txt");
			SFile inf(in_buf, SFile::mRead);
			THROW(SLCHECK_NZ(inf.IsValid()));
			while(inf.ReadLine(line_buf, SFile::rlfChomp)) {
				char * p_str = newStr(line_buf);
				THROW(SLCHECK_NZ(ptr_collection.insert(p_str)));
				_count++;
			}
			THROW(SLCHECK_EQ(ptr_collection.getCount(), _count));
			{
				for(long key = 1; key < ptr_collection.getCountI(); key++) {
					SLCHECK_Z(tsht.Get(key, 0));
					SLCHECK_NZ(tsht.Put(key, ptr_collection.at(key-1)));
				}
			}
			{
				for(long key = 1; key < ptr_collection.getCountI(); key++) {
					SLCHECK_EQ(tsht.Put(key, ptr_collection.at(key-1)), 1);
				}
			}
			{
				for(long key = 1; key < ptr_collection.getCountI(); key++) {
					SLCHECK_NZ(tsht.Get(key, &line_buf));
					SLCHECK_EQ(line_buf, ptr_collection.at(key-1));
				}
			}
		}
	}
	CATCH
		CurrentStatus = 0;
	ENDCATCH
	return CurrentStatus;
}
//
// @sandbox {
// ��������� ����������
//
SLTEST_R(iterator)
{
	class _Container {
	public:
		class Iterator {
			const _Container & R_Obj;
		public:
			Iterator(const _Container & rObj) : R_Obj(rObj), Idx(0)
			{
			}
			Iterator(const Iterator & rS) : R_Obj(rS.R_Obj), Idx(rS.Idx)
			{
			}
			bool operator == (const Iterator & rS) const
			{
				return (&rS.R_Obj == &R_Obj && rS.Idx == Idx);
			}
			bool operator != (const Iterator & rS) const
			{
				return (&rS.R_Obj != &R_Obj || rS.Idx != Idx);
			}
			const int * operator * () const { return &R_Obj[Idx]; }
			const int & operator & () const { return R_Obj[Idx]; }
			Iterator & operator++ ()
			{
				if(Idx < R_Obj.C)
					Idx++;
				return *this;
			}
			Iterator operator++ (int)
			{
				Iterator preserve(*this);
				if(Idx < R_Obj.C)
					Idx++;
				return preserve;
			}
			uint   Idx;
		};
		_Container() : C(0)
		{
			MEMSZERO(Items);
		}
		const int & operator [](size_t idx) const
		{ 
			static int dummy = 0;
			return (idx < C) ? Items[idx] : dummy;
		}
		Iterator begin() const
		{
			Iterator it(*this);
			return it;
		}
		Iterator end() const
		{
			Iterator it(*this);
			it.Idx = C;
			return it;
		}
		int    Items[1024];
		uint   C;
	};
	{
		_Container c;
		int pattern_sum = 0;
		{
			for(uint i = 0; i < 103; i++) {
				c.Items[i] = (int)(i+1);
				c.C++;
				pattern_sum += (i+1);
			}
		}
		{
			int s = 0;
			for(auto item : c) {
				s += *item;
			}
			assert(s == pattern_sum);
		}
	}
	return CurrentStatus;
}

// } @sandbox

int SColorSet::Test()
{
	int    ok = 1;
	SString temp_buf;
	Z();
	const SColor primary(0, 0xff, 0xff);
	const SColor secondary(0, 0x80, 0x00);
	{
		{
			ComplexColorBlock ccb;
			THROW(ParseComplexColorBlock("#00ffff", ccb));
			THROW(ccb.RefSymb.IsEmpty());
			THROW(ccb.Func == funcNone);
			THROW(ccb.C == SColor(SClrCyan));
			THROW(ccb.C == primary);
			THROW(Put("primary", new ComplexColorBlock(ccb)));
		}
		{
			ComplexColorBlock ccb;
			THROW(ParseComplexColorBlock("#008000", ccb));
			THROW(ccb.RefSymb.IsEmpty());
			THROW(ccb.Func == funcNone);
			THROW(ccb.C == SColor(SClrGreen));
			THROW(ccb.C == secondary);
			THROW(Put("secondary", new ComplexColorBlock(ccb)));
		}
	}
	{
		const char * p_symb = "color001";
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("#ff0000", ccb));
		THROW(ccb.RefSymb.IsEmpty());
		THROW(ccb.Func == funcNone);
		THROW(ccb.C == SColor(SClrRed));
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
		}
	}
	{
		const char * p_symb = "color002";
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("#ff0000|0.5", ccb));
		THROW(ccb.RefSymb.IsEmpty());
		THROW(ccb.Func == funcNone);
		THROW(ccb.C == SColor(0xff, 0x00, 0x00).SetAlphaF(0.5f));
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
		}
	}
	{
		const char * p_symb = "color003";
		const SString ref_symb("primary");
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock(temp_buf.Z().CatChar('$').Cat(ref_symb), ccb));
		THROW(ccb.RefSymb.IsEqiAscii(ref_symb));
		THROW(ccb.Func == funcNone);
		THROW(ccb.C == ZEROCOLOR);
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb_ref;
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(Get(ref_symb, &ccb_ref));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
			//
			SColor color_resolved;
			THROW(Get(p_symb, color_resolved) > 0);
			THROW(color_resolved == ccb_ref.C);
		}
		//
		//ResolveComplexColorBlock(ccb, )
	}
	{
		const char * p_symb = "color004";
		const SString ref_symb("primary");
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("$Primary|0.7", ccb));
		THROW(ccb.RefSymb.IsEqiAscii(ref_symb));
		THROW(ccb.Func == funcNone);
		THROW(ccb.C == SColor(ZEROCOLOR).SetAlphaF(0.7f));
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb_ref;
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(Get(ref_symb, &ccb_ref));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
			//
			SColor color_resolved;
			THROW(Get(p_symb, color_resolved) > 0);
			THROW(color_resolved == SColor(ccb_ref.C).SetAlphaF(0.7f));
		}
	}
	{
		const char * p_symb = "color005";
		const SString ref_symb1("primary");
		const SString ref_symb2("secondary");
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("lerp $Primary $Secondary 0.4", ccb));
		THROW(ccb.RefSymb.IsEmpty());
		THROW(ccb.Func == funcLerp);
		THROW(ccb.C == ZEROCOLOR);
		THROW(ccb.ArgList.getCount() == 3);
		THROW(ccb.ArgList.at(0)->GetType() == argtRefColor);
		THROW(ccb.ArgList.at(0)->RefSymb.IsEqiAscii("primary"));
		THROW(ccb.ArgList.at(0)->C == ZEROCOLOR);
		THROW(ccb.ArgList.at(1)->GetType() == argtRefColor);
		THROW(ccb.ArgList.at(1)->RefSymb.IsEqiAscii("secondary"));
		THROW(ccb.ArgList.at(1)->C == ZEROCOLOR);
		THROW(ccb.ArgList.at(2)->GetType() == argtNumber);
		THROW(ccb.ArgList.at(2)->F == 0.4f);
		THROW(ccb.ArgList.at(2)->C == ZEROCOLOR);
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb_ref1;
			ComplexColorBlock ccb_ref2;
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(Get(ref_symb1, &ccb_ref1));
			THROW(Get(ref_symb2, &ccb_ref2));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
			//
			SColor color_resolved;
			THROW(Get(p_symb, color_resolved) > 0);
			THROW(color_resolved == SColor::Lerp(primary, secondary, 0.4f));
		}
	}
	{
		const char * p_symb = "color006";
		const SString ref_symb("primary");
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("lighten $Primary 0.1", ccb));
		THROW(ccb.RefSymb.IsEmpty());
		THROW(ccb.Func == funcLighten);
		THROW(ccb.C == ZEROCOLOR);
		THROW(ccb.ArgList.getCount() == 2);
		THROW(ccb.ArgList.at(0)->GetType() == argtRefColor);
		THROW(ccb.ArgList.at(0)->RefSymb.IsEqiAscii("primary"));
		THROW(ccb.ArgList.at(0)->C == ZEROCOLOR);
		THROW(ccb.ArgList.at(1)->GetType() == argtNumber);
		THROW(ccb.ArgList.at(1)->F == 0.1f);
		THROW(ccb.ArgList.at(1)->C == ZEROCOLOR);
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb_ref;
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(Get(ref_symb, &ccb_ref));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
			//
			SColor color_resolved;
			THROW(Get(p_symb, color_resolved) > 0);
			THROW(color_resolved == SColor(ccb_ref.C).Lighten(0.1f));
		}
	}
	{
		const char * p_symb = "color007";
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("darken #green|180 0.3", ccb));
		THROW(ccb.RefSymb.IsEmpty());
		THROW(ccb.Func == funcDarken);
		THROW(ccb.C == ZEROCOLOR);
		THROW(ccb.ArgList.getCount() == 2);
		THROW(ccb.ArgList.at(0)->GetType() == argtAbsoluteColor);
		THROW(ccb.ArgList.at(0)->RefSymb.IsEmpty());
		THROW(ccb.ArgList.at(0)->C == SColor(SClrGreen).SetAlpha(180));
		THROW(ccb.ArgList.at(1)->GetType() == argtNumber);
		THROW(ccb.ArgList.at(1)->F == 0.3f);
		THROW(ccb.ArgList.at(1)->C == ZEROCOLOR);
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
			//
			SColor color_resolved;
			THROW(Get(p_symb, color_resolved) > 0);
			THROW(color_resolved == SColor(SClrGreen).Darken(0.3f).SetAlpha(180));
		}
	}
	{
		const char * p_symb = "color008";
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("grey 0.14", ccb));
		THROW(ccb.RefSymb.IsEmpty());
		THROW(ccb.Func == funcGrey);
		THROW(ccb.C == ZEROCOLOR);
		THROW(ccb.ArgList.getCount() == 1);
		THROW(ccb.ArgList.at(0)->GetType() == argtNumber);
		THROW(ccb.ArgList.at(0)->RefSymb.IsEmpty());
		THROW(ccb.ArgList.at(0)->C == ZEROCOLOR);
		THROW(ccb.ArgList.at(0)->F == 0.14f);
		THROW(Put(p_symb, new ComplexColorBlock(ccb)));
		{
			ComplexColorBlock ccb2;
			THROW(Get(p_symb, &ccb2));
			THROW(ccb2.RefSymb == ccb.RefSymb);
			THROW(ccb2.Func == ccb.Func);
			THROW(ccb2.C == ccb.C);
			//
			SColor color_resolved;
			THROW(Get(p_symb, color_resolved) > 0);
			THROW(color_resolved == SColor(0.14f));
		}
	}
	{
		ComplexColorBlock ccb;
		THROW(ParseComplexColorBlock("nofunction $secondary 0.14", ccb) == 0);
	}
	CATCHZOK
	return ok;
}

SLTEST_R(SColorSet)
{
	SColorSet cs;
	SLCHECK_NZ(cs.Test());
	return CurrentStatus;
}
