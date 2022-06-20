// This file is part of the FidelityFX SDK.
//
// Copyright (c) 2022 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "ShaderCompiler.h"

namespace FSR2
{
	ShaderCompiler::ShaderCompiler()
	{
	}

	ShaderCompiler::~ShaderCompiler()
	{
		SafeRelease(validator_);
		SafeRelease(compiler_);
		SafeRelease(library_);
		SafeRelease(includeHandler_);
		SafeRelease(linker_);

		if (dll_) {
			FreeLibrary(dll_);
		}
	}

	HRESULT ShaderCompiler::init(const wchar_t* pathDXCompiler_dll, const wchar_t* pathDXIL_dll)
	{
		HRESULT status = S_OK;

		HMODULE dllDXIL = LoadLibraryW(pathDXIL_dll);
		dll_ = LoadLibraryW(pathDXCompiler_dll);
		if (!dll_) {
			//fmt::print(std::cerr, "Cannot load dxcompiler.dll\n");
			return E_FAIL;
		}

		DxcCreateInstanceProc pfnDxcCreateInstance = DxcCreateInstanceProc(GetProcAddress(dll_, "DxcCreateInstance"));
		if (pfnDxcCreateInstance == nullptr)
		{
			//fmt::print(std::cerr, "Failed to DxcCreateInstance address.\n");
			return E_FAIL;
		}

		SafeRelease(validator_);
		if (FAILED(pfnDxcCreateInstance(CLSID_DxcValidator, IID_PPV_ARGS(&validator_)))) {
			//fmt::print(std::cerr, "Failed creating DxcValidator\n");
			status = E_FAIL;
		}

		SafeRelease(compiler_);
		if (FAILED(pfnDxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_)))) {
			//fmt::print(std::cerr, "Failed create DxcCompiler instance.\n");
			status = E_FAIL;
		}

		SafeRelease(library_);
		if (FAILED(pfnDxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library_)))) {
			//fmt::print(std::cerr, "Failed creating DxcLibrary instance.\n");
			status = E_FAIL;
		}

		SafeRelease(reflection_);
		if (FAILED(pfnDxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&reflection_)))) {
			//fmt::print(std::cerr, "Failed creating DxcLibrary instance.\n");
			status = E_FAIL;
		}

		if (FAILED(library_->CreateIncludeHandler(&includeHandler_))) {
			//fmt::print(std::cerr, "Failed creating Dxc include handler.\n");
			status = E_FAIL;
		}

		SafeRelease(linker_);
		if (FAILED(pfnDxcCreateInstance(CLSID_DxcLinker, IID_PPV_ARGS(&linker_)))) {
			//fmt::print(std::cerr, "Failed creating DxcLinker instance.\n");
			status = E_FAIL;
		}

		return status;
	}

	HRESULT ShaderCompiler::validate(IDxcBlob* input, IDxcOperationResult** ppResult, UINT32 flags)
	{
		return validator_->Validate(input, flags, ppResult);
	}

	IDxcContainerReflection* ShaderCompiler::dgbReflection()
	{
		return reflection_;
	}

	HRESULT ShaderCompiler::compile(LPCWSTR FilePath, IDxcBlobEncoding* source, ShaderCompilationDesc* desc, IDxcBlob** ppResult)
	{
		HRESULT hr = E_FAIL;
		std::wstring s;
		for (UINT d = 0; d < (UINT)desc->Defines.size(); ++d)
		{
			s += L"-D";
			s += desc->Defines[d].Name;
			s += L"=";
			s += desc->Defines[d].Value;
			s += L" ";
		}
		IDxcOperationResult* pResult = nullptr;
		if (SUCCEEDED(hr = compiler_->Compile(
			source,									// program text
			FilePath,								// file name, mostly for error messages
			desc->EntryPoint,						// entry point function
			desc->TargetProfile,					// target profile
			desc->CompileArguments.data(),          // compilation arguments
			(UINT)desc->CompileArguments.size(),	// number of compilation arguments
			desc->Defines.data(),					// define arguments
			(UINT)desc->Defines.size(),				// number of define arguments
			includeHandler_,						// handler for #include directives
			&pResult)))
		{
			HRESULT hrCompile = E_FAIL;
			if (SUCCEEDED(hr = pResult->GetStatus(&hrCompile)))
			{
				if (SUCCEEDED(hrCompile))
				{
					if (ppResult)
					{
						pResult->GetResult(ppResult);
						hr = S_OK;
					}
					else
					{
						hr = E_FAIL;
					}
				}
			}

			// Always try and get the error buffer to display possible warnings.
			IDxcBlobEncoding* pPrintBlob = nullptr;
			if (SUCCEEDED(pResult->GetErrorBuffer(&pPrintBlob)))
			{
				// We can use the library to get our preferred encoding.
				IDxcBlobEncoding* pPrintBlob16 = nullptr;
				library_->GetBlobAsUtf16(pPrintBlob, &pPrintBlob16);

				OutputDebugStringW(static_cast<const wchar_t*>(pPrintBlob16->GetBufferPointer()));
				//fmt::print(std::wcerr, L"{}", static_cast<const wchar_t*>(pPrintBlob16->GetBufferPointer()));

				SafeRelease(pPrintBlob);
				SafeRelease(pPrintBlob16);
			}

			SafeRelease(pResult);
		}

		return hr;
	}

	HRESULT ShaderCompiler::compileFromFile(LPCWSTR FilePath, ShaderCompilationDesc* desc, IDxcBlob** ppResult)
	{
		HRESULT hr = E_FAIL;

		if (desc)
		{
			IDxcBlobEncoding* source = nullptr;

			if (SUCCEEDED(hr = library_->CreateBlobFromFile(FilePath, nullptr, &source)))
			{
				if (SUCCEEDED(hr = compile(FilePath, source, desc, ppResult)))
				{
					SafeRelease(source);
				}
			}
		}

		return hr;
	}

	HRESULT ShaderCompiler::compileFromString(std::string code, ShaderCompilationDesc* desc, IDxcBlob** ppResult)
	{
		HRESULT hr = E_FAIL;

		if (desc)
		{
			IDxcBlobEncoding* source = nullptr;
			if (SUCCEEDED(hr = library_->CreateBlobWithEncodingFromPinned(code.data(), (UINT32)code.length(), CP_ACP, &source)))
			{
				if (SUCCEEDED(hr = compile(L"NOT_SET", source, desc, ppResult)))
				{
					SafeRelease(source);
				}
			}
		}

		return hr;
	}
}
