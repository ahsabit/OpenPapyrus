/*
 * The copyright in this software is being made available under the 2-clauses
 * BSD License, included below. This software may be subject to other third
 * party and contributor rights, including patent rights, and no such rights
 * are granted under this license.
 *
 * Copyright (c) 2005, Herve Drolon, FreeImage Team
 * Copyright (c) 2008, 2011-2012, Centre National d'Etudes Spatiales (CNES), FR
 * Copyright (c) 2012, CS Systemes d'Information, France
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */
#include "opj_includes.h"
#pragma hdrstop
// 
// Utility functions
// 
#ifdef OPJ_CODE_NOT_USED
#ifndef _WIN32
static char* i2a(unsigned i, char * a, unsigned r)
{
	if(i / r > 0) {
		a = i2a(i / r, a, r);
	}
	*a = SlConst::P_Rdx36DigU[i % r];
	return a + 1;
}

/**
   Transforms integer i into an ascii string and stores the result in a;
   string is encoded in the base indicated by r.
   @param i Number to be converted
   @param a String result
   @param r Base of value; must be in the range 2 - 36
   @return Returns a
 */
static char * _itoa(int i, char * a, int r)
{
	r = ((r < 2) || (r > 36)) ? 10 : r;
	if(i < 0) {
		*a = '-';
		*i2a(-i, a + 1, r) = 0;
	}
	else {
		*i2a(i, a, r) = 0;
	}
	return a;
}

#endif /* !_WIN32 */
#endif
/**
 * Default callback function.
 * Do nothing.
 */
static void opj_default_callback(const char * msg, void * client_data)
{
	OPJ_ARG_NOT_USED(msg);
	OPJ_ARG_NOT_USED(client_data);
}

boolint opj_event_msg(opj_event_mgr_t* p_event_mgr, int32_t event_type, const char * fmt, ...)
{
#define OPJ_MSG_SIZE 512 /* 512 bytes should be more than enough for a short message */
	opj_msg_callback msg_handler = 0;
	void * l_data = 0;
	if(p_event_mgr != 0) {
		switch(event_type) {
			case EVT_ERROR:
			    msg_handler = p_event_mgr->error_handler;
			    l_data = p_event_mgr->m_error_data;
			    break;
			case EVT_WARNING:
			    msg_handler = p_event_mgr->warning_handler;
			    l_data = p_event_mgr->m_warning_data;
			    break;
			case EVT_INFO:
			    msg_handler = p_event_mgr->info_handler;
			    l_data = p_event_mgr->m_info_data;
			    break;
			default:
			    break;
		}
		if(msg_handler == 0) {
			return FALSE;
		}
	}
	else {
		return FALSE;
	}
	if((fmt != 0) && (p_event_mgr != 0)) {
		va_list arg;
		char message[OPJ_MSG_SIZE];
		memzero(message, OPJ_MSG_SIZE);
		va_start(arg, fmt); /* initialize the optional parameter list */
		vsnprintf(message, OPJ_MSG_SIZE, fmt, arg); /* parse the format string and put the result in 'message' */
		message[OPJ_MSG_SIZE - 1] = '\0'; /* force zero termination for Windows _vsnprintf() of old MSVC */
		va_end(arg); /* deinitialize the optional parameter list */
		msg_handler(message, l_data); /* output the message to the user program */
	}
	return TRUE;
}

void opj_set_default_event_handler(opj_event_mgr_t * p_manager)
{
	p_manager->m_error_data = 0;
	p_manager->m_warning_data = 0;
	p_manager->m_info_data = 0;
	p_manager->error_handler = opj_default_callback;
	p_manager->info_handler = opj_default_callback;
	p_manager->warning_handler = opj_default_callback;
}
