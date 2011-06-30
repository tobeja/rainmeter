/*
  Copyright (C) 2011 Birunthan Mohanathas (www.poiru.net)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "StdAfx.h"
#include "PlayerFoobar.h"

CPlayer* CPlayerFoobar::c_Player = NULL;

/*
** CPlayerFoobar
**
** Constructor.
**
*/
CPlayerFoobar::CPlayerFoobar() : CPlayer(),
	m_Window(),
	m_FooWindow()
{
	Initialize();
}

/*
** ~CPlayerFoobar
**
** Constructor.
**
*/
CPlayerFoobar::~CPlayerFoobar()
{
	c_Player = NULL;
	Uninitialize();
}

/*
** Create
**
** Creates a shared class object.
**
*/
CPlayer* CPlayerFoobar::Create()
{
	if (!c_Player)
	{
		c_Player = new CPlayerFoobar();
	}

	return c_Player;
}

/*
** Initialize
**
** Create receiver window.
**
*/
void CPlayerFoobar::Initialize()
{
	HINSTANCE hInstance = GetModuleHandle(NULL);

	// Create windows class
	WNDCLASS wc = {0};
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"NowPlayingFoobarClass";
	RegisterClass(&wc);

	// Create window
	m_Window = CreateWindow(L"NowPlayingFoobarClass",
							L"ReceiverWindow",
							WS_DISABLED,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							CW_USEDEFAULT,
							NULL,
							NULL,
							hInstance,
							this);

	m_FooWindow = FindWindow(L"foo_rainmeter_class", NULL);
	if (m_FooWindow)
	{
		int version = (int)SendMessage(m_FooWindow, WM_USER, 0, FOO_GETVERSION);
		if (version < 100)
		{
			std::wstring error = L"Your copy of the foo_rainmeter.dll plugin for foobar2000 is outdated.\n";
			error += L"Please download the latest version and try again.";
			MessageBox(NULL, error.c_str(), L"Rainmeter", MB_OK | MB_ICONERROR | MB_TOPMOST);
			m_FooWindow = NULL;
		}
		else
		{
			m_Initialized = true;
			SendMessage(m_FooWindow, WM_USER, (WPARAM)m_Window, FOO_SETCALLBACK);
		}
	}
}

/*
** Uninitialize
**
** Destroy reciever window.
**
*/
void CPlayerFoobar::Uninitialize()
{
	if (m_FooWindow)
	{
		SendMessage(m_FooWindow, WM_USER, 0, FOO_REMOVECALLBACK);
	}

	DestroyWindow(m_Window);
	UnregisterClass(L"NowPlayingFoobarClass", GetModuleHandle(NULL));
}

/*
** WndProc
**
** Window procedure for the reciever window.
**
*/
LRESULT CALLBACK CPlayerFoobar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static CPlayerFoobar* player;

	switch (msg)
	{
	case WM_CREATE:
		{
			// Get pointer to the CPlayerFoobar class from the CreateWindow call
			LPVOID params = ((CREATESTRUCT*)lParam)->lpCreateParams;
			player = (CPlayerFoobar*)params;
			return 0;
		}

	case WM_USER:
		switch (lParam)
		{
		case FOO_GETVERSION:
			return 100;

		case FOO_STATECHANGE:
			{
				PLAYSTATE ps = (PLAYSTATE)wParam;
				if (ps == PLAYER_STOPPED)
				{
					player->ClearData();
				}
				else
				{
					player->m_State = ps;
				}
			}
			break;

		case FOO_TIMECHANGE:
			player->m_Position = (UINT)wParam;
			break;

		case FOO_VOLUMECHANGE:
			player->m_Volume = (UINT)wParam;
			break;

		case FOO_PLAYERSTART:
			player->m_FooWindow = (HWND)wParam;
			break;

		case FOO_PLAYERQUIT:
			player->m_Initialized = false;
			player->ClearData();
			break;
		}
		return 0;

	case WM_COPYDATA:
		{
			PCOPYDATASTRUCT cds = (PCOPYDATASTRUCT)lParam;

			if (cds->dwData == FOO_TRACKCHANGE)
			{
				if (player->m_State != PLAYER_PLAYING)
				{
					player->m_State = PLAYER_PLAYING;
				}

				// In the format "TITLE ARTIST ALBUM LENGTH RATING" (seperated by \t)
				WCHAR buffer[1024];
				MultiByteToWideChar(CP_UTF8, 0, (char*)cds->lpData, cds->cbData, buffer, 1024);
				player->m_Artist = buffer;

				WCHAR* token = wcstok(buffer, L"\t");
				if (token)
				{
					player->m_Title = token;
				}
				token = wcstok(NULL, L"\t");
				if (token)
				{
					player->m_Artist = token;
				}
				token = wcstok(NULL, L"\t");
				if (token)
				{
					player->m_Album = token;
				}
				token = wcstok(NULL, L"\t");
				if (token)
				{
					player->m_Duration = _wtoi(token);
				}
				token = wcstok(NULL, L"\t");
				if (token)
				{
					player->m_Rating = _wtoi(token);
				}
				token = wcstok(NULL, L"\t");
				if (token && wcscmp(token, player->m_FilePath.c_str()) != 0)
				{
					// If different file
					++player->m_TrackCount;
					player->m_FilePath = token;
					player->m_Position = 0;

					if (player->m_HasCoverMeasure || player->m_InstanceCount == 0)
					{
						player->FindCover();
					}

					if (player->m_HasLyricsMeasure)
					{
						player->FindLyrics();
					}
				}
			}
		}
		return 0;

	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
}

