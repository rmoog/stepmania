#include "stdafx.h"
/*
-----------------------------------------------------------------------------
 File: StepMania.cpp

 Desc: 

 Copyright (c) 2001 Chris Danford.  All rights reserved.
-----------------------------------------------------------------------------
*/

//-----------------------------------------------------------------------------
// Includes
//-----------------------------------------------------------------------------
#include "resource.h"

#include "RageHelper.h"
#include "RageScreen.h"
#include "RageTextureManager.h"
#include "RageSound.h"
#include "RageMusic.h"
#include "RageInput.h"

#include "PrefsManager.h"
#include "SongManager.h"
#include "ThemeManager.h"
#include "WindowManager.h"
#include "GameManager.h"
#include "FontManager.h"

#include "WindowSandbox.h"
#include "WindowMenuResults.h"
#include "WindowTitleMenu.h"
#include "WindowPlayerOptions.h"

#include "ScreenDimensions.h"

#include "DXUtil.h"
#include <Afxdisp.h>

//-----------------------------------------------------------------------------
// Links
//-----------------------------------------------------------------------------
#pragma comment(lib, "d3dx8.lib")
#pragma comment(lib, "d3d8.lib")


//-----------------------------------------------------------------------------
// Application globals
//-----------------------------------------------------------------------------
const CString g_sAppName		= "StepMania";
const CString g_sAppClassName	= "StepMania Class";

HWND		g_hWndMain;		// Main Window Handle
HINSTANCE	g_hInstance;	// The Handle to Window Instance
HANDLE		g_hMutex;		// Used to check if an instance of our app is already
const DWORD g_dwWindowStyle = WS_VISIBLE|WS_POPUP|WS_CAPTION|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU;


BOOL	g_bIsActive		= FALSE;	// Whether the focus is on our app



//-----------------------------------------------------------------------------
// Function prototypes
//-----------------------------------------------------------------------------
// Main game functions
LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
BOOL CALLBACK ErrorWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow );	// windows entry point
void MainLoop();		// put everything in here so we can wrap it in a try...catch block
void Update();			// Update the game logic
void Render();			// Render a frame
void ShowFrame();		// Display the contents of the back buffer to the screen

// Functions that work with game objects
HRESULT		CreateObjects( HWND hWnd );	// allocate and initialize game objects
HRESULT		InvalidateObjects();		// invalidate game objects before a display mode change
HRESULT		RestoreObjects();			// restore game objects after a display mode change
VOID		DestroyObjects();			// deallocate game objects when we're done with them

