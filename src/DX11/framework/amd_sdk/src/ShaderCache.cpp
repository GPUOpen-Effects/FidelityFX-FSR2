//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

//--------------------------------------------------------------------------------------
// File: ShaderCache.cpp
//
// Class implementation for the ShaderCache interface. Allows the user to add shaders to a list
// which is then compiled in parallel to object files. Future calls to create the shaders,
// will simply re-use the object files, making creation time very fast. The option is there,
// to force the regeneration of object files.
//
// Assumption, relies on following directory structure:
//
// SolutionDir\..\src\Shaders
//--------------------------------------------------------------------------------------

// DXUT helper code
#include "DXUT.h"
#include "SDKmisc.h"

#include "ShaderCache.h"

#include <process.h>
#include <Shlwapi.h>
#include <algorithm>

#include <string>

#pragma warning( disable : 4100 ) // disable unreference formal parameter warnings for /W4 builds
#pragma warning( disable : 4127 ) // disable conditional expression is constant warnings for /W4 builds
#pragma warning( disable : 4456 ) // disable declaration hides previous local declaration for /W4 builds

using namespace AMD;

// The done event handle
static HANDLE   s_hDoneEvent = 0;

static const wchar_t *FXC_PATH_STRING_LOCAL = L"\\src\\Shaders\\fxc.exe";
static const wchar_t *DEV_PATH_STRING_LOCAL = L"\\src\\Shaders\\Dev.exe";
static const wchar_t *FXC_PATH_STRING_INSTALLED_WIN_10_SDK = L"\\Windows Kits\\10\\bin\\x64\\fxc.exe";
static const wchar_t *FXC_PATH_STRING_INSTALLED_WIN_8_1_SDK = L"\\Windows Kits\\8.1\\bin\\x64\\fxc.exe";
static const wchar_t *FXC_PATH_STRING_INSTALLED_WIN_8_0_SDK = L"\\Windows Kits\\8.0\\bin\\x64\\fxc.exe";
static const wchar_t *DEV_PATH_STRING_INSTALLED = L"\\Dev.exe";

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
ShaderCache::Shader::Shader()
{
    m_eShaderType = SHADER_TYPE_UNKNOWN;
    m_ppShader = NULL;
    m_ppInputLayout = NULL;
    m_pInputLayoutDesc = NULL;
    m_uNumDescElements = 0;

    m_bShaderUpToDate = false;
    m_bGPRsUpToDate = false;

#if AMD_SDK_INTERNAL_BUILD
    m_eISATarget = DEFAULT_ISA_TARGET;
    m_ISA_VGPRs = m_previous_ISA_VGPRs = 0;
    m_ISA_SGPRs = m_previous_ISA_SGPRs = 0;
    m_ISA_GPRPoolSize = m_previous_ISA_GPRPoolSize = 0;
    m_ISA_ALUPacking = m_previous_ISA_ALUPacking = 0.0;
#endif

    memset( m_wsTarget, '\0', sizeof( wchar_t[m_uTARGET_MAX_LENGTH] ) );
    memset( m_wsEntryPoint, '\0', sizeof( wchar_t[m_uENTRY_POINT_MAX_LENGTH] ) );
    memset( m_wsSourceFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsCanonicalName, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );

    m_uNumMacros = 0;
    m_pMacros = NULL;

    memset( m_wsRawFileName, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsHashedFileName, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsObjectFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsErrorFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsAssemblyFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsAssemblyFileWithHashedFilename, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsISAFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsPreprocessFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsHashFile, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsCommandLine, '\0', sizeof( wchar_t[m_uCOMMAND_LINE_MAX_LENGTH] ) );
    memset( m_wsISACommandLine, '\0', sizeof( wchar_t[m_uCOMMAND_LINE_MAX_LENGTH] ) );
    memset( m_wsPreprocessCommandLine, '\0', sizeof( wchar_t[m_uCOMMAND_LINE_MAX_LENGTH] ) );

    memset( m_wsObjectFile_with_ISA, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );
    memset( m_wsPreprocessFile_with_ISA, '\0', sizeof( wchar_t[m_uFILENAME_MAX_LENGTH] ) );

    // Test that we can use paths > 255 characters with unicode file handling via \\?\ syntax
    // Each section of the path needs to be <= 260 characters.
    // Must first create path:
    // C:\xpSnm8ixu8zR3lqGRtIbXMNOVE15oRgP7SwhsqRgkG7atxEOZbgsJmWXDLA7pCSUuQsqko9YN03e7aUw26Aoy9UpS3mSH1uekbTquhozgbB5KQwFxKzaBkSLpVmIWX8gQpoDd9mCTFLtbO0BCGdF7lF48KpxPFTjmNVgJK4sMfi5mavXhc

    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH + 1] =
    {
        L"\\\\?\\C:\\xpSnm8ixu8zR3lqGRtIbXMNOVE15oRgP7SwhsqRgkG7atxEOZbgs"
        L"JmWXDLA7pCSUuQsqko9YN03e7aUw26Aoy9UpS3mSH1uekbTquhozgbB5KQwFxKz"
        L"aBkSLpVmIWX8gQpoDd9mCTFLtbO0BCGdF7lF48KpxPFTjmNVgJK4sMfi5mavXhc"
        L"\\rqgJRzfIBHlFGEcQrb3JUPcFGEt2S0ZnhetHvO2WNerlSOvqDVbBIey9bO7k0"
        L"tytUih5E8ijWnbPHxF65X0jCuwikbtjkOXgoMBvoNGDlpfcrS0PVS7Ww5SJ3Ivg"
        L"KdMj5VyN7qmfWkddiLoTvOybtkSchYW99uyQEgIkiPjM0mFmlCGIsTPFRDC.txt"
        L"\0"
    };

    _wfopen_s( &pFile, wsShaderPathName, L"wb" );

    if (pFile)
    {
        fwrite( "Hello, World!\0", 14, 1, pFile );
        fclose( pFile );
    }

    //DebugBreak();


    m_bBeingProcessed = false;
    m_hCompileProcessHandle = NULL;
    m_hCompileThreadHandle = NULL;
    m_iCompileWaitCount = -1;

    m_pHash = NULL;
    m_uHashLength = 0;

    m_pFilenameHash = NULL;
    m_uFilenameHashLength = 0;

}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
ShaderCache::Shader::~Shader()
{
    if (NULL != m_pMacros)
    {
        delete [] m_pMacros;
        m_pMacros = NULL;
    }

    if (NULL != m_pHash)
    {
        free( m_pHash );
        m_pHash = NULL;
    }

    if (NULL != m_pFilenameHash)
    {
        free( m_pFilenameHash );
        m_pFilenameHash = NULL;
    }

    for (int iElement = 0; iElement < (int)m_uNumDescElements; iElement++)
    {
        delete [] m_pInputLayoutDesc[iElement].SemanticName;
    }
    delete [] m_pInputLayoutDesc;
    m_pInputLayoutDesc = NULL;
}

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
ShaderCache::ShaderCache( const SHADER_AUTO_RECOMPILE_TYPE i_keAutoRecompileTouchedShadersType, const ERROR_DISPLAY_TYPE i_keErrorDisplayType,
    const GENERATE_ISA_TYPE i_keGenerateShaderISAType, const SHADER_COMPILER_EXE_TYPE i_keShaderCompilerExeType )
{
    m_ShaderSourceList.clear();
    m_ShaderList.clear();
    m_PreprocessList.clear();
    m_HashList.clear();
    m_CompileList.clear();
    m_CompileCheckList.clear();
    m_CreateList.clear();
    m_ErrorList.clear();

#if AMD_SDK_INTERNAL_BUILD
    m_ISATargetList.clear();
    m_ISATargetList.reserve( NUM_ISA_TARGETS );
    for (int i = 0; i < NUM_ISA_TARGETS; i++)
    {
        m_ISATargetList.push_back( new std::vector<Shader*>() );
    }
#endif

    BOOL bRet;

    InitializeCriticalSection( &m_CompileShaders_CriticalSection );
    InitializeCriticalSection( &m_GenISA_CriticalSection );

    // the working dir we want for ShaderCache is not necessarily the current directory,
    // so get the current directory and then specify our working dir relative to it
    wchar_t wsWorkingDir[m_uPATHNAME_MAX_LENGTH];
    GetCurrentDirectoryW( m_uPATHNAME_MAX_LENGTH, wsWorkingDir );

    // the safer PathCchCombine/PathCchCombineEx functions are not available in Win7,
    // so just use PathCombine
    PathCombine( m_wsWorkingDir, wsWorkingDir, L"..\\bin" );
    swprintf_s( m_wsUnicodeWorkingDir, L"%s%s", L"\\\\?\\", m_wsWorkingDir );

    PathCombine( m_wsShaderSourceDir, m_wsWorkingDir, L"..\\src\\Shaders" );
    swprintf_s( m_wsUnicodeShaderSourceDir, L"%s%s", L"\\\\?\\", m_wsShaderSourceDir );

    PathCombine( m_wsAmdSdkDir, m_wsWorkingDir, L"..\\amd_sdk" );

    swprintf_s( m_wsBatchWorkingDir, L"%s", m_wsUnicodeWorkingDir );

    wchar_t wsShadersDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsShadersDir, L"%s%s", m_wsWorkingDir, L"\\Shaders" );
    bRet = CreateDirectoryW( wsShadersDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

#if AMD_SDK_INTERNAL_BUILD
    swprintf_s( m_wsSCDEVWorkingDir, L"%s%s", wsShadersDir, L"\\ScDev" );
    bRet = CreateDirectoryW( m_wsSCDEVWorkingDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }
#endif

    wchar_t wsCacheDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsCacheDir, L"%s%s", wsShadersDir, L"\\Cache" );
    bRet = CreateDirectoryW( wsCacheDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsObjectDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsObjectDir, L"%s%s", wsCacheDir, L"\\Object" );
    bRet = CreateDirectoryW( wsObjectDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsObjectDebugDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsObjectDebugDir, L"%s%s", wsCacheDir, L"\\Object\\Debug" );
    bRet = CreateDirectoryW( wsObjectDebugDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsObjectReleaseDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsObjectReleaseDir, L"%s%s", wsCacheDir, L"\\Object\\Release" );
    bRet = CreateDirectoryW( wsObjectReleaseDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsErrorDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsErrorDir, L"%s%s", wsCacheDir, L"\\Error" );
    bRet = CreateDirectoryW( wsErrorDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsAssemblyDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsAssemblyDir, L"%s%s", wsCacheDir, L"\\Assembly" );
    bRet = CreateDirectoryW( wsAssemblyDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsISADir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsISADir, L"%s%s", wsCacheDir, L"\\ISA" );
    bRet = CreateDirectoryW( wsISADir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsPreprocessDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsPreprocessDir, L"%s%s", wsCacheDir, L"\\Preprocess" );
    bRet = CreateDirectoryW( wsPreprocessDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsHashDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsHashDir, L"%s%s", wsCacheDir, L"\\Hash" );
    bRet = CreateDirectoryW( wsHashDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsHashDebugDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsHashDebugDir, L"%s%s", wsCacheDir, L"\\Hash\\Debug" );
    bRet = CreateDirectoryW( wsHashDebugDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    wchar_t wsHashReleaseDir[m_uPATHNAME_MAX_LENGTH];
    swprintf_s( wsHashReleaseDir, L"%s%s", wsCacheDir, L"\\Hash\\Release" );
    bRet = CreateDirectoryW( wsHashReleaseDir, NULL );
    if (bRet == ERROR_PATH_NOT_FOUND)
    {
        assert( false );
    }

    SYSTEM_INFO sysinfo;
    GetSystemInfo( &sysinfo );
    m_uNumCPUCores = sysinfo.dwNumberOfProcessors;
    SetMaximumCoresForShaderCompiler( MAXCORES_USE_ALL_BUT_ONE ); // Fast in Debug, no hitching
    //SetMaximumCoresForShaderCompiler( MAXCORES_USE_ALL_CORES ); // Slightly faster in Release, barely any hitching
    //SetMaximumCoresForShaderCompiler( MAXCORES_2X_CPU_CORES ); // Can be faster in Release, some hitching
    //SetMaximumCoresForShaderCompiler( MAXCORES_NO_LIMIT ); // Can be faster in Release, a lot of hitching

    //SetMaximumCoresForShaderCompiler( MAXCORES_SINGLE_THREADED ); // Fast in Debug, no hitching

    s_hDoneEvent = CreateEvent( NULL, TRUE, FALSE, NULL );
    SetEvent( s_hDoneEvent );

    m_CreateType = CREATE_TYPE_USE_CACHED;

    m_bShadersCreated = false;
    m_bAbort = false;
    m_bPrintedProgress = false;

    m_pProgressInfo = NULL;
    m_uProgressCounter = 0;

    m_bForceDebugShaders = false;

    m_watchHandle = NULL;
    m_waitPoolHandle = NULL;

#if AMD_SDK_INTERNAL_BUILD
    m_eTargetISA = DEFAULT_ISA_TARGET;
#endif

    m_bCreateHashDigest = true;
#if !AMD_SDK_PREBUILT_RELEASE_EXE
    m_bRecompileTouchedShaders = (i_keAutoRecompileTouchedShadersType == SHADER_AUTO_RECOMPILE_ENABLED);
    m_ErrorDisplayType = i_keErrorDisplayType;
#else
    // the pre-built release executable ignores the passed-in construction parameters and just uses default settings
    m_bRecompileTouchedShaders = false;
    m_ErrorDisplayType = ERROR_DISPLAY_IN_DEBUG_OUTPUT_AND_BREAK;
#endif
    m_bShowShaderErrors = (i_keErrorDisplayType == ERROR_DISPLAY_ON_SCREEN);
#if AMD_SDK_INTERNAL_BUILD
#if AMD_SDK_PREBUILT_RELEASE_EXE
#error The pre-built release exe exists specifically to release (i.e. it's external)
#endif
    m_bGenerateShaderISA = (i_keGenerateShaderISAType == GENERATE_ISA_ENABLED);
#else
    m_bGenerateShaderISA = false;
#endif

    m_bHasShaderErrorsToDisplay = false;
    m_shaderErrorRenderedCount = 0;

    swprintf_s( m_wsLastShaderError, L"*** 0 Shader Errors ***\n" );

#if AMD_SDK_INTERNAL_BUILD
    if (i_keShaderCompilerExeType == SHADER_COMPILER_EXE_LOCAL)
    {
        swprintf_s( m_wsFxcExePath, L"%s%s", m_wsAmdSdkDir, FXC_PATH_STRING_LOCAL );
        swprintf_s( m_wsDevExePath, L"%s%s", m_wsAmdSdkDir, DEV_PATH_STRING_LOCAL );
    }
    else
#endif
    {
        // search for fxc in the win8.1 sdk. If not found, then try the 8.0 sdk.
        wchar_t wsProgramFilesDir[MAX_PATH];
        SHGetSpecialFolderPath(
            0,
            wsProgramFilesDir,
            CSIDL_PROGRAM_FILESX86,
            FALSE );
        swprintf_s( m_wsFxcExePath, L"%s%s", wsProgramFilesDir, FXC_PATH_STRING_INSTALLED_WIN_10_SDK );
        if (!CheckFXC())
        {
            swprintf_s( m_wsFxcExePath, L"%s%s", wsProgramFilesDir, FXC_PATH_STRING_INSTALLED_WIN_8_1_SDK );
            if (!CheckFXC())
            {
                swprintf_s( m_wsFxcExePath, L"%s%s", wsProgramFilesDir, FXC_PATH_STRING_INSTALLED_WIN_8_0_SDK );
            }
        }

        // get the scdev environment variable
        size_t requiredSizeForScDevEnvVar;
        wchar_t wsScDevDir[MAX_PATH] = { L'\0' };
        _wgetenv_s( &requiredSizeForScDevEnvVar, wsScDevDir, MAX_PATH, L"AMD_SCDEV_DIR" );
        swprintf_s( m_wsDevExePath, L"%s%s", wsScDevDir, DEV_PATH_STRING_INSTALLED );
    }

    if (m_bGenerateShaderISA)
    {
        if (!CheckSCDEV())
        {
            m_bGenerateShaderISA = false;

#if !AMD_SDK_PREBUILT_RELEASE_EXE
            // For SHADER_COMPILER_EXE_INSTALLED, Dev.exe is assumed to be in location specified in AMD_SCDEV_DIR environment variable
            // For SHADER_COMPILER_EXE_LOCAL, Dev.exe is assumed to be in AMD_SDK\src\Shaders
            DebugBreak();
#endif
        }
    }

    if (!CheckFXC())
    {
#if !AMD_SDK_PREBUILT_RELEASE_EXE
        // For SHADER_COMPILER_EXE_INSTALLED, fxc.exe is assumed to be in the default Win8 SDK install location
        // For SHADER_COMPILER_EXE_LOCAL, fxc.exe is assumed to be in AMD_SDK\src\Shaders
        DebugBreak();
#endif
    }

    m_bShowShaderISA = m_bGenerateShaderISA;

    if (m_bRecompileTouchedShaders)
    {
#if defined(DEBUG) || defined(_DEBUG)
        const bool watchOK = WatchDirectoryForChanges();
        assert( watchOK );
#else
        WatchDirectoryForChanges();
#endif
    }

}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
ShaderCache::~ShaderCache()
{
    WaitForSingleObject( s_hDoneEvent, INFINITE );
    CloseHandle( s_hDoneEvent );

    for (std::list<Shader*>::iterator it = m_ShaderSourceList.begin(); it != m_ShaderSourceList.end(); it++)
    {
        Shader* pShader = *it;
        delete pShader;
    }

    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;
        delete pShader;
    }

