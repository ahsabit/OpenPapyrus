/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */
#include <internal/openssl-crypto-internal.h>
#pragma hdrstop
#include <async_local.h> // This must be the first #include file 

#ifdef ASYNC_NULL
int ASYNC_is_capable()
{
	return 0;
}

void async_local_cleanup()
{
}
#endif
