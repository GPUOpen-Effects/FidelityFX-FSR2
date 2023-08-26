# DXUT for Direct3D 11

http://go.microsoft.com/fwlink/?LinkId=320437

## Release History

## December 2, 2021
* Minor project update

## June 2, 2021 (11.26)
* Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab
* Minor code review

## February 7, 2021
* Added CMake project
* Removed Windows Vista support
* No code changes

### November 17, 2020 (11.25)
* Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab

### June 3, 2020 (11.24)
* Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab
* Retired VS 2015 projects

### January 16, 2020 (11.23)
* Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab

### April 26, 2019 (11.22)
* Added VS 2019 desktop projects
* VS 2017 updated for Windows 10 October 2018 Update SDK (17763)
* Minor code cleanup

### July 12, 2018 (11.21)
* Code cleanup

### May 31, 2018 (11.20)
* VS 2017 updated for Windows 10 April 2018 Update SDK (17134)

### May 11, 2018 (11.19)
* Support for Direct3D 11.2 no longer requires define ``USE_DIRECT3D11_2``
* Retired VS 2013 projects
* Code cleanup

### February 27, 2018 (11.18)
* Fixed array length mismatch issue with ``TOTAL_FEATURE_LEVELS``
* Fixed optional Direct3D 11.4 support in VS 2013/2015 projects
* Minor code cleanup

### November 2, 2017 (11.17)
* VS 2017 updated for Windows 10 Fall Creators Update SDK (16299)
* Optional support for Direct3D 11.4 (define ``USE_DIRECT3D11_4`` in projects using the 14393 or later Windows 10 SDK)

### October 13, 2017 (11.16)
* Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab
* Updated for VS 2017 update 15.1 - 15.3 and Windows 10 SDK (15063)    

### March 10, 2017 (11.15)
* Add VS 2017 projects
* Minor code cleanup

### September 15, 2016 (11.14)
* Updated WICTextureLoader and ScreenGrab

### August 2, 2016 (11.13)
* Updated for VS 2015 Update 3 and Windows 10 SDK (14393)

### April 26, 2016 (11.12)
* Updated DDSTextureLoader, WICTextureLoader, and ScreenGrab
* Retired VS 2012 projects and obsolete adapter code
* Minor code and project file cleanup

### November 30, 2015 (11.11)
* Updated DDSTextureLoader, ScreenGrab, DXERR
* Updated for VS 2015 Update 1 and Windows 10 SDK (10586)

### July 29, 2015 (11.10)
* Updated for VS 2015 and Windows 10 SDK RTM
* Retired VS 2010 projects

### June 16, 2015 (11.09)
* Optional support for Direct3D 11.3 (define ``USE_DIRECT3D11_3`` in VS 2015 projects)

### April 14, 2015 (11.08)
* Fix for auto-gen of volume textures
* More updates for VS 2015

### November 24, 2014 (11.07)
* Minor fix for Present usage
* Minor fix for CBaseCamera::GetInput
* Minor fix for WIC usage of IWICFormatConverter
* Updates for Visual Studio 2015 Technical Preview

### July 28, 2014 (11.06)
* Optional support for Direct3D 11.2 (define ``USE_DIRECT3D11_2`` in VS 2013 projects)
* Fixes for various UI and F2 device settings dialog issues
* Fixes for device and format enumeration
* Changed default resolution to 800x600
* Code review fixes

### January 24, 2014 (11.05)
* Added use of DXGI debugging when available
* Resolved CRT heap leak report
* Fixed compile bug in DXUTLockFreePipe
* Fixed bug reported in DXUT's sprite implementation
* Code cleanup (removed ``DXGI_1_2_FORMATS`` control define; ScopedObject typedef removed)

### October 21, 2013 (11.04)
* Updated for Visual Studio 2013 and Windows 8.1 SDK RTM
* Minor fixes for systems which only have a "Microsoft Basic Renderer" device

### September 2013 (11.03)
* Removed dependencies on the D3DX9 and D3DX11 libraries, so DXUT no longer requires the legacy DirectX SDK to build.
* It does require the d3dcompiler.h header from the Windows 8.x SDK.
* Includes standalone DDSTextureLoader, WICTexureLoader, ScreenGrab, and DxErr modules.
* Removed support for Direct3D 9 and Windows XP
* Deleted the DXUTDevice9.h/.cpp, SDKSound.h/.cpp, and SDKWaveFile.h/.cpp files
* Deleted legacy support for MCE relaunch
* General C++ code cleanups (nullptr, auto keyword, C++ style casting, Safer CRT, etc.) which are compatible with Visual C++ 2010 and 2012
* SAL2 annotation and /analyze cleanup
* Added DXUTCompileFromFile, DXUTCreateShaderResourceViewFromFile, DXUTCreateTextureFromFile, DXUTSaveTextureToFile helpers
* Added ``-forcewarp`` command-line switch
* Added support for DXGI 1.1 and 1.2 formats
* Added Direct3D 11.1 Device/Context state
* Support Feature Level 11.1 when available

### June 2010 (11.02)
* The DirectX SDK (June 2010) included an update to DXUT11. This is the last version to support Visual Studio 2008, Windows XP, or Direct3D 9. The source code is located in ``Samples\C++\DXUT11``.

### February 2010 (11.01)
* An update was shipped with the DirectX SDK (February 2010). This is the last version to support Visual Studio 2005. The source code is located in ``Samples\C++\DXUT11``.

### August 2009 (11.00)
* The initial release of DXUT11 was in DirectX SDK (August 2009). The source code is located in Samples\C++\DXUT11. This was a port of the original DXUT which supported Direct3D 10 / Direct3D 9 applications on Windows XP and Windows Vista.