#if AMD_SDK_INTERNAL_BUILD
    for (std::vector< std::vector<Shader*> * >::iterator it = m_ISATargetList.begin(); it != m_ISATargetList.end(); it++)
    {
        std::vector<Shader*> * pVector = *it;
        pVector->clear();
        delete pVector;
    }
#endif

    m_ShaderSourceList.clear();
    m_ShaderList.clear();
    m_PreprocessList.clear();
    m_HashList.clear();
    m_CompileList.clear();
    m_CompileCheckList.clear();
    m_CreateList.clear();
    m_ErrorList.clear();

#if AMD_SDK_INTERNAL_BUILD
    m_ISATargetList.clear();
#endif

    if (NULL != m_pProgressInfo)
    {
        delete [] m_pProgressInfo;
        m_pProgressInfo = NULL;
    }

    if (m_waitPoolHandle)
    {
        UnregisterWaitEx( m_waitPoolHandle, INVALID_HANDLE_VALUE );
        m_waitPoolHandle = NULL;
    }

    if (m_watchHandle)
    {
        FindCloseChangeNotification( m_watchHandle );
        m_watchHandle = NULL;
    }

    DeleteCriticalSection( &m_GenISA_CriticalSection );
    DeleteCriticalSection( &m_CompileShaders_CriticalSection );

    m_bRecompileTouchedShaders = false;
}


//--------------------------------------------------------------------------------------
// DXUT framework hook method (flags shaders as needing creation)
//--------------------------------------------------------------------------------------
void ShaderCache::OnDestroyDevice()
{
    m_bShadersCreated = false;
    InvalidateShaders();
}


//--------------------------------------------------------------------------------------
// Called by app when WM_QUIT is posted, so that shader generation can be aborted
//--------------------------------------------------------------------------------------
void ShaderCache::Abort()
{
    m_bAbort = true;
}

//--------------------------------------------------------------------------------------
// Allows the ShaderCache to add a new type of ISA Target version of all shaders to the cache
//--------------------------------------------------------------------------------------
bool ShaderCache::CloneShaders( void )
{
    bool bRVal = true;

#if AMD_SDK_INTERNAL_BUILD
    for (std::list<Shader*>::iterator it = m_ShaderSourceList.begin(); it != m_ShaderSourceList.end(); it++)
    {
        Shader* pShaderSource = *it;

        // check if you already have a shader for for this ISA Target
        if (std::find( m_ISATargetList[m_eTargetISA]->begin(), m_ISATargetList[m_eTargetISA]->end(), pShaderSource ) == m_ISATargetList[m_eTargetISA]->end())
        {
            // if not, make one
            bRVal &= AddShader( NULL,
                pShaderSource->m_eShaderType,
                pShaderSource->m_wsTarget,
                pShaderSource->m_wsEntryPoint,
                pShaderSource->m_wsSourceFile,
                pShaderSource->m_uNumMacros,
                pShaderSource->m_pMacros,
                pShaderSource->m_ppInputLayout,
                pShaderSource->m_pInputLayoutDesc,
                pShaderSource->m_uNumDescElements,
                pShaderSource->m_wsCanonicalName,
                pShaderSource->m_ISA_VGPRs,
                pShaderSource->m_ISA_SGPRs,
                false
                );

            // and mark in the list that you now have one for this ISA Target
            m_ISATargetList[m_eTargetISA]->push_back( pShaderSource );
        }
    }
#endif

    return bRVal;
}

void ShaderCache::InsertOutputFilenameIntoCommandLine( wchar_t *pwsCommandLine, const wchar_t* pwsFileName ) const
{
    // enclose path in quotes to handle spaces in path
    {
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\"" );
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, m_wsUnicodeWorkingDir );
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\\" );
    }

    wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pwsFileName );

    // enclose path in quotes to handle spaces in path
    {
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\"" );
    }
}

void ShaderCache::InsertInputFilenameIntoCommandLine( wchar_t *pwsCommandLine, const wchar_t* pwsFileName ) const
{
    // enclose path in quotes to handle spaces in path
    {
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\"" );
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, m_wsUnicodeShaderSourceDir );
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\\" );
    }

    wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pwsFileName );

    // enclose path in quotes to handle spaces in path
    {
        wcscat_s( pwsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\"" );
    }
}

//--------------------------------------------------------------------------------------
// User adds a shader to the cache
//--------------------------------------------------------------------------------------
bool ShaderCache::AddShader( ID3D11DeviceChild** ppShader,
    SHADER_TYPE ShaderType,
    const wchar_t* pwsTarget,
    const wchar_t* pwsEntryPoint,
    const wchar_t* pwsSourceFile,
    unsigned int uNumMacros,
    Macro* pMacros,
    ID3D11InputLayout** ppInputLayout,
    const D3D11_INPUT_ELEMENT_DESC* pInputLayoutDesc,
    unsigned int uNumDescElements,
    const wchar_t* pwsCanonicalName,
    const int i_iMaxVGPR,
    const int i_iMaxSGPR,
    const bool i_kbIsApplicationShader
    )
{
#if AMD_SDK_INTERNAL_BUILD
    assert( (NULL != ppShader) || (!i_kbIsApplicationShader) );
#else
    assert( NULL != ppShader && i_kbIsApplicationShader);
#endif
    assert( (ShaderType >= SHADER_TYPE_VERTEX) && (ShaderType <= SHADER_TYPE_COMPUTE) );
    assert( (NULL != pwsTarget) && (wcslen( pwsTarget ) <= m_uTARGET_MAX_LENGTH) );
    assert( (NULL != pwsEntryPoint) && (wcslen( pwsEntryPoint ) <= m_uENTRY_POINT_MAX_LENGTH) );
    assert( (NULL != pwsSourceFile) && (wcslen( pwsSourceFile ) <= m_uFILENAME_MAX_LENGTH) );
    if (uNumMacros > 0)
    {
        assert( NULL != pMacros );
    }

    if (i_kbIsApplicationShader)
    {
        Shader* pShaderSource = new Shader();
        pShaderSource->m_eShaderType = ShaderType;
        wcscpy_s( pShaderSource->m_wsTarget, m_uTARGET_MAX_LENGTH, pwsTarget );
        wcscpy_s( pShaderSource->m_wsEntryPoint, m_uENTRY_POINT_MAX_LENGTH, pwsEntryPoint );
        wcscpy_s( pShaderSource->m_wsSourceFile, m_uFILENAME_MAX_LENGTH, pwsSourceFile );
        pShaderSource->m_uNumMacros = uNumMacros;
        pShaderSource->m_uNumDescElements = uNumDescElements;
        pShaderSource->m_ppInputLayout = ppInputLayout;
        if (NULL != pwsCanonicalName)
        {
            wcscpy_s( pShaderSource->m_wsCanonicalName, m_uFILENAME_MAX_LENGTH, pwsCanonicalName );
        }
#if AMD_SDK_INTERNAL_BUILD
        pShaderSource->m_ISA_VGPRs = i_iMaxVGPR;
        pShaderSource->m_ISA_SGPRs = i_iMaxSGPR;
#endif

        if (pShaderSource->m_uNumMacros > 0)
        {
            pShaderSource->m_pMacros = new Macro[pShaderSource->m_uNumMacros];
            memcpy( pShaderSource->m_pMacros, pMacros, sizeof( Macro ) * pShaderSource->m_uNumMacros );
        }

        if (pShaderSource->m_eShaderType == SHADER_TYPE_VERTEX)
        {
            if (pShaderSource->m_uNumDescElements > 0)
            {
                assert( NULL != ppInputLayout );
                assert( NULL != pInputLayoutDesc );

                pShaderSource->m_pInputLayoutDesc = new D3D11_INPUT_ELEMENT_DESC[pShaderSource->m_uNumDescElements];

                memcpy( pShaderSource->m_pInputLayoutDesc, pInputLayoutDesc, sizeof( D3D11_INPUT_ELEMENT_DESC ) * pShaderSource->m_uNumDescElements );

                for (int iElement = 0; iElement < (int)pShaderSource->m_uNumDescElements; iElement++)
                {
                    pShaderSource->m_pInputLayoutDesc[iElement].SemanticName = new char[m_uFILENAME_MAX_LENGTH];
                    strcpy_s( (char*)pShaderSource->m_pInputLayoutDesc[iElement].SemanticName, m_uFILENAME_MAX_LENGTH, (char*)pInputLayoutDesc[iElement].SemanticName );
                }
            }
        }

        m_ShaderSourceList.push_back( pShaderSource );
#if AMD_SDK_INTERNAL_BUILD
        m_ISATargetList[m_eTargetISA]->push_back( pShaderSource );
#endif
    }

    Shader* pShader = new Shader();
    if (i_kbIsApplicationShader)
    { // Only copy this if this is an app shader; if we are cloning a shader, we won't render with it, so keep m_ppShader NULL.
        pShader->m_ppShader = ppShader;
    }

    pShader->m_eShaderType = ShaderType;
#if AMD_SDK_INTERNAL_BUILD
    pShader->m_eISATarget = m_eTargetISA;
#endif

    if (pShader->m_eShaderType == SHADER_TYPE_VERTEX)
    {
        pShader->m_uNumDescElements = uNumDescElements;

        if (pShader->m_uNumDescElements > 0)
        {
            assert( NULL != ppInputLayout );
            assert( NULL != pInputLayoutDesc );

            pShader->m_ppInputLayout = ppInputLayout;

            pShader->m_pInputLayoutDesc = new D3D11_INPUT_ELEMENT_DESC[pShader->m_uNumDescElements];

            memcpy( pShader->m_pInputLayoutDesc, pInputLayoutDesc, sizeof( D3D11_INPUT_ELEMENT_DESC ) * pShader->m_uNumDescElements );

            for (int iElement = 0; iElement < (int)pShader->m_uNumDescElements; iElement++)
            {
                pShader->m_pInputLayoutDesc[iElement].SemanticName = new char[m_uFILENAME_MAX_LENGTH];
                strcpy_s( (char*)pShader->m_pInputLayoutDesc[iElement].SemanticName, m_uFILENAME_MAX_LENGTH, (char*)pInputLayoutDesc[iElement].SemanticName );
            }
        }
    }

    wcscpy_s( pShader->m_wsTarget, m_uTARGET_MAX_LENGTH, pwsTarget );
    wcscpy_s( pShader->m_wsEntryPoint, m_uENTRY_POINT_MAX_LENGTH, pwsEntryPoint );
    wcscpy_s( pShader->m_wsSourceFile, m_uFILENAME_MAX_LENGTH, pwsSourceFile );
    if (NULL != pwsCanonicalName)
    {
        wcscpy_s( pShader->m_wsCanonicalName, m_uFILENAME_MAX_LENGTH, pwsCanonicalName );
    }

    pShader->m_uNumMacros = uNumMacros;
    if (pShader->m_uNumMacros > 0)
    {
        pShader->m_pMacros = new Macro[pShader->m_uNumMacros];
        memcpy( pShader->m_pMacros, pMacros, sizeof( Macro ) * pShader->m_uNumMacros );
    }

    // Object, error, assembly, preprocess, and hash file names
    wchar_t wsFileNameBody[m_uFILENAME_MAX_LENGTH] = { 0 };
#ifdef _DEBUG
    wcscat_s( pShader->m_wsObjectFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Object\\Debug\\" );
    wcscat_s( pShader->m_wsHashFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Hash\\Debug\\" );
#else
    wcscat_s( pShader->m_wsObjectFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Object\\Release\\" );
    wcscat_s( pShader->m_wsHashFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Hash\\Release\\" );
#endif
    wcscat_s( pShader->m_wsErrorFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Error\\" );
    wcscat_s( pShader->m_wsAssemblyFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Assembly\\" );
    wcscat_s( pShader->m_wsISAFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\ISA\\" );
    wcscat_s( pShader->m_wsPreprocessFile, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Preprocess\\" );

    if (NULL != pwsCanonicalName)
    {
        wcscat_s( wsFileNameBody, m_uFILENAME_MAX_LENGTH, pShader->m_wsCanonicalName );
    }
    else
    {
        wcscpy_s( wsFileNameBody, m_uFILENAME_MAX_LENGTH, pwsEntryPoint );
        for (int iMacro = 0; iMacro < (int)pShader->m_uNumMacros; ++iMacro)
        {
            wchar_t wsValue[64];
            wcscat_s( wsFileNameBody, m_uFILENAME_MAX_LENGTH, L"_" );
            wcscat_s( wsFileNameBody, m_uFILENAME_MAX_LENGTH, pShader->m_pMacros[iMacro].m_wsName );
            wcscat_s( wsFileNameBody, m_uFILENAME_MAX_LENGTH, L"=" );
            _itow_s( pShader->m_pMacros[iMacro].m_iValue, wsValue, 10 );
            wcscat_s( wsFileNameBody, m_uFILENAME_MAX_LENGTH, wsValue );
        }
    }

    wcscat_s( pShader->m_wsRawFileName, m_uFILENAME_MAX_LENGTH, wsFileNameBody );
    wcscat_s( pShader->m_wsObjectFile, m_uFILENAME_MAX_LENGTH, wsFileNameBody );
    wcscat_s( pShader->m_wsErrorFile, m_uFILENAME_MAX_LENGTH, wsFileNameBody );
    wcscat_s( pShader->m_wsAssemblyFile, m_uFILENAME_MAX_LENGTH, wsFileNameBody );
    wcscat_s( pShader->m_wsPreprocessFile, m_uFILENAME_MAX_LENGTH, wsFileNameBody );
    wcscat_s( pShader->m_wsHashFile, m_uFILENAME_MAX_LENGTH, wsFileNameBody );

    wcscat_s( pShader->m_wsObjectFile, m_uFILENAME_MAX_LENGTH, L".obj" );
    wcscat_s( pShader->m_wsErrorFile, m_uFILENAME_MAX_LENGTH, L".txt" );
    wcscat_s( pShader->m_wsAssemblyFile, m_uFILENAME_MAX_LENGTH, L".asm" );
    wcscat_s( pShader->m_wsPreprocessFile, m_uFILENAME_MAX_LENGTH, L".ppf" );
    wcscat_s( pShader->m_wsHashFile, m_uFILENAME_MAX_LENGTH, L".hsh" );

    pShader->SetupHashedFilename();

    // Setup Hashed Assembly Filename
    wcscat_s( pShader->m_wsAssemblyFileWithHashedFilename, m_uFILENAME_MAX_LENGTH, L"Shaders\\Cache\\Assembly\\" );
    wcscat_s( pShader->m_wsAssemblyFileWithHashedFilename, m_uFILENAME_MAX_LENGTH, pShader->m_wsHashedFileName );
    wcscat_s( pShader->m_wsAssemblyFileWithHashedFilename, m_uFILENAME_MAX_LENGTH, L".asm" );

#if AMD_SDK_INTERNAL_BUILD
    // ISA File now also uses hashed filename
    wcscat_s( pShader->m_wsISAFile, m_uFILENAME_MAX_LENGTH, pShader->m_wsHashedFileName );
    wcscat_s( pShader->m_wsISAFile, m_uFILENAME_MAX_LENGTH, L".asm." );
    wcscat_s( pShader->m_wsISAFile, m_uFILENAME_MAX_LENGTH, AmdTargetInfo[pShader->m_eISATarget].m_Name );
    wcscat_s( pShader->m_wsISAFile, m_uFILENAME_MAX_LENGTH, L".dump.isa" );

    wcscat_s( pShader->m_wsObjectFile_with_ISA, m_uFILENAME_MAX_LENGTH, pShader->m_wsObjectFile );
    wcscat_s( pShader->m_wsPreprocessFile_with_ISA, m_uFILENAME_MAX_LENGTH, pShader->m_wsPreprocessFile );

    if (m_bGenerateShaderISA)
    {
        wcscat_s( pShader->m_wsObjectFile_with_ISA, m_uFILENAME_MAX_LENGTH, L"." );
        wcscat_s( pShader->m_wsObjectFile_with_ISA, m_uFILENAME_MAX_LENGTH, AmdTargetInfo[pShader->m_eISATarget].m_Name );
        wcscat_s( pShader->m_wsPreprocessFile_with_ISA, m_uFILENAME_MAX_LENGTH, L"." );
        wcscat_s( pShader->m_wsPreprocessFile_with_ISA, m_uFILENAME_MAX_LENGTH, AmdTargetInfo[pShader->m_eISATarget].m_Name );
    }
#endif

    // Compilation flags based on build profile
    wchar_t wsCompilationFlags[m_uFILENAME_MAX_LENGTH];
#ifdef _DEBUG
    // Best flags for shader debugging
    // /Zi - Enable debugging information
    // /Od - Disable optimizations
    // /Gfp - Prefer flow control constructs
    wcscpy_s( wsCompilationFlags, m_uFILENAME_MAX_LENGTH, L" /Zi /Od /Gfp" );
#else
    if (m_bForceDebugShaders)
    {
        // Best flags for shader debugging
        // /Zi - Enable debugging information
        // /Od - Disable optimizations
        // /Gfp - Prefer flow control constructs
        wcscpy_s( wsCompilationFlags, m_uFILENAME_MAX_LENGTH, L" /Od" );
    }
    else
    {
        // Select optimization level ( 1 is default)
        // /O{0,1,2,3} - Optimization Level
        wcscpy_s( wsCompilationFlags, m_uFILENAME_MAX_LENGTH, L" /O1" );
    }
#endif

    // Command line
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /T " );
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pwsTarget );
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, wsCompilationFlags );
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /E " );
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pwsEntryPoint );
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /Fo " );
    InsertOutputFilenameIntoCommandLine( pShader->m_wsCommandLine, pShader->m_wsObjectFile );
    for (int iMacro = 0; iMacro < (int)pShader->m_uNumMacros; ++iMacro)
    {
        wchar_t wsValue[64];
        wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /D " );
        wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pShader->m_pMacros[iMacro].m_wsName );
        wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"=" );
        _itow_s( pShader->m_pMacros[iMacro].m_iValue, wsValue, 10 );
        wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, wsValue );
    }
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /Fe " );
    InsertOutputFilenameIntoCommandLine( pShader->m_wsCommandLine, pShader->m_wsErrorFile );
    /*    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /Fc " );
    InsertOutputFilenameIntoCommandLine( pShader->m_wsCommandLine, pShader->m_wsAssemblyFile );*/
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /Fc " ); // Fx for HEX
    InsertOutputFilenameIntoCommandLine( pShader->m_wsCommandLine, pShader->m_wsAssemblyFileWithHashedFilename );
    wcscat_s( pShader->m_wsCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" " );
    InsertInputFilenameIntoCommandLine( pShader->m_wsCommandLine, pShader->m_wsSourceFile );

#if AMD_SDK_INTERNAL_BUILD
    // ISA SCDev Command line
    wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" -q " );
    wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" -ns " );

    wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" -" );
    wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, AmdTargetInfo[m_eTargetISA].m_Name );
    wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" " );

    if (i_iMaxVGPR > 0)
    {
        wchar_t wsValue[64];
        _itow_s( i_iMaxVGPR, wsValue, 10 );
        wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" -vgprs " );
        wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, wsValue );
    }
    if (i_iMaxSGPR > 0)
    {
        wchar_t wsValue[64];
        _itow_s( i_iMaxSGPR, wsValue, 10 );
        wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" -sgprs " );
        wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, wsValue );
    }
    //  wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" " );
    //  wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\"" );
    //  wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pShader->m_wsAssemblyFile );
    //  wcscat_s( pShader->m_wsISACommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"\"" );