void ApplyGraphicOptions();	// Set the display mode according to the user's preferences

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Application entry point
//-----------------------------------------------------------------------------
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow )
{
	// Initialize ActiveX for Flash
	//AfxEnableControlContainer();

	//
	// Check to see if the app is already running.
	//
	g_hMutex = CreateMutex( NULL, TRUE, g_sAppName );
	if( GetLastError() == ERROR_ALREADY_EXISTS )
	{
		CloseHandle( g_hMutex );
		HELPER.FatalError( "StepMania is already running!" );
	}


	//
	// Make sure the current directory is the root program directory
	//
	if( !DoesFileExist("Songs") )
	{
		// change dir to path of the execuctable
		TCHAR szFullAppPath[MAX_PATH];
		GetModuleFileName(NULL, szFullAppPath, MAX_PATH);
		
		// strip off executable name
		LPSTR pLastBackslash = strrchr(szFullAppPath, '\\');
		*pLastBackslash = '\0';	// terminate the string

		SetCurrentDirectory( szFullAppPath );
	}


	CoInitialize (NULL);    // Initialize COM

	// Register the window class
	WNDCLASS wndClass = { 
		0,
		WndProc,	// callback handler
		0,			// cbClsExtra; 
		0,			// cbWndExtra; 
		hInstance,
		LoadIcon( hInstance, MAKEINTRESOURCE(IDI_ICON) ), 
		LoadCursor( hInstance, IDC_ARROW),
		(HBRUSH)GetStockObject( BLACK_BRUSH ),
		NULL,				// lpszMenuName; 
		g_sAppClassName	// lpszClassName; 
	}; 
 	RegisterClass( &wndClass );


	// Set the window's initial width
	RECT rcWnd;
	SetRect( &rcWnd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
	AdjustWindowRect( &rcWnd, g_dwWindowStyle, FALSE );


	// Create our main window
	g_hWndMain = CreateWindow(
					g_sAppClassName,// pointer to registered class name
					g_sAppName,		// pointer to window name
					g_dwWindowStyle,	// window StyleDef
					CW_USEDEFAULT,	// horizontal position of window
					CW_USEDEFAULT,	// vertical position of window
					RECTWIDTH(rcWnd),	// window width
					RECTHEIGHT(rcWnd),// window height
					NULL,				// handle to parent or owner window
					NULL,				// handle to menu, or child-window identifier
					hInstance,		// handle to application instance
					NULL				// pointer to window-creation data
				);
 	if( NULL == g_hWndMain )
		exit(1);






	bool bSuccess = HELPER.Run( MainLoop );

	



	// clean up after a normal exit 
	DestroyObjects();			// deallocate our game objects and leave fullscreen
	ShowWindow( g_hWndMain, SW_HIDE );


	if( !bSuccess )
	{
		// throw up a pretty error dialog
		DialogBox(
			hInstance,
			MAKEINTRESOURCE(IDD_ERROR_DIALOG),
			NULL,
			ErrorWndProc
			);
	}

	DestroyWindow( g_hWndMain );
	UnregisterClass( g_sAppClassName, hInstance );
	CoUninitialize();			// Uninitialize COM
	CloseHandle( g_hMutex );

	return 0L;
}

void MainLoop()
{
	// Load keyboard accelerators
	HACCEL hAccel = LoadAccelerators( NULL, MAKEINTRESOURCE(IDR_MAIN_ACCEL) );

	// run the game
	CreateObjects( g_hWndMain );	// Create the game objects

	// Now we're ready to recieve and process Windows messages.
	MSG msg;
	ZeroMemory( &msg, sizeof(msg) );

	while( WM_QUIT != msg.message  )
	{
		// Look for messages, if none are found then 
		// update the state and display it
		if( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
		{
			GetMessage(&msg, NULL, 0, 0 );

			// Translate and dispatch the message
			if( 0 == TranslateAccelerator( g_hWndMain, hAccel, &msg ) )
			{
				TranslateMessage( &msg ); 
				DispatchMessage( &msg );
			}
		}
		else	// No messages are waiting.  Render a frame during idle time.
		{
			Update();
			Render();
			//if( !g_bFullscreen )
			//::Sleep(0 );	// give some time for the movie decoding thread
		}
	}	// end  while( WM_QUIT != msg.message  )

}


//-----------------------------------------------------------------------------
// Name: ErrorWndProc()
// Desc: Callback for all Windows messages
//-----------------------------------------------------------------------------
BOOL CALLBACK ErrorWndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
	case WM_INITDIALOG:
		{
			CString sMessage = HELPER.GetError() + "\n\nStack Trace:\n" + HELPER.GetErrorStackTrace();
			sMessage.Replace( "\n", "\r\n" );
			SendDlgItemMessage( 
				hWnd, 
				IDC_EDIT_ERROR, 
				WM_SETTEXT, 
				0, 
				(LPARAM)(LPCTSTR)sMessage
				);
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON_VIEW_LOG:
			{
				PROCESS_INFORMATION pi;
				STARTUPINFO	si;
				ZeroMemory( &si, sizeof(si) );

				CreateProcess(
					NULL,		// pointer to name of executable module
					"notepad.exe log.txt",		// pointer to command line string
					NULL,  // process security attributes
					NULL,   // thread security attributes
					false,  // handle inheritance flag
					0, // creation flags
					NULL,  // pointer to new environment block
					NULL,   // pointer to current directory name
					&si,  // pointer to STARTUPINFO
					&pi  // pointer to PROCESS_INFORMATION
				);
			}
			break;
		case IDC_BUTTON_REPORT:
			GotoURL( "Docs/report.htm" );
			break;
		case IDC_BUTTON_RESTART:
			{
				// Launch StepMania
				PROCESS_INFORMATION pi;
				STARTUPINFO	si;
				ZeroMemory( &si, sizeof(si) );

				CreateProcess(
					NULL,		// pointer to name of executable module
					"stepmania.exe",		// pointer to command line string
					NULL,  // process security attributes
					NULL,   // thread security attributes
					false,  // handle inheritance flag
					0, // creation flags
					NULL,  // pointer to new environment block
					NULL,   // pointer to current directory name
					&si,  // pointer to STARTUPINFO
					&pi  // pointer to PROCESS_INFORMATION
				);
			}
			EndDialog( hWnd, 0 );
			break;
			// fall through
		case IDOK:
			EndDialog( hWnd, 0 );
			break;
		}
	}
	return FALSE;
}


