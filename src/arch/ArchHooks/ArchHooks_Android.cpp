#include "global.h"
#include "ArchHooks_Android.h"
#include "ProductInfo.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageThreads.h"
#include "LocalizedString.h"
#include "SpecialFiles.h"
//#include "archutils/Unix/SignalHandler.h"
#include "archutils/Android/Globals.h"
#include "archutils/Unix/GetSysInfo.h"
#include "archutils/Common/PthreadHelpers.h"
#include "archutils/Unix/EmergencyShutdown.h"
#include "archutils/Unix/AssertionHandler.h"
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#if defined(HAVE_FFMPEG)
extern "C"
{
	#include <libavcodec/avcodec.h>
}
#endif

static bool IsFatalSignal( int signal )
{
	switch( signal )
	{
	case SIGINT:
	case SIGTERM:
	case SIGHUP:
		return false;
	default:
		return true;
	}
}

// TODO: Android Clean Exit
/*static bool DoCleanShutdown( int signal, siginfo_t *si, const ucontext_t *uc )
{
	if( IsFatalSignal(signal) )
		return false;

	/* ^C. * /
	ArchHooks::SetUserQuit();
	return true;
}
*/

// TODO: ANR
/*static bool EmergencyShutdown( int signal, siginfo_t *si, const ucontext_t *uc )
{
	if( !IsFatalSignal(signal) )
		return false;

	DoEmergencyShutdown();
    RString crash = ("" + signal);
    AndroidGlobals::Crash::ForceCrash("Emergency Shutdown:"+crash.c_str());

	/* We didn't run the crash handler.  Run the default handler, so we can dump core. * /
	return false;
}*/

	
#if defined(HAVE_TLS)
static thread_local int g_iTestTLS = 0;

static int TestTLSThread( void *p )
{
	g_iTestTLS = 2;
	return 0;
}

static void TestTLS()
{
#if defined(LINUX)
	/* TLS won't work on older threads libraries, and may crash. */
	if( !UsingNPTL() )
		return;
#endif
	/* TLS won't work on older Linux kernels.  Do a simple check. */
	g_iTestTLS = 1;

	RageThread TestThread;
	TestThread.SetName( "TestTLS" );
	TestThread.Create( TestTLSThread, NULL );
	TestThread.Wait();

	if( g_iTestTLS == 1 )
		RageThread::SetSupportsTLS( true );
}
#endif

#if 1
/* If librt is available, use CLOCK_MONOTONIC to implement GetMicrosecondsSinceStart,
 * if supported, so changes to the system clock don't cause problems. */
namespace
{
	clockid_t g_Clock = CLOCK_REALTIME;
 
	void OpenGetTime()
	{
		static bool bInitialized = false;
		if( bInitialized )
			return;
		bInitialized = true;
 
		/* Check whether the clock is actually supported. */
		timespec ts;
		if( clock_getres(CLOCK_MONOTONIC, &ts) == -1 )
			return;

		/* If the resolution is worse than a millisecond, fall back on CLOCK_REALTIME. */
		if( ts.tv_sec > 0 || ts.tv_nsec > 1000000 )
			return;
		
		g_Clock = CLOCK_MONOTONIC;
	}
};

clockid_t ArchHooks_Android::GetClock()
{
	OpenGetTime();
	return g_Clock;
}

int64_t ArchHooks::GetMicrosecondsSinceStart( bool bAccurate )
{
	OpenGetTime();

	timespec ts;
	clock_gettime( g_Clock, &ts );

	int64_t iRet = int64_t(ts.tv_sec) * 1000000 + int64_t(ts.tv_nsec)/1000;
	if( g_Clock != CLOCK_MONOTONIC )
		iRet = ArchHooks::FixupTimeIfBackwards( iRet );
	return iRet;
}
#else
int64_t ArchHooks::GetMicrosecondsSinceStart( bool bAccurate )
{
	struct timeval tv;
	gettimeofday( &tv, NULL );

	int64_t iRet = int64_t(tv.tv_sec) * 1000000 + int64_t(tv.tv_usec);
	ret = FixupTimeIfBackwards( ret );
	return iRet;
}
#endif

RString ArchHooks::GetPreferredLanguage()
{
	RString locale;

	if(getenv("LANG"))
	{
		locale = getenv("LANG");
		locale = locale.substr(0,2);
	}
	else
		locale = "en";

	return locale;
}

void ArchHooks_Android::Init()
{
    // TODO: Revisit this, as we need to properly handle the events.

	/* First, handle non-fatal termination signals. */
	// SignalHandler::OnClose( DoCleanShutdown );

	/* Set up EmergencyShutdown, to try to shut down the window if we crash.
	 * This might blow up, so be sure to do it after the crash handler. */
	// SignalHandler::OnClose( EmergencyShutdown );

	//InstallExceptionHandler();
	
#if defined(HAVE_TLS) && !defined(BSD)
	TestTLS();
#endif
}