#endif

    // Preprocess command line
    wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /E " );
    wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pwsEntryPoint );
    wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" " );
    InsertInputFilenameIntoCommandLine( pShader->m_wsPreprocessCommandLine, pShader->m_wsSourceFile );
    wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /P " );
    InsertOutputFilenameIntoCommandLine( pShader->m_wsPreprocessCommandLine, pShader->m_wsPreprocessFile );
    for (int iMacro = 0; iMacro < (int)pShader->m_uNumMacros; ++iMacro)
    {
        wchar_t wsValue[64];
        wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L" /D " );
        wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, pShader->m_pMacros[iMacro].m_wsName );
        wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, L"=" );
        _itow_s( pShader->m_pMacros[iMacro].m_iValue, wsValue, 10 );
        wcscat_s( pShader->m_wsPreprocessCommandLine, m_uCOMMAND_LINE_MAX_LENGTH, wsValue );
    }

    m_ShaderList.push_back( pShader );

    return true;
}


//--------------------------------------------------------------------------------------
// The shader thread proc, has to be public, but must not be called by user
//--------------------------------------------------------------------------------------
DWORD WINAPI GenerateShaders_ThreadProc_( void* pParameter )
{
    ShaderCache* pShaderCache = (ShaderCache*)pParameter;

    pShaderCache->GenerateShadersThreadProc();

    SetEvent( s_hDoneEvent );

    return 0;
}


