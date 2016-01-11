// IPMIWMI.cpp, extracted from ipmims.cpp from ipmiutil.

/*
The BSD License 

Copyright (c) 2008, Intel Corporation
Copyright (c) 2009 Kontron America, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

a.. Redistributions of source code must retain the above copyright notice, 
this list of conditions and the following disclaimer. 
b.. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution. 
c.. Neither the name of Intel Corporation nor the names of its contributors 
may be used to endorse or promote products derived from this software 
without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND 
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR 
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON 
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <windows.h>  
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

typedef unsigned char uchar;

static char fmsopen = 0;
static IWbemLocator *pLoc = 0;
static IWbemServices *pSvc = 0;
static IWbemClassObject* pClass = NULL;
static IEnumWbemClassObject* pEnumerator = NULL;
static IWbemClassObject* pInstance = NULL;
static VARIANT varPath; 

/* 
* Microsoft_IPMI methods:
* void RequestResponse(
*    [in]   uint8 Command,
*    [out]  uint8 CompletionCode,
*    [in]   uint8  Lun,
*    [in]   uint8 NetworkFunction,
*    [in]   uint8 RequestData[],
*    [out]  uint32 ResponseDataSize,
*    [in]   uint32 RequestDataSize,
*    [in]   uint8 ResponderAddress,
*    [out]  uint8 ResponseData
* );
*/

static void cleanup_wmi(void)
{
	VariantClear(&varPath);
	if (pInstance != NULL) pInstance->Release();
	if (pEnumerator != NULL) pEnumerator->Release();
	if (pClass != NULL) pClass->Release();
	if (pSvc != 0) pSvc->Release();
	if (pLoc != 0) pLoc->Release();
	CoUninitialize();
}

int ipmi_open_ms(void)
{
	int bRet = -1;
	HRESULT hres;
	ULONG dwCount = NULL;

	// Initialize COM.
	hres =  CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(hres)) {
		return bRet;
	}

	// Obtain the initial locator to Windows Management
	// on a particular host computer.
	hres = CoCreateInstance( CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
		IID_IWbemLocator, (LPVOID *) &pLoc);
	if (FAILED(hres)) {
		CoUninitialize();
		return bRet;
	}

	// Connect to the root\cimv2 namespace with the current user 
	// and obtain pointer pSvc to make IWbemServices calls.
	hres = pLoc->ConnectServer( _bstr_t(L"ROOT\\WMI"), NULL,  NULL, 0,   
		NULL, 0,  0,  &pSvc );
	if (FAILED(hres)) {
		pLoc->Release();
		CoUninitialize();
		return bRet;
	}

	// Set the IWbemServices proxy so that impersonation
	// of the user (client) occurs.
	hres = CoSetProxyBlanket( pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
		NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
		NULL, EOAC_NONE );
	if (FAILED(hres)) {
		cleanup_wmi();
		return bRet; 
	}

	hres = pSvc->GetObject( L"Microsoft_IPMI", 0, NULL, &pClass, NULL);
	if (FAILED(hres)) {
		cleanup_wmi();
		return bRet;
	} 

	hres = pSvc->CreateInstanceEnum( L"microsoft_ipmi", 0, NULL, &pEnumerator);
	if (FAILED(hres)) {
		cleanup_wmi();
		return bRet;
	} 

	hres = pEnumerator->Next( WBEM_INFINITE, 1, &pInstance, &dwCount);
	if (FAILED(hres)) {
		cleanup_wmi();
		return bRet;
	} 
	VariantInit(&varPath);
	hres = pInstance->Get(_bstr_t(L"__RelPath"), 0, &varPath, NULL, 0);
	if (FAILED(hres)) {
		cleanup_wmi();
		return bRet;
	} else {   /*success*/
		// usually  L"Microsoft_IPMI.InstanceName=\"Root\\SYSTEM\\0003_0\"",
		fmsopen = 1;
		bRet = 0;
	}

	return bRet;
}

