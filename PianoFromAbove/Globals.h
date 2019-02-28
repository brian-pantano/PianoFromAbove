/*************************************************************************************************
*
* File: Globals.h
*
* Description: Global variables. Mostly window handlers.
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <Windows.h>
#include "Misc.h"

extern HINSTANCE g_hInstance;
extern HWND g_hWnd;
extern HWND g_hWndBar;
extern HWND g_hWndLibDlg;
extern HWND g_hWndGfx;
extern TSQueue< MSG > g_MsgQueue; // Producer/consumer to hold events for our game thread

#define ERRORANDRETURN( hwnd, msg, retval ) { MessageBox( ( hwnd ), ( msg ), TEXT( "Error" ), MB_OK | MB_ICONERROR ); return ( retval ); }