//--------------------------------------------------------------------------------------
// Initiates shader generation based upon the creation flags:
// CREATE_TYPE_FORCE_COMPILE,      // Clean the cache, and compile all
// CREATE_TYPE_COMPILE_CHANGES,    // Only compile shaders that have changed (development mode)
// CREATE_TYPE_USE_CACHED,         // Use cached shaders (release mode)
//--------------------------------------------------------------------------------------
HRESULT ShaderCache::GenerateShaders( CREATE_TYPE CreateType, const bool i_kbRecreateShaders )
{
    DWORD dwRet = WaitForSingleObject( s_hDoneEvent, 0 );

    if (dwRet == WAIT_OBJECT_0)
    {
#if !AMD_SDK_PREBUILT_RELEASE_EXE
        m_CreateType = CreateType;
#else
        // ignore the CreateType parameter for the pre-built release,
        // because the point of the pre-built is that it doesn't require
        // the DXSDK, Win8 SDK, etc.
        m_CreateType = CREATE_TYPE_USE_CACHED;
#endif
        m_bShadersCreated = false;
        m_bPrintedProgress = false;

        if (i_kbRecreateShaders)
        {
            m_CreateList.clear();
        }

        for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
        {
            Shader* pShader = *it;

            if ((m_CreateType == CREATE_TYPE_COMPILE_CHANGES) ||
                (m_CreateType == CREATE_TYPE_FORCE_COMPILE) ||
                (!CheckObjectFile( pShader )))
            {
                m_PreprocessList.push_back( pShader );
            }
            else
            {
                m_CreateList.push_back( pShader );
            }
        }

        if (m_PreprocessList.size())
        {
            m_pProgressInfo = new ProgressInfo[m_PreprocessList.size() * 2];
            m_uProgressCounter = 0;

            ResetEvent( s_hDoneEvent );
            QueueUserWorkItem( GenerateShaders_ThreadProc_, this, WT_EXECUTELONGFUNCTION );
        }
        else
        {
            SetEvent( s_hDoneEvent );
        }
    }

    if ((m_CreateType == CREATE_TYPE_USE_CACHED) || ShadersReady())
    {
        GenerateShaderGPRUsageFromISAForAllShaders();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
//  Called by the thread proc to actaully do the work
//--------------------------------------------------------------------------------------
void ShaderCache::GenerateShadersThreadProc()
{
    DeleteErrorFiles();
    DeleteAssemblyFiles();
    DeletePreprocessFiles();

    if (m_CreateType == CREATE_TYPE_FORCE_COMPILE)
    {
        DeleteHashFiles();
        DeleteObjectFiles();
    }

    // Remove Old Shader Errors from displaying over shader recompilation
    m_bHasShaderErrorsToDisplay = false;
    m_shaderErrorRenderedCount = 0;

    PreprocessShaders();
    CompileShaders();
}

//--------------------------------------------------------------------------------------
// Renders the progress of the shader generation process
//--------------------------------------------------------------------------------------
void ShaderCache::RenderProgress( CDXUTTextHelper* g_pTxtHelper, int iFontHeight, DirectX::XMVECTOR FontColor )
{
    wchar_t wsOverallProgress[m_uPATHNAME_MAX_LENGTH];
    wchar_t wsCurrentLine[m_uCOMMAND_LINE_MAX_LENGTH];

    g_pTxtHelper->Begin();
    g_pTxtHelper->SetForegroundColor( FontColor );
    g_pTxtHelper->SetInsertionPos( 5, 5 );

    int iNumLines = (int)((DXUTGetDXGIBackBufferSurfaceDesc()->Height - (iFontHeight)) * 0.99f / iFontHeight);

    if (!m_bPrintedProgress && !m_PreprocessList.size())
    {
        swprintf_s( wsOverallProgress, L"*** Shader Cache: Creating Shaders... ***" );
        g_pTxtHelper->DrawTextLine( wsOverallProgress );

        m_bPrintedProgress = true;
    }
    else
    {
        swprintf_s( wsOverallProgress, L"*** Shader Cache: Shaders to Preprocess = %d, Compile = %d ***", (int)m_PreprocessList.size(), (int)m_CompileList.size() );
        g_pTxtHelper->DrawTextLine( wsOverallProgress );
    }

    if (NULL != m_pProgressInfo)
    {
        g_pTxtHelper->SetInsertionPos( 5, 5 + iFontHeight );

        int iCounter = m_uProgressCounter;
        int iStrings = (iCounter < iNumLines) ? (iCounter) : (iNumLines);

        for (int i = iCounter - iStrings; i < iCounter; i++)
        {
            if (m_pProgressInfo[i].m_pShader->m_iCompileWaitCount >= 0)
            {
                swprintf_s( wsCurrentLine, L"%s ... [%s][%i]", m_pProgressInfo[i].m_wsFilename, m_pProgressInfo[i].m_pShader->m_wsCompileStatus, m_pProgressInfo[i].m_pShader->m_iCompileWaitCount );
            }
            else
            {
                swprintf_s( wsCurrentLine, L"%s ... [%s]", m_pProgressInfo[i].m_wsFilename, m_pProgressInfo[i].m_pShader->m_wsCompileStatus/*m_pProgressInfo[i].m_wsStatus*/ );
            }
            g_pTxtHelper->DrawTextLine( wsCurrentLine );
        }
    }

    g_pTxtHelper->End();

    if (ShowShaderErrors() && HasErrorsToDisplay())
    {
        RenderShaderErrors( g_pTxtHelper, iFontHeight, FontColor );
    }
}


//--------------------------------------------------------------------------------------
// boolean method to determine if the shaders are ready
//--------------------------------------------------------------------------------------
bool ShaderCache::ShadersReady()
{
    if (TryEnterCriticalSection( &m_CompileShaders_CriticalSection ))
    {

        DWORD dwRet = WaitForSingleObject( s_hDoneEvent, 0 );

        if (dwRet == WAIT_OBJECT_0)
        {
            if (m_bPrintedProgress)
            {
                if (!m_bShadersCreated)
                {
                    CreateShaders();
                    m_bShadersCreated = true;

                    if (NULL != m_pProgressInfo)
                    {
                        delete [] m_pProgressInfo;
                        m_pProgressInfo = NULL;
                        m_uProgressCounter = 0;
                    }
                }

                LeaveCriticalSection( &m_CompileShaders_CriticalSection );
                return true;
            }
        }
        LeaveCriticalSection( &m_CompileShaders_CriticalSection );

    }

    return false;
}


//--------------------------------------------------------------------------------------
// public and private setter/getter methods:
//--------------------------------------------------------------------------------------

const bool ShaderCache::HasErrorsToDisplay( void ) const
{
    return m_bHasShaderErrorsToDisplay;
}

const bool ShaderCache::ShowShaderErrors( void ) const
{
    return (m_ErrorDisplayType == ERROR_DISPLAY_ON_SCREEN) && m_bShowShaderErrors;
}

const int ShaderCache::ShaderErrorDisplayType( void ) const
{
    return m_ErrorDisplayType;
}

const bool ShaderCache::RecompileTouchedShaders( void ) const
{
    return m_bRecompileTouchedShaders;
}

const bool ShaderCache::GenerateISAGPRPressure( void ) const
{
    return m_bGenerateShaderISA;
}

const bool ShaderCache::ShowISAGPRPressure( void ) const
{
    return m_bShowShaderISA;
}

void ShaderCache::SetRecompileTouchedShadersFlag( const bool i_bRecompileWhenTouched )
{
    m_bRecompileTouchedShaders = i_bRecompileWhenTouched;

    if (m_bRecompileTouchedShaders)
    {
        // Create Directory Watcher
        if ((m_waitPoolHandle == NULL) || (m_watchHandle == NULL))
        {
#if defined(DEBUG) || defined(_DEBUG)
            const bool kb_Success = WatchDirectoryForChanges();
            assert( kb_Success );
#else
            WatchDirectoryForChanges();
#endif
        }
    }
}

void ShaderCache::SetMaximumCoresForShaderCompiler( const int ki_MaxCores )
{

    switch (ki_MaxCores)
    {
    case MAXCORES_NO_LIMIT:
        m_MaxCoresType = MAXCORES_NO_LIMIT;
        m_uNumCPUCoresToUse = 65536;
        break;
    case MAXCORES_2X_CPU_CORES:
        m_MaxCoresType = MAXCORES_2X_CPU_CORES;
        m_uNumCPUCoresToUse = m_uNumCPUCores * 2;
        break;
    case MAXCORES_USE_ALL_CORES:
        m_MaxCoresType = MAXCORES_USE_ALL_CORES;
        m_uNumCPUCoresToUse = m_uNumCPUCores;
        break;
    case MAXCORES_MULTI_THREADED:
    case MAXCORES_USE_ALL_BUT_ONE:
        m_MaxCoresType = MAXCORES_USE_ALL_BUT_ONE;
        m_uNumCPUCoresToUse = (m_uNumCPUCores > 1) ? (m_uNumCPUCores - 1) : (1);
        break;
    case MAXCORES_SINGLE_THREADED:
        m_MaxCoresType = MAXCORES_SINGLE_THREADED;
        m_uNumCPUCoresToUse = 1;
        break;
    default:
        m_MaxCoresType = MAXCORES_MULTI_THREADED;
        m_uNumCPUCoresToUse = ki_MaxCores;
        break;
    };

    return;

}

void ShaderCache::SetShowShaderErrorsFlag( const bool i_kbShowShaderErrors )
{
    m_bShowShaderErrors = i_kbShowShaderErrors;
}

void ShaderCache::SetGenerateShaderISAFlag( const bool i_kbGenerateShaderISA )
{
    m_bGenerateShaderISA = i_kbGenerateShaderISA;
}

void ShaderCache::SetShowShaderISAFlag( const bool i_kbShowShaderISA )
{
    m_bShowShaderISA = i_kbShowShaderISA;
}

#if AMD_SDK_INTERNAL_BUILD
void ShaderCache::SetTargetISA( const ISA_TARGET i_eTargetISA )
{
    m_eTargetISA = i_eTargetISA;
    assert( (i_eTargetISA >= FIRST_ISA_TARGET) && (i_eTargetISA < NUM_ISA_TARGETS) );
}
#endif

//--------------------------------------------------------------------------------------
// private methods:
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Shader Watchdog -- Recompiles shaders as they are touched (if they have changed)
//--------------------------------------------------------------------------------------

bool ShaderCache::WatchDirectoryForChanges( void )
{
    assert( !m_watchHandle );
    assert( !m_waitPoolHandle );

    if (m_waitPoolHandle)
    {
        UnregisterWaitEx( m_waitPoolHandle, INVALID_HANDLE_VALUE );
        m_waitPoolHandle = NULL;
    }

    if (m_watchHandle)
    {
        FindCloseChangeNotification( m_watchHandle );
        m_watchHandle = NULL;
    }

    assert( !m_watchHandle );
    assert( !m_waitPoolHandle );

    HANDLE watchHandle = FindFirstChangeNotification( m_wsShaderSourceDir, TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE );
    if (watchHandle == INVALID_HANDLE_VALUE)
    {
        wchar_t wsErrorString[m_uCOMMAND_LINE_MAX_LENGTH];
        DWORD error = GetLastError();
        swprintf_s( wsErrorString, L"\n\n*** Shader Cache: Error '%x' in FindFirstChangeNotification while attempting to watch directory '%s' ***\n\n", error, m_wsShaderSourceDir );
        OutputDebugStringW( wsErrorString );
        return false;
    }

    m_watchHandle = watchHandle;

    HANDLE poolHandle;
    if (!RegisterWaitForSingleObject( &poolHandle, watchHandle, onDirectoryChangeEventTriggered, (void*) this, INFINITE, WT_EXECUTEINWAITTHREAD ))
    {
        wchar_t wsErrorString[m_uCOMMAND_LINE_MAX_LENGTH];
        DWORD error = GetLastError();
        swprintf_s( wsErrorString, L"\n\n*** Shader Cache: Error '%x' in RegisterWaitForSingleObject while attempting to watch directory '%s' ***\n\n", error, m_wsShaderSourceDir );
        OutputDebugStringW( wsErrorString );
        return false;
    }

    m_waitPoolHandle = poolHandle;

    wchar_t wsErrorString[m_uCOMMAND_LINE_MAX_LENGTH];
    swprintf_s( wsErrorString, L"\n\n*** Shader Cache: Succesfully enabled watching of directory '%s' ***\n\n", m_wsShaderSourceDir );
    OutputDebugStringW( wsErrorString );

    return true;
}


void __stdcall ShaderCache::onDirectoryChangeEventTriggered( void* args, BOOLEAN /*timeout*/ )
{
    ShaderCache* pShaderCache = reinterpret_cast<ShaderCache *>(args);

    if (pShaderCache->RecompileTouchedShaders())
    {
        // Don't recompile if shaders are currently compiling!
        if (pShaderCache->ShadersReady())
        {
            pShaderCache->GenerateShaders( AMD::ShaderCache::CREATE_TYPE_COMPILE_CHANGES, true );

            wchar_t wsErrorString[m_uCOMMAND_LINE_MAX_LENGTH];
            swprintf_s( wsErrorString, L"\n\n*** ShaderCache::onDirectoryChangeEventTriggered! @ [%s] ***\n\n", pShaderCache->m_wsShaderSourceDir );
            OutputDebugStringW( wsErrorString );
        }
        else
        {
            wchar_t wsErrorString[m_uCOMMAND_LINE_MAX_LENGTH];
            swprintf_s( wsErrorString, L"\n\n*** ShaderCache::onDirectoryChangeEventTriggered! @ [%s] -- SKIPPED, because shaders are already compiling. ***\n\n", pShaderCache->m_wsShaderSourceDir );
            OutputDebugStringW( wsErrorString );
        }
    }

    FindNextChangeNotification( pShaderCache->m_watchHandle );
    FindNextChangeNotification( pShaderCache->m_watchHandle ); //twice
}

//--------------------------------------------------------------------------------------
// Methods to get AMD ISA
//--------------------------------------------------------------------------------------

// Methods to run SCDev to generate ISA
bool ShaderCache::GenerateISAForAllShaders()
{
    return false;
}

bool ShaderCache::GenerateShaderISA( Shader *pShader, const bool i_kbParseGPRPressure )
{

    bool bSuccess = false;
    if (!m_bGenerateShaderISA)
    {
        return bSuccess;
    }

#if AMD_SDK_INTERNAL_BUILD
    //EnterCriticalSection( &m_GenISA_CriticalSection );

    {
        pShader->m_wsCompileStatus = L"ISA Compiler: Phase 1";

        wchar_t wsASM[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsASM, L"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, m_wsUnicodeWorkingDir );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\\" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, pShader->m_wsAssemblyFileWithHashedFilename );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );

        wchar_t wsISACL[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsISACL, L"%s %s", pShader->m_wsISACommandLine, wsASM );

        wchar_t wsShaderSCDEVWorkingDir[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsShaderSCDEVWorkingDir, L"%s\\%s", m_wsSCDEVWorkingDir, AmdTargetInfo[pShader->m_eISATarget].m_Name );
#if defined(DEBUG) || defined(_DEBUG)
        const bool bRet1 = CreateDirectoryW( wsShaderSCDEVWorkingDir, NULL ) != ERROR_PATH_NOT_FOUND; assert( bRet1 );
#else
        CreateDirectoryW( wsShaderSCDEVWorkingDir, NULL );
#endif
        //swprintf_s( wsShaderSCDEVWorkingDir, L"%s\\%s\\%s", m_wsSCDEVWorkingDir, AmdTargetInfo[ pShader->m_eISATarget ].m_Name, pShader->m_wsRawFileName );
        swprintf_s( wsShaderSCDEVWorkingDir, L"%s\\%s\\%s", m_wsSCDEVWorkingDir, AmdTargetInfo[pShader->m_eISATarget].m_Name, pShader->m_wsHashedFileName );
#if defined(DEBUG) || defined(_DEBUG)
        const bool bRet2 = CreateDirectoryW( wsShaderSCDEVWorkingDir, NULL ) != ERROR_PATH_NOT_FOUND; assert( bRet2 );
#else
        CreateDirectoryW( wsShaderSCDEVWorkingDir, NULL );
#endif

        SHELLEXECUTEINFO shExecInfo;
        memset( &shExecInfo, 0, sizeof( SHELLEXECUTEINFO ) );
        shExecInfo.cbSize = sizeof( SHELLEXECUTEINFO );
        shExecInfo.fMask = SEE_MASK_NOASYNC | SEE_MASK_UNICODE;
        shExecInfo.hwnd = NULL;
        shExecInfo.lpFile = m_wsDevExePath;
        shExecInfo.nShow = SW_HIDE;
        shExecInfo.hInstApp = NULL;
        shExecInfo.lpParameters = wsISACL;
        shExecInfo.lpDirectory = wsShaderSCDEVWorkingDir;
        bSuccess = ShellExecuteEx( &shExecInfo ) ? true : false;
        assert( bSuccess );
    }

    if (bSuccess)
    {
        pShader->m_wsCompileStatus = L"ISA Compiler: Phase 1 ... done!";
    }
    else
    {
        pShader->m_wsCompileStatus = L"ISA Compiler: Phase 1 ... failed!";
    }

    if (bSuccess)
    {
        pShader->m_wsCompileStatus = L"ISA Compiler: Phase 2";

        wchar_t wsEXE[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsEXE, L"%s%s", m_wsAmdSdkDir, L"\\src\\Shaders\\MoveSCDevOutput.bat" );

        wchar_t wsASM[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsASM, L"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        //wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, pShader->m_wsRawFileName );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, pShader->m_wsHashedFileName );
        //wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L".asm" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L" " );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, m_wsBatchWorkingDir );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L" " );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, AmdTargetInfo[pShader->m_eISATarget].m_Name );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L" " );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, pShader->m_wsHashedFileName );
        wcscat_s( wsASM, m_uFILENAME_MAX_LENGTH, L"\"" );

        SHELLEXECUTEINFO shExecInfo;
        memset( &shExecInfo, 0, sizeof( SHELLEXECUTEINFO ) );
        shExecInfo.cbSize = sizeof( SHELLEXECUTEINFO );
        shExecInfo.fMask = (i_kbParseGPRPressure) ? (SEE_MASK_NOASYNC | SEE_MASK_UNICODE) : SEE_MASK_ASYNCOK; // Optimization: Async if we don't plan to read this immediately
        shExecInfo.hwnd = NULL;
        shExecInfo.lpFile = wsEXE;
        shExecInfo.nShow = SW_HIDE;
        shExecInfo.hInstApp = NULL;
        shExecInfo.lpParameters = wsASM;
        bSuccess = ShellExecuteEx( &shExecInfo ) ? true : false;
        assert( bSuccess );

        if (bSuccess)
        {
            pShader->m_wsCompileStatus = L"ISA Compiler: Finished";
        }
        else
        {
            pShader->m_wsCompileStatus = L"ISA Compiler Failed!";
        }

        pShader->m_bGPRsUpToDate = false;
    }

    if (i_kbParseGPRPressure)
    {
        unsigned int uNumVGPR = 0;
        unsigned int uNumSGPR = 0;
        if (GetShaderGPRUsageFromISA( pShader, uNumVGPR, uNumSGPR ))
        {
            wchar_t wsGPRS[m_uPATHNAME_MAX_LENGTH];
            swprintf_s( wsGPRS, L"\n\n%s::%s\nVGPRs: %u\nSGPRs: %u\n\n", pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, uNumVGPR, uNumSGPR );
            OutputDebugStringW( wsGPRS );
        }
        else
        {
            wchar_t wsGPRS[m_uPATHNAME_MAX_LENGTH];
            swprintf_s( wsGPRS, L"\n\nFailed to determine NumVGPR and NumSGPR for %s::%s\n\n", pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name );
            OutputDebugStringW( wsGPRS );
        }
    }

    //LeaveCriticalSection( &m_GenISA_CriticalSection );
#endif

    return bSuccess;
}

void ShaderCache::DeleteISAFiles()
{}

void ShaderCache::DeleteISAFile( Shader *pShader )
{}

bool ShaderCache::GetShaderGPRUsageFromISA( Shader *pShader, unsigned int& io_uNumVGPR, unsigned int& io_uNumSGPR ) const
{
    if (!GenerateISAGPRPressure())
    {
        return true;
    }

#if AMD_SDK_INTERNAL_BUILD
    unsigned int io_uGPRPoolSize = 0;
    float io_fALUPacking = 0.0;

    pShader->m_wsCompileStatus = L"Parsing GPR Pressure";

    if (pShader->m_bGPRsUpToDate)
    {
        wchar_t wsGPRS[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsGPRS, L"\n\n%s::%s\nVGPRs: %u\nSGPRs: %u [Warning -- GetShaderGPRUsageFromISA called unnecessarily; GPRs Up To Date; Update Skipped]\n\n",
            pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, pShader->m_ISA_VGPRs, pShader->m_ISA_SGPRs );
        OutputDebugStringW( wsGPRS );
        assert( false );
        pShader->m_wsCompileStatus = L"GPRs Up-to-date!";
        return true;
    }

    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];
    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsISAFile );
    _wfopen_s( &pFile, wsShaderPathName, L"rt" );

    io_uNumVGPR = 0;
    io_uNumSGPR = 0;
    io_uGPRPoolSize = 0;
    io_fALUPacking = 0.0f;

    /*
    NumVgprs             = 27;
    NumSgprs             = 28;
    */

    if (pFile)
    {

        wchar_t szLine[m_uCOMMAND_LINE_MAX_LENGTH];
        wchar_t* pLine = szLine;

        wchar_t szNumVgprs[32];
        wchar_t szNumSgprs[32];
        wchar_t szGPRPoolSize[32];
        wchar_t szNumGPRs[32];
        wchar_t szALUPacking[32];

        wcscpy_s( szNumVgprs, 32, L"NumVgprs" );                  // GCN Architecture (Vector GPR)
        wcscpy_s( szNumSgprs, 32, L"NumSgprs" );                  // GCN Architecture (Scalar GPR)
        wcscpy_s( szGPRPoolSize, 32, L"GprPoolSize" );               // VLIW Architecture
        wcscpy_s( szNumGPRs, 32, L"SQ_PGM_RESOURCES:NUM_GPRS" ); // VLIW Architecture
        wcscpy_s( szALUPacking, 32, L";AluPacking" );               // VLIW Architecture


        while (fgetws( pLine, m_uCOMMAND_LINE_MAX_LENGTH, pFile ))
        {
            wchar_t* pTemp = pLine;
            while (*pTemp != L'\n')
            {

                if (!wcsncmp( pTemp, szNumVgprs, wcslen( szNumVgprs ) ))
                {
                    wchar_t *pStr = wcschr( pTemp, L'=' );
                    if (pStr)
                    {
                        pStr += 2;
                        io_uNumVGPR = _wtoi( pStr );
                    }
                }
                else if (!wcsncmp( pTemp, szNumSgprs, wcslen( szNumSgprs ) ))
                {
                    wchar_t *pStr = wcschr( pTemp, L'=' );
                    if (pStr)
                    {
                        pStr += 2;
                        io_uNumSGPR = _wtoi( pStr );
                    }
                }
                else if (!wcsncmp( pTemp, szNumGPRs, wcslen( szNumGPRs ) ))
                {
                    wchar_t *pStr = wcschr( pTemp, L'=' );
                    if (pStr)
                    {
                        pStr += 2;
                        io_uNumVGPR = _wtoi( pStr );
                        assert( (io_uNumSGPR == 0) || (io_uNumSGPR == (~0)) );
                        io_uNumSGPR = (~0u); // Special Flag to indicate that this is a GPR Pool
                    }
                }
                else if (!wcsncmp( pTemp, szALUPacking, wcslen( szALUPacking ) ))
                {
                    wchar_t *pStr = wcschr( pTemp, L'=' );
                    if (pStr)
                    {
                        pStr += 2;
                        io_fALUPacking = static_cast<float>(_wtof( pStr ));
                        assert( (io_uNumSGPR == 0) || (io_uNumSGPR == (~0)) );
                        io_uNumSGPR = (~0u); // Special Flag to indicate that this is a GPR Pool
                    }
                }
                else if (!wcsncmp( pTemp, szGPRPoolSize, wcslen( szGPRPoolSize ) ))
                {
                    wchar_t *pStr = wcschr( pTemp, L'=' );
                    if (pStr)
                    {
                        pStr += 2;
                        io_uGPRPoolSize = _wtoi( pStr );
                        assert( (io_uNumSGPR == 0) || (io_uNumSGPR == (~0)) );
                        io_uNumSGPR = (~0u); // Special Flag to indicate that this is a GPR Pool
                    }
                }

                if ((io_uNumVGPR > 0) && (io_uNumSGPR > 0))
                {
                    // Cache previous results
                    pShader->m_previous_ISA_VGPRs = pShader->m_ISA_VGPRs;
                    pShader->m_previous_ISA_SGPRs = pShader->m_ISA_SGPRs;

                    pShader->m_previous_ISA_GPRPoolSize = pShader->m_ISA_GPRPoolSize;
                    pShader->m_previous_ISA_ALUPacking = pShader->m_ISA_ALUPacking;

                    pShader->m_ISA_VGPRs = io_uNumVGPR;
                    pShader->m_ISA_SGPRs = io_uNumSGPR;

                    pShader->m_ISA_GPRPoolSize = io_uGPRPoolSize;
                    pShader->m_ISA_ALUPacking = io_fALUPacking;

                    pShader->m_bGPRsUpToDate = true;
                    fclose( pFile );

                    pShader->m_wsCompileStatus = L"GPR Pressure Updated";
                    return true;
                }

                ++pTemp;
            }
        }

        fclose( pFile );

    }

    pShader->m_wsCompileStatus = L"Failed to read ISA";
    return false;
#else
    return true;
#endif

}

bool ShaderCache::GenerateShaderGPRUsageFromISAForAllShaders( const bool ik_bGenerateISAOnFailure )
{
    if (!GenerateISAGPRPressure())
    {
        return true;
    }

#if AMD_SDK_INTERNAL_BUILD
    bool bReturnValue = false;

    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;
        unsigned int VGPR = 0, SGPR = 0;

        if (pShader->m_bGPRsUpToDate)
        {
            wchar_t wsGPRS[m_uPATHNAME_MAX_LENGTH];
            swprintf_s( wsGPRS, L"\n\n%s::%s\nVGPRs: %u\nSGPRs: %u [GPRs Up To Date; Update Skipped]\n\n",
                pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, pShader->m_ISA_VGPRs, pShader->m_ISA_SGPRs );
            OutputDebugStringW( wsGPRS );
            pShader->m_wsCompileStatus = L"GPRs Up-to-date!";
            continue;
        }

        pShader->m_wsCompileStatus = L"Reading GPR Pressure";
        bool bOK = GetShaderGPRUsageFromISA( pShader, VGPR, SGPR );
        // assert( k_bOK );

        if ((!bOK) && ik_bGenerateISAOnFailure) // Allow one single retry on failure...
        {
            // Possibly need to generate the Shader ISA? Try it once.
            pShader->m_wsCompileStatus = L"Generating ISA";
            const bool kGenSuccess = GenerateShaderISA( pShader, false ); // Don't parse GPR pressure (prevent infinite loop)
            if (kGenSuccess)
            {
                bOK = GetShaderGPRUsageFromISA( pShader, VGPR, SGPR );
            }
        }

        const bool k_bOK = bOK;

        if (k_bOK)
        {
            wchar_t wsGPRS[m_uPATHNAME_MAX_LENGTH];
            swprintf_s( wsGPRS, L"\n\n%s::%s\nVGPRs: %u\nSGPRs: %u\n\n", pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, VGPR, SGPR );
            OutputDebugStringW( wsGPRS );
        }
        else
        {
            wchar_t wsGPRS[m_uPATHNAME_MAX_LENGTH];
            swprintf_s( wsGPRS, L"\n\nFailed to determine NumVGPR and NumSGPR for %s::%s\n\n", pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name );
            OutputDebugStringW( wsGPRS );
        }

        if (!k_bOK)
        {
            if (m_ErrorDisplayType == ERROR_DISPLAY_IN_MESSAGE_BOX)
            {
                wchar_t wsFailureMessage[m_uPATHNAME_MAX_LENGTH];
                swprintf_s( wsFailureMessage, L"*** Could not extract GPR usage from %s ISA.\nThe ISA Shader Compiler may have failed to execute. ***", AmdTargetInfo[pShader->m_eISATarget].m_Name );
                MessageBoxW( NULL, wsFailureMessage, L"Error", MB_OK );
            }
            else
            {
                wchar_t wsFailureMessage[m_uPATHNAME_MAX_LENGTH];
                swprintf_s( wsFailureMessage, L"*** Could not extract GPR usage from %s ISA. The ISA Shader Compiler may have failed to execute. ***\n", AmdTargetInfo[pShader->m_eISATarget].m_Name );
                OutputDebugStringW( wsFailureMessage );
                if (ShowShaderErrors())
                {
                    const bool kbAppendToError = (wcslen( m_wsLastShaderError ) < (3 * m_uCOMMAND_LINE_MAX_LENGTH));
                    swprintf_s( m_wsLastShaderError, L"%s%s", (m_bHasShaderErrorsToDisplay && kbAppendToError) ? m_wsLastShaderError : L"", wsFailureMessage );
                    m_bHasShaderErrorsToDisplay = true;
                    m_shaderErrorRenderedCount = 0;
                }
            }
        }

        bReturnValue |= k_bOK;
    }

    return bReturnValue;
#else
    return true;
#endif
}

// Renders the GPR usage for the shaders
void ShaderCache::RenderISAInfo( CDXUTTextHelper* g_pTxtHelper, int iFontHeight, DirectX::XMVECTOR FontColor, const Shader *i_pShaderCmp, wchar_t *o_wsGPRInfo )
{
#if AMD_SDK_INTERNAL_BUILD
    wchar_t wsOverallProgress[m_uPATHNAME_MAX_LENGTH];
    DirectX::XMVECTOR RedFontColor = DirectX::XMVectorSet( 1.0f, 0.0f, 0.0f, 1.0f );
    DirectX::XMVECTOR GreenFontColor = DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    DirectX::XMVECTOR BlueFontColor = DirectX::XMVectorSet( 0.0f, 0.0f, 1.0f, 1.0f );

    if (!ShowISAGPRPressure()) { return; }

    if (g_pTxtHelper)
    {
        g_pTxtHelper->Begin();
        g_pTxtHelper->SetForegroundColor( FontColor );
        g_pTxtHelper->SetInsertionPos( 5, (m_bHasShaderErrorsToDisplay) ? 300 : 60 );
    }

    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;

        if (i_pShaderCmp && (pShader != i_pShaderCmp)) { continue; } // We're trying to update GPR info for a single shader, skip the rest

        if ((pShader->m_ISA_SGPRs == 0) && (pShader->m_ISA_VGPRs == 0))
        {
            // Failed to read ISA
            swprintf_s( wsOverallProgress, L"%s.%s%s\t[Failed to locate ISA (try building all shaders?)]",
                pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, AmdTargetInfo[pShader->m_eISATarget].m_Info );
        }
        else if (pShader->m_ISA_SGPRs == (~0))
        {
            // VLIW Hardware, GPR Pool
            swprintf_s( wsOverallProgress, L"%s.%s%s\t[VGPR: %u, SGPR: n/a, GPRPool: %u, ALUPacking %f]",
                pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, AmdTargetInfo[pShader->m_eISATarget].m_Info, pShader->m_ISA_VGPRs,
                pShader->m_ISA_GPRPoolSize, pShader->m_ISA_ALUPacking );
            const int GPRDelta = (pShader->m_ISA_VGPRs - pShader->m_previous_ISA_VGPRs);
            if ((pShader->m_previous_ISA_VGPRs != 0) && (GPRDelta != 0))
            {
                if (g_pTxtHelper)
                {
                    g_pTxtHelper->SetForegroundColor( (GPRDelta > 0) ? RedFontColor : GreenFontColor );
                }
                swprintf_s( wsOverallProgress, L"%s\t[Prev VGPR: %u, Delta: %i] -- [Prev GPRPool: %u, Prev ALUPacking %f]",
                    wsOverallProgress, pShader->m_previous_ISA_VGPRs, GPRDelta, pShader->m_previous_ISA_GPRPoolSize, pShader->m_previous_ISA_ALUPacking );
            }
        }
        else
        {
            swprintf_s( wsOverallProgress, L"%s.%s%s\t[VGPR: %u, SGPR: %u]",
                pShader->m_wsRawFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, AmdTargetInfo[pShader->m_eISATarget].m_Info, pShader->m_ISA_VGPRs, pShader->m_ISA_SGPRs );
            const int VGPRDelta = (pShader->m_ISA_VGPRs - pShader->m_previous_ISA_VGPRs);
            const int SGPRDelta = (pShader->m_ISA_SGPRs - pShader->m_previous_ISA_SGPRs);
            DirectX::XMVECTOR thisColor;
            if (VGPRDelta > 0)
            {
                if (SGPRDelta >= 0)
                {
                    thisColor = RedFontColor;
                }
                else
                {
                    thisColor = BlueFontColor;
                }
            }
            else
            {
                if (SGPRDelta <= 0)
                {
                    thisColor = GreenFontColor;
                }
                else
                {
                    thisColor = BlueFontColor;
                }
            }

            if ((pShader->m_previous_ISA_VGPRs != 0) && (VGPRDelta != 0))
            {
                if (g_pTxtHelper)
                {
                    g_pTxtHelper->SetForegroundColor( thisColor );
                }
                swprintf_s( wsOverallProgress, L"%s\t[Prev VGPR: %u, Delta: %i]", wsOverallProgress, pShader->m_previous_ISA_VGPRs, VGPRDelta );
            }
            if ((pShader->m_previous_ISA_SGPRs != 0) && (SGPRDelta != 0))
            {
                if (g_pTxtHelper)
                {
                    g_pTxtHelper->SetForegroundColor( thisColor );
                }
                swprintf_s( wsOverallProgress, L"%s\t[Prev SGPR: %u, Delta: %i]", wsOverallProgress, pShader->m_previous_ISA_SGPRs, SGPRDelta );
            }
        }
        if (g_pTxtHelper)
        {
            g_pTxtHelper->DrawTextLine( wsOverallProgress );
            g_pTxtHelper->SetForegroundColor( FontColor );
        }
        if (o_wsGPRInfo)
        {
            swprintf_s( o_wsGPRInfo, m_uPATHNAME_MAX_LENGTH, L"%s", wsOverallProgress ); // Write out the progress string
        }
    }

    if (g_pTxtHelper)
    {
        g_pTxtHelper->End();
    }
#endif
}

class HTMLWriter
{
public:
    HTMLWriter( FILE* i_pFile, const wchar_t* i_wcsTitle = NULL, const int i_ReloadRate = -1, const bool i_bUseScripts = true )
        : m_pFile( i_pFile )
        , m_wsTitle( i_wcsTitle )
        , m_bUseScripts( i_bUseScripts )
        , m_iReloadRate( i_ReloadRate )
    {
        assert( i_pFile );
        WriteHTMLHeader( i_wcsTitle );
    }

    ~HTMLWriter( void )
    {
        //WriteHTMLFooter();
    }

    void writeHTML( const wchar_t* i_wcsString )
    {
        fwrite( i_wcsString, wcslen( i_wcsString ) * sizeof( wchar_t ), 1, m_pFile );
    }

    void StartTabTable( void )
    {
        writeHTML( L"<div id=\"tabs\">\n" );
        writeHTML( L"<ul>\n" );
    }

    void AddTab( const int i_tabID, const wchar_t* i_wcsTabName )
    {
        wchar_t wsTabString[AMD::ShaderCache::m_uPATHNAME_MAX_LENGTH];
        assert( wcslen( i_wcsTabName ) < AMD::ShaderCache::m_uFILENAME_MAX_LENGTH );
        swprintf_s( wsTabString, L"<li><a href=\"#tabs-%i\">%s</a></li>\n", i_tabID, i_wcsTabName );
        writeHTML( wsTabString );
    }

    void EndTabTableHeader( void )
    {
        writeHTML( L"</ul>\n" );
    }

    void AddTabBody( const int i_tabID, const wchar_t* i_wcsTabName, const wchar_t* i_wcsTabBody )
    {
        wchar_t wsTabString[AMD::ShaderCache::m_uPATHNAME_MAX_LENGTH];
        assert( wcslen( i_wcsTabName ) < AMD::ShaderCache::m_uFILENAME_MAX_LENGTH );
        swprintf_s( wsTabString, L"<div id=\"tabs-%i\">\n", i_tabID );
        writeHTML( wsTabString );
        writeHTML( i_wcsTabBody );
        writeHTML( L"</div>\n" );
    }

    void EndTabTable( void )
    {
        writeHTML( L"</div>\n" );
    }

    void AddFileTableRow( const wchar_t* i_wcsString, const bool i_bCreateLink = true )
    {
        wchar_t wsTabString[AMD::ShaderCache::m_uPATHNAME_MAX_LENGTH];
        assert( wcslen( i_wcsString ) < AMD::ShaderCache::m_uFILENAME_MAX_LENGTH );
        swprintf_s( wsTabString, L"<a href=\"%s\">%s</a>", i_wcsString, i_wcsString );

        writeHTML( L"<td>" );
        writeHTML( (i_bCreateLink) ? wsTabString : i_wcsString );
        writeHTML( L"</td>\n" );
    }

    void FinishHTML( void )
    {
        WriteHTMLFooter();
    }

private:
    // Declare the copy constructor and assignment operator private, and don't implement them.
    // That is, make HTMLWriter uncopyable. This also prevents the compiler warning (level 4)
    // C4512 : assignment operator could not be generated (caused by const member variables)
    HTMLWriter( const HTMLWriter& );
    HTMLWriter& operator=(const HTMLWriter&);

    void WriteHTMLHeader( const wchar_t* i_wcsTitle = NULL )
    {
        writeHTML( L"<!doctype html>\n" );
        writeHTML( L"<html lang=\"en\">\n" );
        writeHTML( L"<head>\n" );
        writeHTML( L"<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n" );
        writeHTML( L"<meta http-equiv=\"X-UA-Compatible\" content=\"IE=7\">\n" );
        if (m_iReloadRate > 0)
        {
            wchar_t wsReloadRate[AMD::ShaderCache::m_uPATHNAME_MAX_LENGTH];
            swprintf_s( wsReloadRate, L"<meta http-equiv=\"refresh\" content=\"%i\">\n", m_iReloadRate );
            writeHTML( wsReloadRate );
        }
        writeHTML( L"\t<title>" );
        writeHTML( (i_wcsTitle) ? i_wcsTitle : L"Page Generated by HTMLWriter" );
        writeHTML( L"</title>\n" );
        if (m_bUseScripts)
        {
            WriteScriptHeader();
        }
        WriteHTMLHeadEnding();
    }

    void WriteScriptHeader( void )
    {
        writeHTML( L"\t<link rel=\"stylesheet\" href=\"http://code.jquery.com/ui/1.10.3/themes/smoothness/jquery-ui.css\" />\n" );
        writeHTML( L"\t<script src=\"http://code.jquery.com/jquery-1.9.1.js\"></script>\n" );
        writeHTML( L"\t<script src=\"http://code.jquery.com/ui/1.10.3/jquery-ui.js\"></script>\n" );
        writeHTML( L"\t<link rel=\"stylesheet\" href=\"http://jqueryui.com/resources/demos/style.css\" />\n" );
        writeHTML( L"\t<script>\n" );
        writeHTML( L"\t$(function() {\n" );
        writeHTML( L"\t\t$( \"#tabs\" ).tabs();\n" );
        writeHTML( L"\t});\n" );
        writeHTML( L"\t</script>\n" );
    }

    void WriteHTMLHeadEnding( void )
    {
        writeHTML( L"</head>\n" );
        writeHTML( L"<body>\n" );
    }

    void WriteHTMLFooter( void )
    {
        writeHTML( L"</body>\n" );
        writeHTML( L"</html>\n" );
    }

private:
    FILE*           m_pFile;
    const wchar_t*  m_wsTitle;
    const int       m_iReloadRate;
    const bool      m_bUseScripts;

};

//--------------------------------------------------------------------------------------
// Creates Human-readable Hash Digest html file with hyperlinks links and plain filenames
//--------------------------------------------------------------------------------------
bool ShaderCache::CreateHashDigest( const std::list<Shader*>& i_ShaderList )
{
    FILE* pFile = NULL;
    wchar_t wsPathName[m_uPATHNAME_MAX_LENGTH];
    CreateFullPathFromOutputFilename( wsPathName, L"HashDigest.html" );

    _wfopen_s( &pFile, wsPathName, L"w+,ccs=UTF-8" );

    if (!pFile)
    {
        return false;
    }

    HTMLWriter html( pFile, L"AMD ShaderCache Hash Digest", 0 ); // Refresh the page every 15 seconds

    int tabID = 0;
    html.StartTabTable();
    for (std::list<Shader*>::const_iterator it = i_ShaderList.begin(); it != i_ShaderList.end(); it++)
    {
        Shader* pShader = *it;
        html.AddTab( ++tabID, pShader->m_wsRawFileName );
    }
    html.EndTabTableHeader();

    tabID = 0;
    for (std::list<Shader*>::const_iterator it = i_ShaderList.begin(); it != i_ShaderList.end(); it++)
    {
        Shader* pShader = *it;
        wchar_t wsShaderInfoHTML[16384];
        swprintf_s( wsShaderInfoHTML, L"Raw Filename: %s<br>\n"
            L"Entry Point: %s<br>\n"
            L"Hashed Filename: %s<br>\n"
            L"ASM Target: %s<br>\n"
#if AMD_SDK_INTERNAL_BUILD
            L"ISA Target: %s<br>\n"
#endif
            L"<br>\n"
            L"Source HLSL File: <a href=\"%s\">%s</a><br>\n"
            L"Preprocess File: <a href=\"%s\">%s</a><br>\n"
            L"Assembly File: <a href=\"%s\">%s</a><br>\n"
            L"Object File: <a href=\"%s\">%s</a><br>\n"
            L"Error File: <a href=\"%s\">%s</a><br>\n"
            L"Hash File: <a href=\"%s\">%s</a><br>\n"
#if AMD_SDK_INTERNAL_BUILD
            L"ISA File: <a href=\"%s\">%s</a><br>\n"
            L"ISA Dir: <a href=\"Shaders\\ScDev\\%s\\%s\">Shaders\\ScDev\\%s\\%s</a><br>\n"
#endif
            L"<br>\n"
            L"Compile Status: %s<br>\n"
            L"Compile Timing: %i<br>\n"
            L"Processing?: %s<br>\n"
            L"Up-to-date?: %s<br>\n"
#if AMD_SDK_INTERNAL_BUILD
            L"ISA Generated: %s<br>\n"
            L"<br>\n"
            L"GPR Pressure: <br>\n"
            L"Current  VGPR: %i SGPR: %i<br>\n"
            L"Previous VGPR: %i SGPR: %i<br>\n"
            L"ALU Packing: %f<br>\n"
            L"GPR Pool Size: %i<br>\n"
            L"[Prev] ALU Packing: %f<br>\n"
            L"[Prev] GPR Pool Size: %i<br>\n"
#endif
            L"<br>\n"
            L"Macros: <br>\n"
            ,
            pShader->m_wsRawFileName,
            pShader->m_wsEntryPoint,
            pShader->m_wsHashedFileName,
            pShader->m_wsTarget,
#if AMD_SDK_INTERNAL_BUILD
            AmdTargetInfo[pShader->m_eISATarget].m_Name,
#endif
            pShader->m_wsSourceFile, pShader->m_wsSourceFile,
            pShader->m_wsPreprocessFile, pShader->m_wsPreprocessFile,
            pShader->m_wsAssemblyFileWithHashedFilename, pShader->m_wsAssemblyFileWithHashedFilename,
            pShader->m_wsObjectFile, pShader->m_wsObjectFile,
            pShader->m_wsErrorFile, pShader->m_wsErrorFile,
            pShader->m_wsHashFile, pShader->m_wsHashFile,
#if AMD_SDK_INTERNAL_BUILD
            pShader->m_wsISAFile, pShader->m_wsISAFile,
            AmdTargetInfo[pShader->m_eISATarget].m_Name, pShader->m_wsHashedFileName, AmdTargetInfo[pShader->m_eISATarget].m_Name, pShader->m_wsHashedFileName,
#endif
            pShader->m_wsCompileStatus,
            pShader->m_iCompileWaitCount,
            pShader->m_bBeingProcessed ? L"yes" : L"no",
            pShader->m_bShaderUpToDate ? L"yes" : L"no"
#if AMD_SDK_INTERNAL_BUILD
            , pShader->m_bGPRsUpToDate ? L"yes" : L"no",
            pShader->m_ISA_VGPRs,
            pShader->m_ISA_SGPRs,
            pShader->m_previous_ISA_VGPRs,
            pShader->m_previous_ISA_SGPRs,
            pShader->m_ISA_ALUPacking,
            pShader->m_ISA_GPRPoolSize,
            pShader->m_previous_ISA_ALUPacking,
            pShader->m_previous_ISA_GPRPoolSize
#endif
            );

        // Append Macros to String
        for (unsigned int i = 0; i < pShader->m_uNumMacros; ++i)
        {
            wchar_t* wsMacroStart = &(wsShaderInfoHTML[wcslen( wsShaderInfoHTML )]);
            swprintf_s( wsMacroStart, m_uMACRO_MAX_LENGTH, L"%s: %i<br>\n", pShader->m_pMacros[i].m_wsName, pShader->m_pMacros[i].m_iValue );
        }

        html.AddTabBody( ++tabID, pShader->m_wsRawFileName, wsShaderInfoHTML );
    }

    html.EndTabTable();

    // File Table:

    html.writeHTML( L"<font face=\"Calibri\" size=\"-3\">\n" );
    // Error List

    if (m_ErrorList.size() > 0)
    {
        html.writeHTML( L"<br><h1><center><font color=\"FF0000\">Error List</font></center></h1><br>\n" );
        for (std::set<Shader*>::const_iterator it = m_ErrorList.begin(); it != m_ErrorList.end(); it++)
        {
            Shader* pShader = *it;
            wchar_t wsShaderInfoHTML[16384];
            swprintf_s( wsShaderInfoHTML, L"%s::%s <a href=\"%s\">%s</a> <a href=\"%s\">%s</a><br>\n",
                pShader->m_wsRawFileName, pShader->m_wsEntryPoint, pShader->m_wsSourceFile, L"Source HLSL", pShader->m_wsErrorFile, L"Errors" );
            html.writeHTML( wsShaderInfoHTML );
        }
        if (m_bHasShaderErrorsToDisplay)
        {
            html.writeHTML( L"<font color=\"FF0000\">\n" );
            std::wstring shaderErrors( m_wsLastShaderError );
            size_t offset = 0;
            do
            {
                offset = shaderErrors.find_first_of( L'\n' );
                if (offset != shaderErrors.npos)
                {
                    shaderErrors.replace( offset, 1, L"<br>" );
                }
            } while (offset != shaderErrors.npos);

            html.writeHTML( shaderErrors.c_str() );
            html.writeHTML( L"</font>\n" );
        }
    }

    html.writeHTML( L"<br><h1><center>File Table</center></h1><br>\n" );
    html.writeHTML( L"<table><tr><td>Raw Filename</td><td>Filename Hash</td><td>Assembly File</td><td>Error File</td><td>Hash File</td>" );
    html.writeHTML( L"<td>ISA File</td><td>Object File</td><td>Preprocess File</td></tr>\n" );

    for (std::list<Shader*>::const_iterator it = i_ShaderList.begin(); it != i_ShaderList.end(); it++)
    {
        Shader* pShader = *it;
        html.writeHTML( L"\n\n<tr>" );

        html.AddFileTableRow( pShader->m_wsRawFileName, false );
        html.AddFileTableRow( pShader->m_wsHashedFileName, false );
        html.AddFileTableRow( pShader->m_wsAssemblyFileWithHashedFilename );
        html.AddFileTableRow( pShader->m_wsErrorFile );
        html.AddFileTableRow( pShader->m_wsHashFile );
        html.AddFileTableRow( pShader->m_wsISAFile );
        html.AddFileTableRow( pShader->m_wsObjectFile );
        html.AddFileTableRow( pShader->m_wsPreprocessFile );

        html.writeHTML( L"</tr>\n\n" );

    }

    html.writeHTML( L"</table>\n" );
    html.writeHTML( L"</font>\n" );

    html.FinishHTML();
    fclose( pFile );
    return true;
}


//--------------------------------------------------------------------------------------
// Preprocesses shaders in the list, and generates a hash file, this is subsequently used to
// determine if a shader has changed
//--------------------------------------------------------------------------------------
void ShaderCache::PreprocessShaders()
{
    Shader* pShader = NULL;
    unsigned int uNumWorkThreads = 0;

    // Create Hash Digest File
    bool compileStatusInitialized = false;
    /*if( m_bCreateHashDigest )
    {
    compileStatusInitialized = CreateHashDigest( m_PreprocessList );
    }*/

    // Setup Progress Info and Compile Status for all shaders
    for (std::list<Shader*>::iterator it = m_PreprocessList.begin(); it != m_PreprocessList.end(); it++)
    {
        pShader = *it;
        pShader->m_wsCompileStatus = L"Preparing to pre-process . . ."; // Starting to Process the Shader
        pShader->m_bBeingProcessed = false;
        if (!compileStatusInitialized) { m_pProgressInfo[m_uProgressCounter++] = pShader; } // Add this if Hash Digest hasn't already done it!
    }

    while (m_PreprocessList.size())
    {
        bool bRemove = false;

        for (std::list<Shader*>::iterator it = m_PreprocessList.begin(); it != m_PreprocessList.end(); it++)
        {
            if (bRemove)
            {
                m_PreprocessList.remove( pShader );
                bRemove = false;
            }

            pShader = *it;

            if (uNumWorkThreads < m_uNumCPUCoresToUse)
            {
                // LAYLAFIXED: If we have 0 worker threads, then it implies we can't be processing a shader (so when (uNumWorkThreads == 0 ) && (m_bBeingProcessed == true), we have a bug!)
                assert( (pShader->m_bBeingProcessed == false) || (uNumWorkThreads > 0) );
                if ((pShader->m_bBeingProcessed == false) /*|| (uNumWorkThreads == 0)*/)
                {
                    bRemove = true;
                    pShader->m_wsCompileStatus = L"Finding Shader"; // Starting to PreProcess the Shader
                    if (CheckShaderFile( pShader ))
                    {
                        PreprocessShader( pShader );
                        pShader->m_wsCompileStatus = L"Preprocessing"; // Starting to PreProcess the Shader
                        //pShader->m_wsPreprocessFile_with_ISA

                        pShader->m_bBeingProcessed = true;

                        m_HashList.push_back( pShader );

                        uNumWorkThreads++;
                    }
                    else
                    {
                        pShader->m_wsCompileStatus = L"ERROR: Shader Not Found!";
                        pShader->m_bBeingProcessed = false;
                        --uNumWorkThreads;
                        continue;
                    }
                }
            }
            else
            {
                // else break out of the for loop, and let this batch of work finish
                break;
            }

            Sleep( 1 );
        }

        if (bRemove)
        {
            m_PreprocessList.remove( pShader );
            bRemove = false;
        }

        if (m_bAbort)
        {
            break;
        }

        // Wait for current batch of preprocessing to finish
        {
            HANDLE handles[MAXIMUM_WAIT_OBJECTS];
            DWORD nHandleCount = 0;
            for (std::list<Shader*>::iterator it = m_HashList.begin(); it != m_HashList.end(); it++)
            {
                pShader = *it;
                if (nHandleCount < MAXIMUM_WAIT_OBJECTS)
                {
                    handles[nHandleCount++] = pShader->m_hCompileProcessHandle;
                }
                if (nHandleCount == MAXIMUM_WAIT_OBJECTS)
                {
                    WaitForMultipleObjects( nHandleCount, handles, TRUE, INFINITE );
                    nHandleCount = 0;
                }
            }
            if (nHandleCount > 0)
            {
                WaitForMultipleObjects( nHandleCount, handles, TRUE, INFINITE );
            }
        }

        // Close handles for current batch of preprocessing that is now finished
        {
            for (std::list<Shader*>::iterator it = m_HashList.begin(); it != m_HashList.end(); it++)
            {
                pShader = *it;
                CloseHandle( pShader->m_hCompileProcessHandle );
                CloseHandle( pShader->m_hCompileThreadHandle );
                pShader->m_hCompileProcessHandle = NULL;
                pShader->m_hCompileThreadHandle = NULL;
            }
        }

        // Hash Preprocessed Shaders
        while (m_HashList.size() && (!m_bAbort))
        {
            bRemove = false;

            for (std::list<Shader*>::iterator it = m_HashList.begin(); it != m_HashList.end(); it++)
            {
                if (bRemove)
                {
                    pShader->m_iCompileWaitCount = -1;
                    m_HashList.remove( pShader );
                    bRemove = false;
                }

                pShader = *it;

                assert( pShader->m_bBeingProcessed == true );
                pShader->m_wsCompileStatus = L"Waiting for Preprocessor";
                pShader->m_iCompileWaitCount++;

                assert( pShader->m_hCompileProcessHandle == NULL );

                /*pShader->m_iCompileWaitCount = 0;

                bool bKeepLooping = true;
                while( bKeepLooping )
                {
                unsigned long exitCode = 0;
                int rValue = GetExitCodeProcess(pShader->m_hCompileProcessHandle, &exitCode );
                bKeepLooping = ( (rValue != 0) && (exitCode == STILL_ACTIVE) );
                pShader->m_iCompileWaitCount++;
                }*/

                if ((pShader->m_bBeingProcessed == true) && CreateHashFromPreprocessFile( pShader ))
                {
                    // Set Status to COMPARING HASH
                    pShader->m_wsCompileStatus = L"Comparing Hash";
                    //m_pProgressInfo[m_uProgressCounter++] = pShader;
                    //m_pProgressInfo[m_uProgressCounter++].m_wsFilename = pShader->m_wsPreprocessFile_with_ISA;

                    if (!CompareHash( pShader ))
                    {
                        DeleteObjectFile( pShader );

                        WriteHashFile( pShader );

                        m_CompileList.push_back( pShader );
                    }
                    else
                    {
                        if (CheckObjectFile( pShader ))
                        {
                            m_CreateList.push_back( pShader );
                        }
                        else
                        {
                            m_CompileList.push_back( pShader );
                        }
                    }

                    // Set Status to FINISHED
                    pShader->m_wsCompileStatus = L"Finished Preprocessing";
                    //m_pProgressInfo[m_uProgressCounter++] = pShader;
                    //m_pProgressInfo[m_uProgressCounter++].m_wsFilename = pShader->m_wsPreprocessFile_with_ISA;

                    pShader->m_bBeingProcessed = false;

                    if (uNumWorkThreads > 0)
                    {
                        uNumWorkThreads--;
                    }

                    bRemove = true;
                }
                else
                {
                    Sleep( 1 );
                }
                /*else if( !bRemove )
                {
                // The Preprocess File Doesn't Exist; try again?
                pShader->m_bBeingProcessed = false;

                if ( uNumWorkThreads > 0 )
                uNumWorkThreads--;

                m_PreprocessList.push_back( pShader );

                // SET STATUS TO RETRY!
                pShader->m_wsCompileStatus = L"Preprocessing: RETRY";

                bRemove = true;
                //m_pProgressInfo[m_uProgressCounter++] = pShader;
                //m_pProgressInfo[m_uProgressCounter++].m_wsFilename = pShader->m_wsPreprocessFile_with_ISA;
                }*/
            }

            if (bRemove)
            {
                m_HashList.remove( pShader );
            }

            if (m_bAbort)
            {
                break;
            }

            Sleep( 1 );

        }

    }

}

// a binary predicate implemented as a function:
bool shader_duplicate_ptr( AMD::ShaderCache::Shader* pFirst, AMD::ShaderCache::Shader* pSecond )
{
    return (pFirst == pSecond);
}

//--------------------------------------------------------------------------------------
// Compiles the shaders in the list
//--------------------------------------------------------------------------------------
void ShaderCache::CompileShaders()
{
    Shader* pShader = NULL;
    unsigned int uNumWorkThreads = 0;

    EnterCriticalSection( &m_CompileShaders_CriticalSection );

    while (m_CompileList.size())
    {
        bool bRemove = false;

        Sleep( 1 );

        for (std::list<Shader*>::iterator it = m_CompileList.begin(); it != m_CompileList.end(); it++)
        {

            Sleep( 1 );
            if (bRemove)
            {
                m_CompileList.remove( pShader );
                bRemove = false;
            }

            pShader = *it;

            pShader->m_wsCompileStatus = L"Waiting to Compile...";

            if (uNumWorkThreads < m_uNumCPUCoresToUse)
            {
                if (pShader->m_bBeingProcessed == false)
                {
                    bRemove = true;
                    pShader->m_wsCompileStatus = L"Compiling Shader";
                    CompileShader( pShader );

                    pShader->m_bBeingProcessed = true;

                    m_CompileCheckList.push_back( pShader );

                    uNumWorkThreads++;
                }
            }
            else
            {
                // else break out of the for loop, and let this batch of work finish
                break;
            }
        }

        if (bRemove)
        {
            m_CompileList.remove( pShader );
            bRemove = false;
        }

        if (m_bAbort)
        {
            break;
        }

        // Wait for current batch of compiling to finish
        {
            HANDLE handles[MAXIMUM_WAIT_OBJECTS];
            DWORD nHandleCount = 0;
            for (std::list<Shader*>::iterator it = m_CompileCheckList.begin(); it != m_CompileCheckList.end(); it++)
            {
                pShader = *it;
                if (nHandleCount < MAXIMUM_WAIT_OBJECTS)
                {
                    handles[nHandleCount++] = pShader->m_hCompileProcessHandle;
                }
                if (nHandleCount == MAXIMUM_WAIT_OBJECTS)
                {
                    WaitForMultipleObjects( nHandleCount, handles, TRUE, INFINITE );
                    nHandleCount = 0;
                }
            }
            if (nHandleCount > 0)
            {
                WaitForMultipleObjects( nHandleCount, handles, TRUE, INFINITE );
            }
        }

        // Close handles for current batch of compiling that is now finished
        {
            for (std::list<Shader*>::iterator it = m_CompileCheckList.begin(); it != m_CompileCheckList.end(); it++)
            {
                pShader = *it;
                CloseHandle( pShader->m_hCompileProcessHandle );
                CloseHandle( pShader->m_hCompileThreadHandle );
                pShader->m_hCompileProcessHandle = NULL;
                pShader->m_hCompileThreadHandle = NULL;
            }
        }

        // Check Compiled Shaders
        while (m_CompileCheckList.size() && (!m_bAbort))
        {
            bRemove = false;

            for (std::list<Shader*>::iterator it = m_CompileCheckList.begin(); it != m_CompileCheckList.end(); it++)
            {
                if (bRemove)
                {
                    m_CompileCheckList.remove( pShader );
                    bRemove = false;
                }

                pShader = *it;

                bool bHasObjectFile = false;

                //pShader->m_wsCompileStatus = L"Waiting for Object File . . .";
                if (CheckObjectFile( pShader ))
                {
                    pShader->m_wsCompileStatus = L"Found Object File";
                    //m_pProgressInfo[m_uProgressCounter++].m_wsFilename = pShader->m_wsObjectFile_with_ISA;

                    m_CreateList.push_back( pShader );

                    bHasObjectFile = true;

                    pShader->m_bBeingProcessed = false;

                    if (uNumWorkThreads > 0)
                    {
                        uNumWorkThreads--;
                    }

                    bRemove = true;
                }

                //pShader->m_wsCompileStatus = L"Checking For Errors . . .";
                bool bShaderHasCompilerError = false;
                if ( /*bHasObjectFile && */CheckErrorFile( pShader, bShaderHasCompilerError ))
                {
                    bRemove |= bShaderHasCompilerError;

                    if (bHasObjectFile && !bShaderHasCompilerError)
                    {
                        if (m_bGenerateShaderISA)
                        {
                            pShader->m_wsCompileStatus = L"Generating ISA";
                            pShader->m_bShaderUpToDate = false; // Shader Has Been Updated
                            if (GenerateShaderISA( pShader, false ))
                            {
                                pShader->m_wsCompileStatus = L"Done!";
                            }
                        }
                        else
                        {
                            pShader->m_wsCompileStatus = L"Done!";
                            pShader->m_bShaderUpToDate = false; // Shader Has Been Updated
                        }
                    }
                    else if (bShaderHasCompilerError)
                    {
                        pShader->m_bShaderUpToDate = true;
                        pShader->m_bGPRsUpToDate = true;
                        m_ErrorList.insert( pShader );
                        pShader->m_wsCompileStatus = L"Compiler Error!";
                        if (uNumWorkThreads > 0)
                        {
                            uNumWorkThreads--;
                        }
                    }
                    else
                    {
                        pShader->m_wsCompileStatus = L"Still Compiling . . .";
                    }
                }
                else
                {
                    Sleep( 1 );
                }
            }

            if (bRemove)
            {
                m_CompileCheckList.remove( pShader );
            }

            if (m_bAbort)
            {
                break;
            }

            Sleep( 1 );

        }
    }

    GenerateShaderGPRUsageFromISAForAllShaders(); // Generate GPR Usage for any shaders that still need updating

    LeaveCriticalSection( &m_CompileShaders_CriticalSection );

    if (m_bCreateHashDigest)
    {
        CreateHashDigest( m_CreateList );
    }
}


