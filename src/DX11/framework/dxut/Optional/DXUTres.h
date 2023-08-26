//----------------------------------------------------------------------------
// File: dxutres.h
//
// Functions to create DXUT media from arrays in memory 
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=320437
//-----------------------------------------------------------------------------
#pragma once

HRESULT WINAPI DXUTCreateGUITextureFromInternalArray( _In_ ID3D11Device* pd3dDevice, _Outptr_ ID3D11Texture2D** ppTexture );