//-----------------------------------------------------------------------------
// Name: WndProc()
// Desc: Callback for all Windows messages
//-----------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
	switch( msg )
	{
		case WM_ACTIVATEAPP:
            // Check to see if we are losing our window...
			g_bIsActive = (BOOL)wParam;
			break;

		case WM_SIZE:
            // Check to see if we are losing our window...
			if( SIZE_MAXHIDE==wParam || SIZE_MINIMIZED==wParam )
                g_bIsActive = FALSE;
            else
                g_bIsActive = TRUE;
            break;

		case WM_GETMINMAXINFO:
			{
				// Don't allow the window to be resized smaller than the screen resolution.
				// This should snap to multiples of the screen size two!

				RECT rcWnd;
				SetRect( &rcWnd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
				DWORD dwWindowStyle = GetWindowLong( g_hWndMain, GWL_STYLE );
				AdjustWindowRect( &rcWnd, dwWindowStyle, FALSE );

				((MINMAXINFO*)lParam)->ptMinTrackSize.x = RECTWIDTH(rcWnd);
				((MINMAXINFO*)lParam)->ptMinTrackSize.y = RECTHEIGHT(rcWnd);
			}
			break;

		case WM_SETCURSOR:
			// Turn off Windows cursor in fullscreen mode
			if( SCREEN && !SCREEN->IsWindowed() )
            {
                SetCursor( NULL );
                return TRUE; // prevent Windows from setting the cursor
            }
            break;

		case WM_SYSCOMMAND:
			// Prevent moving/sizing and power loss
			switch( wParam )
			{
				case SC_MOVE:
				case SC_SIZE:
				case SC_KEYMENU:
				case SC_MONITORPOWER:
					return 1;
				case SC_MAXIMIZE:
					//SendMessage( g_hWndMain, WM_COMMAND, IDM_TOGGLEFULLSCREEN, 0 );
					//return 1;
					break;
			}
			break;


		case WM_COMMAND:
		{
			GraphicOptions &go = PREFS->m_GraphicOptions;
            switch( LOWORD(wParam) )
            {
				case IDM_TOGGLEFULLSCREEN:
					go.m_bWindowed = !go.m_bWindowed;
					ApplyGraphicOptions();
					return 0;
				case IDM_CHANGERESOLUTION:
					go.m_iResolution = (go.m_iResolution==640) ? 320 : 640;
					ApplyGraphicOptions();
					return 0;
               case IDM_EXIT:
                    // Recieved key/menu command to exit app
                    SendMessage( hWnd, WM_CLOSE, 0, 0 );
                    return 0;
            }
            break;
		}
        case WM_NCHITTEST:
            // Prevent the user from selecting the menu in fullscreen mode
            if( SCREEN && !SCREEN->IsWindowed() )
                return HTCLIENT;
            break;

		case WM_PAINT:
			// redisplay the contents of the back buffer if the window needs to be redrawn
			ShowFrame();
			break;

		case WM_DESTROY:
			PostQuitMessage( 0 );
			break;
	}

	return DefWindowProc( hWnd, msg, wParam, lParam );
}