// TODO: Intent the URL.
bool ArchHooks_Android::GoToURL( RString sUrl )
{
    return false;
}

#ifndef _CS_GNU_LIBC_VERSION
#define _CS_GNU_LIBC_VERSION 2
#endif

static RString LibcVersion()
{
    // Android uses Bionic.
	return "(unknown)";
}

void ArchHooks_Android::DumpDebugInfo()
{
	RString sys;
	int vers;
	GetKernel( sys, vers );
	LOG->Info( "OS: %s ver %06i", sys.c_str(), vers );

	LOG->Info( "Runtime library: %s", LibcVersion().c_str() );
	LOG->Info( "Threads library: %s", ThreadsVersion().c_str() );
#if defined(HAVE_FFMPEG)
	LOG->Info( "libavcodec: %#x (%u)", avcodec_version(), avcodec_version() );
#endif
}

void ArchHooks_Android::SetTime( tm newtime )
{
	RString sCommand = ssprintf( "date %02d%02d%02d%02d%04d.%02d",
		newtime.tm_mon+1,
		newtime.tm_mday,
		newtime.tm_hour,
		newtime.tm_min,
		newtime.tm_year+1900,
		newtime.tm_sec );

	LOG->Trace( "executing '%s'", sCommand.c_str() ); 
	int ret = system( sCommand );
	if( ret == -1 || ret == 127 || !WIFEXITED(ret) || WEXITSTATUS(ret) )
		LOG->Trace( "'%s' failed", sCommand.c_str() );

	ret = system( "hwclock --systohc" );
	if( ret == -1 || ret == 127 || !WIFEXITED(ret) || WEXITSTATUS(ret) )
		LOG->Trace( "'hwclock --systohc' failed" );
}

#include "RageFileManager.h"
#include <sys/stat.h>

static LocalizedString COULDNT_FIND_SONGS( "ArchHooks_Android", "Couldn't find 'Songs'" );
void ArchHooks::MountInitialFilesystems( const RString &sDirOfExecutable )
{
	RString Root;
	struct stat st;
	if( !stat(sDirOfExecutable + "/Packages", &st) && st.st_mode&S_IFDIR )
		Root = sDirOfExecutable;
	else if( !stat(sDirOfExecutable + "/Songs", &st) && st.st_mode&S_IFDIR )
		Root = sDirOfExecutable;
	else if( !stat(RageFileManagerUtil::sInitialWorkingDirectory + "/Songs", &st) && st.st_mode&S_IFDIR )
		Root = RageFileManagerUtil::sInitialWorkingDirectory;
	else
		RageException::Throw( "%s :: Execution Context: %s", COULDNT_FIND_SONGS.GetValue().c_str(), sDirOfExecutable );

	FILEMAN->Mount( "dir", Root, "/" );
}

void ArchHooks::MountUserFilesystems( const RString &sDirOfExecutable )
{
	/* Path to write general mutable user data when not Portable
	 * Lowercase the PRODUCT_ID; dotfiles and directories are almost always lowercase.
	 */
	const char *szHome = getenv( "HOME" );
	RString sUserDataPath = ssprintf( "%s/.%s", szHome? szHome:".", "stepmania-5.0" ); //call an ambulance!
	FILEMAN->Mount( "dir", sUserDataPath + "/Announcers", "/Announcers" );
	FILEMAN->Mount( "dir", sUserDataPath + "/BGAnimations", "/BGAnimations" );
	FILEMAN->Mount( "dir", sUserDataPath + "/BackgroundEffects", "/BackgroundEffects" );
	FILEMAN->Mount( "dir", sUserDataPath + "/BackgroundTransitions", "/BackgroundTransitions" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Cache", "/Cache" );
	FILEMAN->Mount( "dir", sUserDataPath + "/CDTitles", "/CDTitles" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Characters", "/Characters" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Courses", "/Courses" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Logs", "/Logs" );
	FILEMAN->Mount( "dir", sUserDataPath + "/NoteSkins", "/NoteSkins" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Packages", "/" + SpecialFiles::USER_PACKAGES_DIR );
	FILEMAN->Mount( "dir", sUserDataPath + "/Save", "/Save" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Screenshots", "/Screenshots" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Songs", "/Songs" );
	FILEMAN->Mount( "dir", sUserDataPath + "/RandomMovies", "/RandomMovies" );
	FILEMAN->Mount( "dir", sUserDataPath + "/Themes", "/Themes" );
}

/*
 * (c) 2003-2004 Glenn Maynard
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */