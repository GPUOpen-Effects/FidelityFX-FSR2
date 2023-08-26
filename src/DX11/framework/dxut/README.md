![DirectX Logo](https://raw.githubusercontent.com/wiki/Microsoft/DXUT/Dx_logo.GIF)

# DXUT for Direct3D 11

http://go.microsoft.com/fwlink/?LinkId=320437

Copyright (c) Microsoft Corporation.

**December 2, 2021**

DXUT is a "GLUT"-like framework for Direct3D 11.x Win32 desktop applications; primarily samples, demos, and prototypes.

This code is designed to build with Visual Studio 2017 ([15.9](https://walbourn.github.io/vs-2017-15-9-update/)) or Visual Studio 2019. It is recommended that you make use of the Windows 10 May 2020 Update SDK ([19041](https://walbourn.github.io/windows-10-may-2020-update-sdk/)).

These components are designed to work without requiring any content from the legacy DirectX SDK. For details, see [Where is the DirectX SDK?](https://aka.ms/dxsdk).

## Documentation

Documentation is available on the [GitHub wiki](https://github.com/Microsoft/DXUT/wiki).

## Notices

*This project is 'archived'. It is still available for use for legacy projects, but use of it for new projects is not recommended.*

All content and source code for this package are subject to the terms of the [MIT License](http://opensource.org/licenses/MIT).

## Contributing

This project welcomes contributions and suggestions. Most contributions require you to agree to a Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft trademarks or logos is subject to and must follow [Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general). Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship. Any use of third-party trademarks or logos are subject to those third-party's policies.

## Credits

The DXUT library is the work of Shanon Drone, Jason Sandlin, and David Tuft with contributions from David Cook, Kev Gee, Matt Lee, and Chuck Walbourn.

## Samples

* Direct3D Tutorial08 - 10
* BasicHLSL11, EmptyProject11, SimpleSample11
* DXUT+DirectXTK Simple Sample

These are hosted on [GitHub](https://github.com/walbourn/directx-sdk-samples)

## Disclaimer

DXUT is being provided as a porting aid for older code that makes use of the legacy DirectX SDK, the deprecated D3DX9/D3DX11 library, and the DXUT11 framework. It is a cleaned up version of the original DXUT11 that will build with the Windows 8.1 / 10 SDK and does not make use of any legacy DirectX SDK or DirectSetup deployed components.

The DXUT framework is for use in Win32 desktop applications. It not usable for Universal Windows Platform apps, Windows Store apps,
Xbox, or Windows phone.

This version of DXUT only supports Direct3D 11, and therefore is not compatible with Windows XP or early versions of Windows Vista.

## Release Notes

* The VS 2017/2019 projects make use of ``/permissive-`` for improved C++ standard conformance. Use of a Windows 10 SDK prior to the Fall Creators Update (16299) or an Xbox One XDK prior to June 2017 QFE 4 may result in failures due to problems with the system headers. You can work around these by disabling this switch in the project files which is found in the ``<ConformanceMode>`` elements, or in some cases adding ``/Zc:twoPhase-`` to the ``<AdditionalOptions>`` elements.