//-----------------------------------------------------------------------------
// Name: CreateObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT CreateObjects( HWND hWnd )
{
	//
	// Draw a splash bitmap so the user isn't looking at a black screen
	//
	HBITMAP hSplashBitmap = (HBITMAP)LoadImage( 
		GetModuleHandle( NULL ),
		TEXT("BITMAP_SPLASH"), 
		IMAGE_BITMAP,
		0, 0, LR_CREATEDIBSECTION );
    BITMAP bmp;
    RECT rc;
    GetClientRect( hWnd, &rc );

    // Display the splash bitmap in the window
    HDC hDCWindow = GetDC( hWnd );
    HDC hDCImage  = CreateCompatibleDC( NULL );
    SelectObject( hDCImage, hSplashBitmap );
    GetObject( hSplashBitmap, sizeof(bmp), &bmp );
    StretchBlt( hDCWindow, 0, 0, rc.right, rc.bottom,
                hDCImage, 0, 0,
                bmp.bmWidth, bmp.bmHeight, SRCCOPY );
    DeleteDC( hDCImage );
    ReleaseDC( hWnd, hDCWindow );
	
	// Delete the bitmap
	DeleteObject( hSplashBitmap );



	//
	// Create game objects
	//
	srand( (unsigned)time(NULL) );	// seed number generator
	
	SOUND	= new RageSound( hWnd );
	MUSIC	= new RageSoundStream;
	INPUTM	= new RageInput( hWnd );
	PREFS	= new PrefsManager;
	SCREEN	= new RageScreen( hWnd );
	SONG	= new SongManager;		// this takes a long time to load
	GAME	= new GameManager;
	THEME	= new ThemeManager;

	BringWindowToTop( hWnd );
	SetForegroundWindow( hWnd );

	// We can't do any texture loading unless the D3D device is created.  
	// Set the display mode to make sure the D3D device is created.
	ApplyGraphicOptions(); 

	TEXTURE	= new RageTextureManager( SCREEN );

	// Ugly...  Set graphic options again so the TextureManager knows our preferences
	ApplyGraphicOptions(); 

	// These things depend on the TextureManager, so do them last!
	FONT	= new FontManager;
	WM		= new WindowManager;

	// Ugly...  Switch the screen resolution again so that the system message will display
	ApplyGraphicOptions(); 

	WM->SystemMessage( ssprintf("Found %d songs.", SONG->m_pSongs.GetSize()) );


	//WM->SetNewWindow( new WindowLoading );
	//WM->SetNewWindow( new WindowSandbox );
	//WM->SetNewWindow( new WindowMenuResults );
	//WM->SetNewWindow( new WindowPlayerOptions );
	WM->SetNewWindow( new WindowTitleMenu );


    DXUtil_Timer( TIMER_START );    // Start the accurate timer


	return S_OK;
}

//-----------------------------------------------------------------------------
// Name: DestroyObjects()
// Desc:
//-----------------------------------------------------------------------------
void DestroyObjects()
{
    DXUtil_Timer( TIMER_STOP );

	SAFE_DELETE( WM );
	SAFE_DELETE( PREFS );
	SAFE_DELETE( SONG );
	SAFE_DELETE( GAME );
	SAFE_DELETE( FONT );

	SAFE_DELETE( INPUTM );
	SAFE_DELETE( MUSIC );
	SAFE_DELETE( SOUND );
	SAFE_DELETE( TEXTURE );
	SAFE_DELETE( SCREEN );
}


//-----------------------------------------------------------------------------
// Name: RestoreObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT RestoreObjects()
{
	/////////////////////
	// Restore the window
	/////////////////////
	
    // Set window size
    RECT rcWnd;
	if( SCREEN->IsWindowed() )
	{
		SetRect( &rcWnd, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT );
  		AdjustWindowRect( &rcWnd, g_dwWindowStyle, FALSE );
	}
	else	// if fullscreen
	{
		SetRect( &rcWnd, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) );
	}

	// Bring the window to the foreground
    SetWindowPos( g_hWndMain, 
				  HWND_NOTOPMOST, 
				  0, 
				  0, 
				  RECTWIDTH(rcWnd), 
				  RECTHEIGHT(rcWnd),
                  0 );


	///////////////////////////
	// Restore all game objects
	///////////////////////////

	SCREEN->Restore();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: InvalidateObjects()
