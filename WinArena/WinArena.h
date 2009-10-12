/////////////////////////////////////////////////////////////////////////////
//
//	$Header: /WinArena/WinArena.h 4     12/31/06 11:09a Lee $
//
//	File: WinArena.h
//
//
/////////////////////////////////////////////////////////////////////////////


#pragma once

#define _WIN32_WINNT 0x0400

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <assert.h>

#include "Profiler.h"


#define SafeRelease(x)		{ if (NULL != (x)) { (x)->Release(); (x) = NULL; } }
#define SafeDelete(x)		{ if (NULL != (x)) { delete (x);     (x) = NULL; } }
#define SafeDeleteArray(x)	{ if (NULL != (x)) { delete [] (x);  (x) = NULL; } }
#define ArraySize(x)		(sizeof(x) / (sizeof((x)[0])))
#define ClampRange(lo,v,hi)	(((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))

template <class T> void Swap(T &a, T &b) { T temp; temp = a; a = b; b = temp; }
