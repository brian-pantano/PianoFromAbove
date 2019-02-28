/*************************************************************************************************
*
* File: MainProcs.h
*
* Description: Defines the main GUI functions. Not C++ :/
*
* Copyright (c) 2010 Brian Pantano. All rights reserved.
*
*************************************************************************************************/
#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <string>
using namespace std;

// Message handlers for the main windows
LRESULT WINAPI WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
HMENU GetMainMenu();
VOID SizeWindows( int iMainWidth, int iMainHeight );

LRESULT WINAPI GfxProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
VOID CopyMenuState( HMENU hMenuSrc, HMENU hMenuDest );
VOID CopyMenuItem( HMENU hMenuSrc, INT iItemSrc, HMENU hMenuDest, INT iItemDest, BOOL bByPosition );

LRESULT WINAPI BarProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
INT_PTR WINAPI NoteLabelProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
HWND CreateRebar( HWND hWndOwner );
VOID DrawSliderChannel( LPNMCUSTOMDRAW lpnmcd, HWND hWndOwner );

LRESULT WINAPI PosnProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
VOID GetChannelRect( HWND hWnd, RECT *rcChannel );
VOID GetThumbRect( HWND hWnd, int iPosition, const RECT *rcChannel, RECT *rcThumb );
INT GetThumbPosition( short iXPos, RECT *rcChannel );
VOID MoveThumbPosition( int iPositionNew, int &iPosition, HWND hWnd, RECT *rcChannel, RECT *rcThumbOld, BOOL bUpdateGame = TRUE );

INT_PTR WINAPI LibDlgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
VOID PopulateLibrary( HWND hWndLibrary );
VOID SortLibrary( HWND hWndLibrary, INT iSortCol );
INT CALLBACK CompareLibrary( LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort );
VOID AddSingleLibraryFile( HWND hWndLibrary, const wstring &sFile );
BOOL PlayLibrary( HWND hWndLibrary, int iItem, INT ePlayMode, bool bCustomSettings = false );

INT_PTR WINAPI AboutProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

VOID HandOffMsg( UINT msg, WPARAM wParam, LPARAM lParam );
VOID ShowLibrary( BOOL bShow );
VOID ShowControls( BOOL bShow );
VOID ShowKeyboard( BOOL bShow );
VOID ShowNoteLabels( BOOL bShow );
VOID SetOnTop( BOOL bOnTop );
VOID SetFullScreen( BOOL bFullScreen );
VOID SetZoomMove( BOOL bZoomMove );
VOID SetMute( BOOL bMute );
VOID SetSpeed( DOUBLE dSpeed );
VOID SetNSpeed( DOUBLE dSpeed );
VOID SetVolume( DOUBLE dVolume );
VOID SetPosition( INT iPosition );
VOID SetLoop( BOOL bClear );
VOID SetMetronome( INT iMetronome );
VOID SetPlayable( BOOL bPlayable );
VOID SetPlayMode( INT ePlayMode );
VOID SetLearnMode( INT eLearnMode );
VOID SetPlayPauseStop( BOOL bPlay, BOOL bPause, BOOL bStop );
BOOL PlayFile( const wstring &sFile, int ePlayMode, bool bCustomSettings = false, bool bLibraryEligible = false );
VOID CheckActivity( BOOL bIsActive, POINT *ptNew = NULL, BOOL bToggleEnable = false );