// Desc:
//-----------------------------------------------------------------------------
HRESULT InvalidateObjects()
{
	SCREEN->Invalidate();
	
	return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Update()
// Desc:
//-----------------------------------------------------------------------------
void Update()
{
	float fDeltaTime = DXUtil_Timer( TIMER_GETELAPSEDTIME );
	
	// This was a hack to fix timing issues with the old WindowSelectSong
	//
	if( fDeltaTime > 0.050f )	// we dropped a bunch of frames
		fDeltaTime = 0.050f;
	
	MUSIC->Update( fDeltaTime );

	WM->Update( fDeltaTime );


	static DeviceInputArray diArray;
	diArray.SetSize( 0, 10 );	// zero the array
	INPUTM->GetDeviceInputs( diArray );

	DeviceInput DeviceI;
	PadInput PadI;
	PlayerInput PlayerI;

	for( int i=0; i<diArray.GetSize(); i++ )
	{
		DeviceI = diArray[i];

		PREFS->DeviceToPad( DeviceI, PadI );
		PREFS->PadToPlayer( PadI, PlayerI );

		WM->Input( DeviceI, PadI, PlayerI );
	}

}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc:
//-----------------------------------------------------------------------------
void Render()
{
	HRESULT hr = SCREEN->BeginFrame();
	switch( hr )
	{
		case D3DERR_DEVICELOST:
			// The user probably alt-tabbed out of fullscreen.
			// Do not render a frame until we re-acquire the device
			break;
		case D3DERR_DEVICENOTRESET:
			InvalidateObjects();

            // Resize the device
            if( SUCCEEDED( hr = SCREEN->Reset() ) )
            {
                // Initialize the app's device-dependent objects
                RestoreObjects();
				return;
            }
			else
			{
				HELPER.FatalErrorHr( hr, "Failed to SCREEN->Reset()" );
			}

			break;
		case S_OK:
			{
				// set texture and alpha properties
				LPDIRECT3DDEVICE8 pd3dDevice = SCREEN->GetDevice();

				// calculate view and projection transforms
				D3DXMATRIX matProj;
				D3DXMatrixOrthoOffCenterLH( &matProj, 0, 640, 480, 0, -100, 100 );
				pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

				D3DXMATRIX matView;
				D3DXMatrixIdentity( &matView );
				pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

				SCREEN->ResetMatrixStack();

				// draw the game
				WM->Draw();


				SCREEN->EndFrame();
			}
			break;
	}

	ShowFrame();
}


//-----------------------------------------------------------------------------
// Name: ShowFrame()
// Desc:
//-----------------------------------------------------------------------------
void ShowFrame()
{
	// display the contents of the back buffer to the front
	if( SCREEN )
		SCREEN->ShowFrame();
}


//-----------------------------------------------------------------------------
// Name: ApplyGraphicOptions()
// Desc:
//-----------------------------------------------------------------------------
void ApplyGraphicOptions()
{
	InvalidateObjects();

	GraphicOptions &go = PREFS->m_GraphicOptions;

	//
	// Let the texture manager know about our preferences
	//
	if( TEXTURE != NULL )
	{
		TEXTURE->SetMaxTextureSize( go.m_iMaxTextureSize );
		TEXTURE->SetTextureColorDepth( go.m_iTextureColor );
	}

	//
	// use GameOptions to get the display settings
	//
	bool bWindowed	=	go.m_bWindowed;
	DWORD dwWidth	=	go.m_iResolution;
	DWORD dwHeight;
	// fill in dwHeight
	switch( dwWidth )
	{
	case 320:	dwHeight = 240;		break;
	case 400:	dwHeight = 300;		break;
	case 512:	dwHeight = 384;		break;
	case 640:	dwHeight = 480;		break;
	case 800:	dwHeight = 600;		break;
	case 1024:	dwHeight = 768;		break;
	case 1280:	dwHeight = 1024;	break;
	default:	dwHeight = 480;		break;
	}
	DWORD dwBPP		=	go.m_iDisplayColor;

	DWORD dwFlags = 0 | (go.m_b30fpsLock ? SCREEN_FLAG_CAP_AT_30HZ : 0);

	//
	// If the requested resolution doesn't work, keep switching until we find one that does.
	//
	if( !SCREEN->SwitchDisplayMode( bWindowed, dwWidth, dwHeight, dwBPP, dwFlags ) )
	{
		// We failed.  Try full screen with same params.
		bWindowed = false;
		if( !SCREEN->SwitchDisplayMode( bWindowed, dwWidth, dwHeight, dwBPP, dwFlags ) )
		{
			// Failed again.  Try 16 BPP
			dwBPP = 16;
			if( !SCREEN->SwitchDisplayMode( bWindowed, dwWidth, dwHeight, dwBPP, dwFlags ) )
			{
				// Failed again.  Try 640x480
				dwWidth = 640;
				dwHeight = 480;
				if( !SCREEN->SwitchDisplayMode( bWindowed, dwWidth, dwHeight, dwBPP, dwFlags ) )
				{
					// Failed again.  Try 320x240
					dwWidth = 320;
					dwHeight = 240;
					if( !SCREEN->SwitchDisplayMode( bWindowed, dwWidth, dwHeight, dwBPP, dwFlags ) )
					{
						HELPER.FatalError( "Tried every possible display mode, and couldn't change to any of them." );
					}
				}
			}
		}
	}


	RestoreObjects();

    go.m_bWindowed = bWindowed;
    go.m_iDisplayColor = dwBPP;
    go.m_iResolution = dwWidth;
	PREFS->SavePrefsToDisk();

	if( WM )
	{
		WM->SystemMessage( ssprintf("%s - %ux%u - %u bits", bWindowed ? "Windowed" : "FullScreen", dwWidth, dwHeight, dwBPP) );
	}
}





