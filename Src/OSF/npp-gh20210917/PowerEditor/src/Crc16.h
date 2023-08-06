// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//
#ifndef _CRC16_H_
#define _CRC16_H_

class CRC16_ISO_3309 {
public:
	CRC16_ISO_3309(ushort polynom = /*0x1021*/SlConst::CrcPoly_CCITT16, ushort initVal = 0xFFFF) : _polynom(polynom), _initVal(initVal) 
	{
	}
	~CRC16_ISO_3309()
	{
	}
	void set(ushort polynom, ushort initVal) 
	{
		_polynom = polynom;
		_initVal = initVal;
	}
	ushort calculate(uchar * data, ushort count)
	{
		ushort fcs = _initVal;
		ushort d, i, k;
		for(i = 0; i<count; i++) {
			d = *data++ << 8;
			for(k = 0; k<8; k++) {
				if((fcs ^ d) & 0x8000)
					fcs = (fcs << 1) ^ _polynom;
				else
					fcs = (fcs << 1);
				d <<= 1;
			}
		}
		return(fcs);
	}
private:
	ushort _polynom;
	ushort _initVal;
};

const bool bits8 = true;
const bool bits16 = false;

class CRC16 : public CRC16_ISO_3309 {
public:
	CRC16()
	{
	}
	~CRC16()
	{
	}
	ushort calculate(const uchar * data, ushort count)
	{
		assert(data != NULL);
		assert(count != 0);
		//unsigned short wordResult;
		uchar * pBuffer = new uchar[count];
		// Reverse all bits of the byte then copy the result byte by byte in the array
		for(int i = 0; i < count; i++)
			pBuffer[i] = reverseByte<uchar>(data[i]);
		// calculate CRC : by default polynom = /*0x1021*/SlConst::CrcPoly_CCITT16, init val = 0xFFFF)
		ushort wordResult = CRC16_ISO_3309::calculate(pBuffer, count);
		// Reverse the WORD bits
		wordResult = reverseByte<ushort>(wordResult);
		// XOR FFFF
		wordResult ^= 0xFFFF;
		// Invert MSB/LSB
		wordResult = wordResult << 8 | wordResult >> 8;
		delete [] pBuffer;
		return wordResult;
	}
private:
	template <class IntType>
	IntType reverseByte(IntType val2Reverses)
	{
		IntType reversedValue = 0;
		long mask = 1;
		int nBits = sizeof(val2Reverses) * 8;
		for(int i = 0; i < nBits; i++)
			if((mask << i) & val2Reverses)
				reversedValue += (mask << (nBits - 1 - i));
		return reversedValue;
	};
};

#endif