//--------------------------------------------------------------------------------------
// Creates the shaders in the list
//--------------------------------------------------------------------------------------
HRESULT ShaderCache::CreateShaders()
{
    HRESULT hr = E_FAIL;
    Shader* pShader = NULL;

    for (std::list<Shader*>::iterator it = m_CreateList.begin(); it != m_CreateList.end(); it++)
    {
        pShader = *it;

        if (pShader->m_ppShader)
        {
            if (NULL == *(pShader->m_ppShader) || (!pShader->m_bShaderUpToDate))
            {
                assert( (!pShader->m_bShaderUpToDate) || (NULL != *(pShader->m_ppShader)) );
                hr = CreateShader( pShader );
                assert( S_OK == hr );
            }
        } // Else, this is a cloned shader, and we won't be using it for rendering, so don't initialize it.
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Invalidates the shaders in the list
//--------------------------------------------------------------------------------------
void ShaderCache::InvalidateShaders( void )
{
    Shader* pShader = NULL;

    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        pShader = *it;
        pShader->m_bShaderUpToDate = false;
        //assert( pShader->m_ppShader == NULL );
    }
}

//--------------------------------------------------------------------------------------
// The preprocess file generated by fxc can have the full path to the source file in it.
// Strip that out before you create the hash.
//--------------------------------------------------------------------------------------
void ShaderCache::StripPathInfoFromPreprocessFile( Shader* pShader, FILE* pFile, char* pFileBufDst, int iFileSize )
{
    // clear out the destination buffer
    memset( pFileBufDst, '\0', iFileSize );

    // make a plain old char version of our source filename
    size_t i;
    char szSourceFileWithBackSlashes[m_uFILENAME_MAX_LENGTH];
    memset( szSourceFileWithBackSlashes, '\0', sizeof( char[m_uFILENAME_MAX_LENGTH] ) );
    wcstombs_s( &i, szSourceFileWithBackSlashes, m_uFILENAME_MAX_LENGTH, pShader->m_wsSourceFile, m_uFILENAME_MAX_LENGTH );

    // fxc accepts both forward and back slashes, so convert any forward
    // slashes to back slashes for consistency
    char* pForwardSlash = strchr( szSourceFileWithBackSlashes, L'/' );
    while (pForwardSlash)
    {
        *pForwardSlash = '\\';
        pForwardSlash = strchr( szSourceFileWithBackSlashes, L'/' );
    }

    // now that we know we have back slashes (if we have slashes at all),
    // find the last one to strip off any path info
    char* pFileName = strrchr( szSourceFileWithBackSlashes, '\\' );

    // support the case where the source filename doesn't contain any path info
    // (this would happen if shaders were in the project working directory instead of
    // a subfolder)
    if (!pFileName)
    {
        pFileName = szSourceFileWithBackSlashes;
    }
    else
    {
        // if there was a slash, you are now pointing to it,
        // so go one past it to get just the filename
        pFileName++;
    }


    char szLine[m_uCOMMAND_LINE_MAX_LENGTH];
    char* pLine = szLine;
    char szFxcLineDirective[32];
    strcpy_s( szFxcLineDirective, 32, "#line" );

    while (fgets( pLine, m_uCOMMAND_LINE_MAX_LENGTH, pFile ))
    {
        // check if this is a line directive
        char* pStartOfFxcLineDirective = strstr( pLine, szFxcLineDirective );
        if (pStartOfFxcLineDirective)
        {
            // if it is, then check if the filename appears after #line
            if (!strstr( pStartOfFxcLineDirective, pFileName ))
            {
                // if it is a line directive, but not one containing the filename,
                // copy it over to the destination buffer
                strcat_s( pFileBufDst, iFileSize, pLine );
            }
            // else, assume it is one of the problematic #line directives
            // that contains full path info, and skip it
        }
        else
        {
            // else, not a line directive, so copy it over to the destination buffer
            strcat_s( pFileBufDst, iFileSize, pLine );
        }
    }
}


//--------------------------------------------------------------------------------------
// Creates a hash from a given shader
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CreateHashFromPreprocessFile( Shader* pShader )
{
    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsPreprocessFile );

    const unsigned int kuMaxPath = AMD::ShaderCache::m_uPATHNAME_MAX_LENGTH;
    const size_t kPathLength = wcslen( wsShaderPathName );
    if (kPathLength >= kuMaxPath)
    {
        wchar_t wsErrorText[m_uCOMMAND_LINE_MAX_LENGTH];
        swprintf_s( wsErrorText, L"Error: PATH LENGTH TOO LONG [%u/%u]\n\n%s", (unsigned)kPathLength, kuMaxPath, wsShaderPathName );
        MessageBoxW( NULL, wsErrorText, L"ERROR: Path Length Too Long", MB_OK );
        DebugBreak();
    }

    _wfopen_s( &pFile, wsShaderPathName, L"rt" );

    if (pFile)
    {
        fseek( pFile, 0, SEEK_END );
        int iFileSize = ftell( pFile );
        rewind( pFile );
        char* pFileBuf = new char[iFileSize];

        // Strip path info from the preprocessed file, as otherwise this causes problems
        // if you move a project on disk. Without this, it triggers a full rebuild of the
        // shader cache, purely because the path has changed
        StripPathInfoFromPreprocessFile( pShader, pFile, pFileBuf, iFileSize );

        if (NULL != pShader->m_pHash)
        {
            free( pShader->m_pHash );
            pShader->m_pHash = NULL;
            pShader->m_uHashLength = 0;
        }

        CreateHash( pFileBuf, iFileSize, &pShader->m_pHash, &pShader->m_uHashLength );

        delete [] pFileBuf;
        fclose( pFile );

        return TRUE;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Creates a hash for the shader filename
//--------------------------------------------------------------------------------------
void ShaderCache::Shader::SetupHashedFilename( void )
{

    if (NULL != m_pFilenameHash)
    {
        free( m_pFilenameHash );
        m_pFilenameHash = NULL;
        m_uFilenameHashLength = 0;
    }

    // TODO: Convert into URL-Safe String
    // Convert filename from wchar_t to char*
    size_t i;
    char asciiString[m_uPATHNAME_MAX_LENGTH];
    memset( asciiString, '\0', sizeof( char[m_uPATHNAME_MAX_LENGTH] ) );
    wcstombs_s( &i, asciiString, m_uPATHNAME_MAX_LENGTH, m_wsRawFileName, m_uPATHNAME_MAX_LENGTH );
    CreateHash( asciiString, 0, &m_pFilenameHash, &m_uFilenameHashLength );
    swprintf_s( m_wsHashedFileName, L"%x", *reinterpret_cast<unsigned long *>(m_pFilenameHash) );
    assert( m_uFilenameHashLength == 16 );

}


//--------------------------------------------------------------------------------------
// Creates the hash
//--------------------------------------------------------------------------------------
void ShaderCache::CreateHash( const char* data, int iFileSize, BYTE** hash, long* len )
{
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE *pbHash = NULL;
    DWORD dwHashLen = 0;

    BYTE * pbBuffer = NULL;
    DWORD dwCount = 0;
    DWORD i = 0;
    size_t bufLen = 0;

    // DwFlags is set to zero to attempt to open an existing key container.
    if (!CryptAcquireContext( &hProv, NULL, NULL, PROV_RSA_FULL, 0 ))
    {
        // An error occurred in acquiring the context. This could mean that
        // the key container requested does not exist. In this case, the
        // function can be called again to attempt to create a new key container.
        if (GetLastError() == NTE_BAD_KEYSET)
        {
            if (!CryptAcquireContext( &hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET ))
            {
                return;
            }
        }
    }
    if (!CryptCreateHash( hProv, CALG_MD5, 0, 0, &hHash ))
    {
        return;
    }

    bufLen = strlen( data );

    pbBuffer = (BYTE*)malloc( bufLen + 1 );
    memset( pbBuffer, 0, bufLen + 1 );

    for (i = 0; i < bufLen; i++)
    {
        pbBuffer[i] = (BYTE)data[i];
    }

    if (!CryptHashData( hHash, pbBuffer, (DWORD)bufLen, 0 ))
    {
        return;
    }

    dwCount = sizeof( DWORD );
    if (!CryptGetHashParam( hHash, HP_HASHSIZE, (BYTE *)&dwHashLen, &dwCount, 0 ))
    {
        return;
    }

    if ((pbHash = (unsigned char*)malloc( dwHashLen )) == NULL)
    {
        return;
    }

    memset( pbHash, 0, dwHashLen );

    if (!CryptGetHashParam( hHash, HP_HASHVAL, pbHash, &dwHashLen, 0 ))
    {
        return;
    }

    *hash = pbHash;
    *len = dwHashLen;

    if (hHash)
    {
        CryptDestroyHash( hHash );
    }

    if (hProv)
    {
        CryptReleaseContext( hProv, 0 );
    }

    if (NULL != pbBuffer)
    {
        free( pbBuffer );
        pbBuffer = NULL;
    }
}


//--------------------------------------------------------------------------------------
// Writes out the hash file to disk
//--------------------------------------------------------------------------------------
void ShaderCache::WriteHashFile( Shader* pShader )
{
    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsHashFile );

    _wfopen_s( &pFile, wsShaderPathName, L"wb" );

    if (pFile)
    {
        fwrite( pShader->m_pHash, pShader->m_uHashLength, 1, pFile );

        fclose( pFile );
    }
}


//--------------------------------------------------------------------------------------
// Compares a shaders hash with the has file on disk
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CompareHash( Shader* pShader )
{
    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsHashFile );

    _wfopen_s( &pFile, wsShaderPathName, L"rb" );

    if (pFile)
    {
        fseek( pFile, 0, SEEK_END );
        int iFileSize = ftell( pFile );
        rewind( pFile );
        BYTE* pFileBuf = new BYTE[iFileSize];

        fread( pFileBuf, 1, iFileSize, pFile );

        fclose( pFile );

        if (!memcmp( pShader->m_pHash, pFileBuf, pShader->m_uHashLength ))
        {
            delete [] pFileBuf;
            return TRUE;
        }

        delete [] pFileBuf;
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
// Creates a shader
//--------------------------------------------------------------------------------------
HRESULT ShaderCache::CreateShader( Shader* pShader )
{
    HRESULT hr = E_FAIL;
    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    assert( !pShader->m_bShaderUpToDate );
    ID3D11DeviceChild* pTempD3DShader = *pShader->m_ppShader;
    *pShader->m_ppShader = NULL;

    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsObjectFile );

    _wfopen_s( &pFile, wsShaderPathName, L"rb" );

    if (pFile)
    {
        fseek( pFile, 0, SEEK_END );
        int iFileSize = ftell( pFile );
        rewind( pFile );
        char* pFileBuf = new char[iFileSize];
        fread( pFileBuf, 1, iFileSize, pFile );

        switch (pShader->m_eShaderType)
        {
        case SHADER_TYPE_VERTEX:
            hr = DXUTGetD3D11Device()->CreateVertexShader( pFileBuf, iFileSize, NULL, (ID3D11VertexShader**)pShader->m_ppShader );
            assert( S_OK == hr );
            if (pShader->m_uNumDescElements && (pTempD3DShader == NULL))
            { // Only create the Input Layout if one doesn't already exist (it shouldn't change at runtime... I *think*)
                hr = DXUTGetD3D11Device()->CreateInputLayout( pShader->m_pInputLayoutDesc, pShader->m_uNumDescElements, pFileBuf, iFileSize, pShader->m_ppInputLayout );
            }
            break;
        case SHADER_TYPE_HULL:
            hr = DXUTGetD3D11Device()->CreateHullShader( pFileBuf, iFileSize, NULL, (ID3D11HullShader**)pShader->m_ppShader );
            assert( S_OK == hr );
            break;
        case SHADER_TYPE_DOMAIN:
            hr = DXUTGetD3D11Device()->CreateDomainShader( pFileBuf, iFileSize, NULL, (ID3D11DomainShader**)pShader->m_ppShader );
            assert( S_OK == hr );
            break;
        case SHADER_TYPE_GEOMETRY:
            hr = DXUTGetD3D11Device()->CreateGeometryShader( pFileBuf, iFileSize, NULL, (ID3D11GeometryShader**)pShader->m_ppShader );
            assert( S_OK == hr );
            break;
        case SHADER_TYPE_PIXEL:
            hr = DXUTGetD3D11Device()->CreatePixelShader( pFileBuf, iFileSize, NULL, (ID3D11PixelShader**)pShader->m_ppShader );
            assert( S_OK == hr );
            break;
        case SHADER_TYPE_COMPUTE:
            hr = DXUTGetD3D11Device()->CreateComputeShader( pFileBuf, iFileSize, NULL, (ID3D11ComputeShader**)pShader->m_ppShader );
            assert( S_OK == hr );
            break;
        }

        delete [] pFileBuf;
        fclose( pFile );
    }

    if (hr == S_OK)
    {
        SAFE_RELEASE( pTempD3DShader ); // Clean up Old Shader
        pShader->m_bShaderUpToDate = true;
    }
    else
    {
        *pShader->m_ppShader = pTempD3DShader; // Restore last known good shader!
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Compiles a shader
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CompileShader( Shader* pShader )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    // Start the child process.
    BOOL bSuccess = CreateProcess( m_wsFxcExePath,   // Application name
        pShader->m_wsCommandLine, // Command line
        NULL,             // Process handle not inheritable
        NULL,             // Thread handle not inheritable
        FALSE,            // Set handle inheritance to FALSE
        CREATE_NO_WINDOW, // Don't make a console window
        NULL,             // Use parent's environment block
        NULL,             // Use parent's starting directory
        &si,              // Pointer to STARTUPINFO structure
        &pi );            // Pointer to PROCESS_INFORMATION structure

    assert( pShader->m_hCompileProcessHandle == NULL );
    assert( pShader->m_hCompileThreadHandle == NULL );
    pShader->m_hCompileProcessHandle = pi.hProcess;
    pShader->m_hCompileThreadHandle = pi.hThread;

    return bSuccess;
}


//--------------------------------------------------------------------------------------
// Preprocesses a shader
//--------------------------------------------------------------------------------------
BOOL ShaderCache::PreprocessShader( Shader* pShader )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    ZeroMemory( &si, sizeof( si ) );
    si.cb = sizeof( si );
    ZeroMemory( &pi, sizeof( pi ) );

    // Start the child process.
    BOOL bSuccess = CreateProcess( m_wsFxcExePath,   // Application name
        pShader->m_wsPreprocessCommandLine, // Command line
        NULL,             // Process handle not inheritable
        NULL,             // Thread handle not inheritable
        FALSE,            // Set handle inheritance to FALSE
        CREATE_NO_WINDOW, // Don't make a console window
        NULL,             // Use parent's environment block
        NULL,             // Use parent's starting directory
        &si,              // Pointer to STARTUPINFO structure
        &pi );            // Pointer to PROCESS_INFORMATION structure

    assert( pShader->m_hCompileProcessHandle == NULL );
    assert( pShader->m_hCompileThreadHandle == NULL );
    pShader->m_hCompileProcessHandle = pi.hProcess;
    pShader->m_hCompileThreadHandle = pi.hThread;

    return bSuccess;
}

//--------------------------------------------------------------------------------------
// Checks to see if the object file exists for a given shader
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CheckShaderFile( Shader* pShader )
{
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    CreateFullPathFromInputFilename( wsShaderPathName, pShader->m_wsSourceFile );

    if (_waccess( wsShaderPathName, 00 ) != -1)
    {
        return TRUE;
    }
    else
    {
        wchar_t wsErrorText[m_uCOMMAND_LINE_MAX_LENGTH];
        swprintf_s( wsErrorText, L"Error: %s (FILE NOT FOUND)\n", wsShaderPathName );

        const bool kbAppendToError = (wcslen( m_wsLastShaderError ) < (3 * m_uCOMMAND_LINE_MAX_LENGTH));
        swprintf_s( m_wsLastShaderError, L"%s%s", (m_bHasShaderErrorsToDisplay && kbAppendToError) ? m_wsLastShaderError : L"", wsErrorText );
        m_bHasShaderErrorsToDisplay = true;
        m_shaderErrorRenderedCount = 0;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Checks to see if the oject file exists for a given shader
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CheckObjectFile( Shader* pShader )
{
    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsObjectFile );

    _wfopen_s( &pFile, wsShaderPathName, L"rt" );

    if (pFile)
    {
        fseek( pFile, 0, SEEK_END );
        int iFileSize = ftell( pFile );
        fclose( pFile );

        if (iFileSize > 0)
        {
            return TRUE;
        }
    }

    return FALSE;
}


//--------------------------------------------------------------------------------------
// Checks to see if FXC.exe is located correctly
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CheckFXC()
{
    FILE* pFile = NULL;
    _wfopen_s( &pFile, m_wsFxcExePath, L"rb" );

    if (pFile)
    {
        fclose( pFile );

        return TRUE;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Checks to see if Dev.exe is located correctly
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CheckSCDEV()
{
    if (!m_bGenerateShaderISA) { return FALSE; }

    FILE* pFile = NULL;
    _wfopen_s( &pFile, m_wsDevExePath, L"rb" );

    if (pFile)
    {
        fclose( pFile );

        return TRUE;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Checks to see if an error file exists, and opens it if non-zero in size
//--------------------------------------------------------------------------------------
BOOL ShaderCache::CheckErrorFile( Shader* pShader, bool& io_bHasShaderCompilerError )
{
    io_bHasShaderCompilerError = false;
    FILE* pFile = NULL;
    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];

    CreateFullPathFromOutputFilename( wsShaderPathName, pShader->m_wsErrorFile );

    _wfopen_s( &pFile, wsShaderPathName, L"rt" );

    if (pFile)
    {
        fseek( pFile, 0, SEEK_END );
        int iFileSize = ftell( pFile );

        if (iFileSize > 0)
        {
            rewind( pFile );
            if (IsAnError( pFile ))
            {
                io_bHasShaderCompilerError = true;

                {
                    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];
                    swprintf_s( wsShaderPathName, L"\n\n*** Shader Compiler: Errors found in [%s\\%s]\n\n", m_wsWorkingDir, pShader->m_wsErrorFile );
                    OutputDebugStringW( wsShaderPathName );
                    rewind( pFile );
                    PrintShaderErrors( pFile );
                }

                DeleteHashFile( pShader );

                if (m_ErrorDisplayType == ERROR_DISPLAY_IN_MESSAGE_BOX)
                {
                    wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];
                    swprintf_s( wsShaderPathName, L"*** The HLSL Shader Compiler has found the following ERROR(s) ***" );
                    MessageBoxW( NULL, m_wsLastShaderError, wsShaderPathName, MB_OK );
                }
                else if (m_ErrorDisplayType == ERROR_DISPLAY_IN_DEBUG_OUTPUT_AND_BREAK)
                {
                    DebugBreak();
                }

            }
        }

        fclose( pFile );

        return TRUE;
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Renders the progress of the shader generation process
//--------------------------------------------------------------------------------------
void ShaderCache::RenderShaderErrors( CDXUTTextHelper* g_pTxtHelper, int iFontHeight, DirectX::XMVECTOR FontColor, const unsigned int ki_FrameTimeout )
{
    wchar_t wsOverallProgress[m_uPATHNAME_MAX_LENGTH];

    if (!m_bHasShaderErrorsToDisplay)
    {
        return;
    }

    if (ShowShaderErrors())
    {
        g_pTxtHelper->Begin();
        g_pTxtHelper->SetForegroundColor( FontColor );
        g_pTxtHelper->SetInsertionPos( 5, 60 );

        // 2500 frames @ 500 fps = ~5 seconds
        const unsigned int ki_FramesToDisplayErrorsFor = ki_FrameTimeout;
        if (m_bHasShaderErrorsToDisplay && (m_shaderErrorRenderedCount < ki_FramesToDisplayErrorsFor))
        {
            swprintf_s( wsOverallProgress, L"*** Shader Compiler Generated ERROR(s) -- this notification will auto-hide in %i frames ***", (ki_FramesToDisplayErrorsFor - m_shaderErrorRenderedCount) );
            g_pTxtHelper->DrawTextLine( wsOverallProgress );
            g_pTxtHelper->DrawTextLine( m_wsLastShaderError );
            ++m_shaderErrorRenderedCount;
        }
        else
        {
            // Error time limit expired (this is in frames not ms, should change to ms!)
            m_bHasShaderErrorsToDisplay = false;
            m_shaderErrorRenderedCount = 0;
            swprintf_s( m_wsLastShaderError, L"*** ShaderCompiler: 0 Shader Errors ***\n" );
        }

        g_pTxtHelper->End();
    }

    else if (m_ErrorDisplayType == ERROR_DISPLAY_IN_MESSAGE_BOX)
    {
        m_bHasShaderErrorsToDisplay = false;
        m_shaderErrorRenderedCount = 0;
        wchar_t wsShaderPathName[m_uPATHNAME_MAX_LENGTH];
        swprintf_s( wsShaderPathName, L"*** ERROR(s) Occured ***" );
        MessageBoxW( NULL, m_wsLastShaderError, wsShaderPathName, MB_OK );
    }

    else if (m_ErrorDisplayType == ERROR_DISPLAY_IN_DEBUG_OUTPUT_AND_BREAK)
    {
        m_bHasShaderErrorsToDisplay = false;
        m_shaderErrorRenderedCount = 0;
        DebugBreak();
    }

}

//--------------------------------------------------------------------------------------
// Prints the error message to debug output, and copies it to the last shader error variable
//--------------------------------------------------------------------------------------
void ShaderCache::PrintShaderErrors( FILE* pFile )
{
    wchar_t szLine[m_uCOMMAND_LINE_MAX_LENGTH];
    wchar_t* pLine = szLine;
    wchar_t szError[32];
    wcscpy_s( szError, 32, L"error" );

    while (fgetws( pLine, m_uCOMMAND_LINE_MAX_LENGTH, pFile ))
    {
        OutputDebugStringW( pLine );

        wchar_t* pTemp = pLine;
        while (*pTemp != L'\n')
        {

            if (!wcsncmp( pTemp, szError, wcslen( szError ) ))
            {
                const bool kbAppendToError = (wcslen( m_wsLastShaderError ) < (3 * m_uCOMMAND_LINE_MAX_LENGTH));
                swprintf_s( m_wsLastShaderError, L"%s%s", (m_bHasShaderErrorsToDisplay && kbAppendToError) ? m_wsLastShaderError : L"", pLine );
                m_bHasShaderErrorsToDisplay = true;
                m_shaderErrorRenderedCount = 0;
                //return TRUE;
            }

            ++pTemp;
        }
    }

    return;

}

//--------------------------------------------------------------------------------------
// Checks an error file to see if there really was an error, rather than just a warning
//--------------------------------------------------------------------------------------
BOOL ShaderCache::IsAnError( FILE* pFile )
{
    char szLine[m_uCOMMAND_LINE_MAX_LENGTH];
    char* pLine = szLine;
    char szError[32];
    strcpy_s( szError, 32, "error" );

    while (fgets( pLine, m_uCOMMAND_LINE_MAX_LENGTH, pFile ))
    {
        char* pTemp = pLine;
        while (*pTemp != '\n')
        {
            if (!strncmp( pTemp, szError, strlen( szError ) ))
            {
                return TRUE;
            }

            pTemp++;
        }
    }

    return FALSE;
}

//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteFileByFilename( const wchar_t* pwsFile ) const
{
    wchar_t wsPathName[m_uPATHNAME_MAX_LENGTH];
    BOOL bDeleted;

    CreateFullPathFromOutputFilename( wsPathName, pwsFile );

    bDeleted = DeleteFile( wsPathName );
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteErrorFiles()
{
    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;

        DeleteErrorFile( pShader );
    }
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteErrorFile( Shader* pShader )
{
    DeleteFileByFilename( pShader->m_wsErrorFile );
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteAssemblyFiles()
{
    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;

        DeleteAssemblyFile( pShader );
    }
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteAssemblyFile( Shader* pShader )
{
    //DeleteFileByFilename( pShader->m_wsAssemblyFile );
    DeleteFileByFilename( pShader->m_wsAssemblyFileWithHashedFilename );
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteObjectFiles()
{
    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;

        DeleteObjectFile( pShader );
    }
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteObjectFile( Shader* pShader )
{
    DeleteFileByFilename( pShader->m_wsObjectFile );
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeletePreprocessFiles()
{
    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;

        DeletePreprocessFile( pShader );
    }
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeletePreprocessFile( Shader* pShader )
{
    DeleteFileByFilename( pShader->m_wsPreprocessFile );
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteHashFiles()
{
    for (std::list<Shader*>::iterator it = m_ShaderList.begin(); it != m_ShaderList.end(); it++)
    {
        Shader* pShader = *it;

        DeleteHashFile( pShader );
    }
}


//--------------------------------------------------------------------------------------
// Deletion utility method
//--------------------------------------------------------------------------------------
void ShaderCache::DeleteHashFile( Shader* pShader )
{
    DeleteFileByFilename( pShader->m_wsHashFile );
}
