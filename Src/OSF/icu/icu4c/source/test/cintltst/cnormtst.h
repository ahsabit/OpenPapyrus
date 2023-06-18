// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2001, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/********************************************************************************
*
* File CNORMTST.H
*
* Modification History:
*        Name                     Description            
*     Madhu Katragadda            Converted to C
*     synwee                      added test for quick check
*     synwee                      added test for checkFCD
*********************************************************************************
*/
#ifndef _NORMTST
#define _NORMTST
/**
 *  tests for u_normalization
 */

#include "cintltst.h"
    
    void TestDecomp();
    void TestCompatDecomp();
    void TestCanonDecompCompose();
    void TestCompatDecompCompose();
    void TestNull();
    void TestQuickCheck();
    void TestCheckFCD();

    /*internal functions*/
    
/*    static void assertEqual(const char16_t * result,  const char16_t * expected, int32_t index);
*/
    static void assertEqual(const char16_t * result,  const char * expected, int32_t index);


    


#endif