extern "C" {

	int ipmi_close_ms(void)
	{
		int bRet = -1;
		if (fmsopen) {
			cleanup_wmi();
			fmsopen = 0;
			bRet = 0;
		}
		return bRet;
	}

	int ipmi_cmdraw_ms(uchar cmd, uchar netfn, uchar lun, uchar sa,
		uchar bus, uchar *pdata, int sdata, uchar *presp, int *sresp, 
		uchar *pcc)
	{
		int bRet;
		HRESULT hres;
		IWbemClassObject* pInParams = NULL; /*class definition*/
		IWbemClassObject* pInReq = NULL;    /*instance*/
		IWbemClassObject* pOutResp = NULL;
		VARIANT varCmd, varNetfn, varLun, varSa, varSize, varData;
		long i;
		uchar *p;

		if (!fmsopen) {
			bRet = ipmi_open_ms();
			if (bRet != 0) return(bRet);
		}
		bRet = -1;

		hres = pClass->GetMethod(L"RequestResponse",0,&pInParams,NULL);
		if (FAILED(hres)) {
			return (bRet);
		}

		VariantInit(&varCmd);
		varCmd.vt = VT_UI1;
		varCmd.bVal = cmd;
		hres = pInParams->Put(_bstr_t(L"Command"), 0, &varCmd, 0);
		VariantClear(&varCmd);
		if (FAILED(hres)) goto MSRET;

		VariantInit(&varNetfn);
		varNetfn.vt = VT_UI1;
		varNetfn.bVal = netfn;
		hres = pInParams->Put(_bstr_t(L"NetworkFunction"), 0, &varNetfn, 0);
		VariantClear(&varNetfn);
		if (FAILED(hres)) goto MSRET;

		VariantInit(&varLun);
		varLun.vt = VT_UI1;
		varLun.bVal = lun;
		hres = pInParams->Put(_bstr_t(L"Lun"), 0, &varLun, 0);
		VariantClear(&varLun);
		if (FAILED(hres)) goto MSRET;

		VariantInit(&varSa);
		varSa.vt = VT_UI1;
		varSa.bVal = sa;
		hres = pInParams->Put(_bstr_t(L"ResponderAddress"), 0, &varSa, 0);
		VariantClear(&varSa);
		if (FAILED(hres)) goto MSRET;

		VariantInit(&varSize);
		varSize.vt = VT_I4;
		varSize.lVal = sdata;
		hres = pInParams->Put(_bstr_t(L"RequestDataSize"), 0, &varSize, 0);
		VariantClear(&varSize);
		if (FAILED(hres)) goto MSRET;

		SAFEARRAY* psa = NULL;
		SAFEARRAYBOUND rgsabound[1];
		rgsabound[0].cElements = sdata;
		rgsabound[0].lLbound = 0;
		psa = SafeArrayCreate(VT_UI1,1,rgsabound);
		if(!psa) {
			goto MSRET;
		}

		/* Copy the real RequestData into psa */
		memcpy(psa->pvData,pdata,sdata);

		VariantInit(&varData);
		varData.vt = VT_ARRAY | VT_UI1;
		varData.parray = psa;
		hres = pInParams->Put(_bstr_t(L"RequestData"), 0, &varData, 0);
		varData.parray = NULL; //DEBUG: set to NULL for final SafeDestoryArray.
		VariantClear(&varData);
		if (FAILED(hres)) {
			goto MSRET;
		}

		hres = pSvc->ExecMethod( V_BSTR(&varPath), _bstr_t(L"RequestResponse"), 
			0, NULL, pInParams, &pOutResp, NULL);
		if (FAILED(hres)) {
			bRet = -1; 
			/*fall through for cleanup and return*/
		}
		else {  /*successful, get ccode and response data */
			VARIANT varByte, varRSz, varRData;
			VariantInit(&varByte);
			VariantInit(&varRSz);
			VariantInit(&varRData);
			long rlen;

			hres = pOutResp->Get(_bstr_t(L"CompletionCode"),0, &varByte, NULL, 0);
			if (FAILED(hres)) goto MSRET;
			*pcc = V_UI1(&varByte);

			hres = pOutResp->Get(_bstr_t(L"ResponseDataSize"),0, &varRSz, NULL, 0);
			if (FAILED(hres)) goto MSRET;
			rlen = V_I4(&varRSz);
			if (rlen > 1) rlen--;   /*skip cc*/
			if (rlen > *sresp) {
				rlen = *sresp; /*truncate*/
			}
			*sresp = (int)rlen;

			hres = pOutResp->Get(_bstr_t(L"ResponseData"),0, &varRData, NULL,0);
			if (FAILED(hres)) { /*ignore failure */ 
			} else {  /* success */
				/* Copy the real ResponseData into presp. */
				p = (uchar*)varRData.parray->pvData;
				for( i = 0; i <= rlen; i++) {
					/* skip the completion code */
					if (i > 0) presp[i-1] = p[i];
				}
			}
			bRet = 0;
		}

MSRET:
		/* VariantClear(&var*) should be done by pInParams->Release() */
		if (pInParams != NULL) pInParams->Release();
		if (pOutResp != NULL) pOutResp->Release();
		if (psa != NULL) SafeArrayDestroy(psa);
		return(bRet);
	}

#if 0
	void GetLastErrorText(DWORD nErrorCode)	{
		char* lpMsgBuf;
		FormatMessageA( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | 
			FORMAT_MESSAGE_FROM_SYSTEM | 
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			nErrorCode,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			 (LPSTR)&lpMsgBuf,
			0,
			NULL 
		);
		printf(lpMsgBuf);
	}
#endif

	void SaveRegistryKey(const char* p) {
		LONG h;
		HKEY hRegKey;
		DWORD len;
		h=RegCreateKeyEx(HKEY_CURRENT_USER,L"Software\\IPMIScanner",
			  0L,
			  NULL,
			  REG_OPTION_VOLATILE,
			  KEY_ALL_ACCESS,
			  NULL,
			  &hRegKey,
			  NULL
			);
		if(h==ERROR_SUCCESS) {
			len=1+strlen(p);
			h=RegSetValueExA(hRegKey,
			  "gadget",
			  0L,
			  REG_SZ,
			  (const unsigned char*)p,
			  len
			);
			h=RegCloseKey(hRegKey);
		}
	}
}