/*
** UpdateData
**
** Called during each update of the main measure.
**
*/
void CPlayerFoobar::UpdateData()
{
}

/*
** Pause
**
** Handles the Pause bang.
**
*/
void CPlayerFoobar::Pause()
{
	SendMessage(m_FooWindow, WM_USER, 0, FOO_PAUSE);
}

/*
** Play
**
** Handles the Play bang.
**
*/
void CPlayerFoobar::Play()
{
	SendMessage(m_FooWindow, WM_USER, 0, (m_State == PLAYER_PAUSED) ? FOO_PLAYPAUSE : FOO_PLAY);
}

/*
** Stop
**
** Handles the Stop bang.
**
*/
void CPlayerFoobar::Stop() 
{
	SendMessage(m_FooWindow, WM_USER, 0, FOO_STOP);
}

/*
** Next
**
** Handles the Next bang.
**
*/
void CPlayerFoobar::Next() 
{
	SendMessage(m_FooWindow, WM_USER, 0, FOO_NEXT);
}

/*
** Previous
**
** Handles the Previous bang.
**
*/
void CPlayerFoobar::Previous() 
{
	SendMessage(m_FooWindow, WM_USER, 0, FOO_PREVIOUS);
}

/*
** SetPosition
**
** Handles the SetPosition bang.
**
*/
void CPlayerFoobar::SetPosition(int position)
{
	SendMessage(m_FooWindow, WM_USER, position, FOO_SETPOSITION);
}

/*
** SetVolume
**
** Handles the SetVolume bang.
**
*/
void CPlayerFoobar::SetVolume(int volume) 
{
	SendMessage(m_FooWindow, WM_USER, volume, FOO_SETVOLUME);
}

/*
** ClosePlayer
**
** Handles the ClosePlayer bang.
**
*/
void CPlayerFoobar::ClosePlayer()
{
	SendMessage(m_FooWindow, WM_USER, 0, FOO_QUITPLAYER);
}

/*
** OpenPlayer
**
** Handles the OpenPlayer bang.
**
*/
void CPlayerFoobar::OpenPlayer(std::wstring& path)
{
	if (!m_Initialized)
	{
		if (path.empty())
		{
			// Gotta figure out where foobar2000 is located at
			HKEY hKey;
			RegOpenKeyEx(HKEY_CLASSES_ROOT,
						 L"Applications\\foobar2000.exe\\shell\\open\\command",
						 0,
						 KEY_QUERY_VALUE,
						 &hKey);

			DWORD size = 512;
			WCHAR* data = new WCHAR[size];
			DWORD type = 0;

			if (RegQueryValueEx(hKey,
								NULL,
								NULL,
								(LPDWORD)&type,
								(LPBYTE)data,
								(LPDWORD)&size) == ERROR_SUCCESS)
			{
				if (type == REG_SZ && data[0] == L'\"')
				{
					path = data;
					path.erase(0, 1);	// Get rid of the leading quote
					std::wstring::size_type pos = path.find_first_of(L'\"');

					if (pos != std::wstring::npos)
					{
						path.resize(pos);	// Get rid the last quote and everything after it
						ShellExecute(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);
						path = path;
					}
					else
					{
						path.clear();
					}
				}
			}

			delete [] data;
			RegCloseKey(hKey);
		}
		else
		{
			ShellExecute(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOW);
		}
	}
	else
	{
		SendMessage(m_FooWindow, WM_USER, 0, FOO_SHOWPLAYER);
	}
}