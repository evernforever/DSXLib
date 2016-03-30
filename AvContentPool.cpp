/*

(c) Copyright Matrox Electronic Systems Ltd., 2003. All rights reserved. 

This code and information is provided "as is" without warranty of any kind,
either expressed or implied, including but not limited to the implied 
warranties of merchantability and/or fitness for a particular purpose.

Disclaimer: Matrox Electronic Systems Ltd. reserves the right to make 
changes in specifications and code at any time and without notice. 
No responsibility is assumed by Matrox Electronic Systems Ltd. for 
its use; nor for any infringements of patents or other rights of 
third parties resulting from its use. No license is granted under 
any patents or patent rights of Matrox Electronic Systems Ltd.

*/

#include "stdafx.h"
#include "LogManager.h"
#include "AvContentPool.h"

// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::CAvContentPool
// Description     : 
// ----------------------------------------------------------------------------
CAvContentPool::CAvContentPool(TAPCLogManager   & in_rJLogManager,
                               wchar_t          * in_pwszName,
                               TAPIMvFlexEngine & in_rJFlexEngine,
                               unsigned long      in_ulMaxAvContent,
                               HRESULT          & io_rhr) 
   : CMvUnknown(in_pwszName, NULL)
   , m_pJLogManager(in_rJLogManager)
   , m_ulTotalNumberOfContents(0)
   , m_ulMaxAvContent(in_ulMaxAvContent)
{
   if (FAILED(io_rhr))
   {
      return;
   }

   ASSERT( wcslen(in_pwszName) < (sizeof(m_wszName) / sizeof(wchar_t)) );
   wcscpy_s(m_wszName, 255, in_pwszName);

   io_rhr = in_rJFlexEngine->CreateAVContentPoolManager(
                     AVCONTENT_POOL_PRIORITY, 
                     this,
                     m_wszName,
                     &m_pJPool);
   ASSERT(SUCCEEDED(io_rhr));
}


// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::~CAvContentPool
// Description     : 
// ----------------------------------------------------------------------------
CAvContentPool::~CAvContentPool()
{
   if (m_pJPool != NULL)
   {
      _EmptyPool();

      m_pJPool = NULL;
   }
}


// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::Free
// Description     : 
// ----------------------------------------------------------------------------
void CAvContentPool::Free(void)
{
   if (m_pJPool != NULL)
   {
      _EmptyPool();

      m_pJPool = NULL;
   }
}


// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::_EmptyPool
// Description     : 
// ----------------------------------------------------------------------------
HRESULT CAvContentPool::_EmptyPool()
{
   HRESULT hr = MV_NOERROR;


   // test if we can still get an avcontent
   while ((m_pJPool->GetNumberOfLinkedAVContent() != 0) && SUCCEEDED(hr))
   {
      TMvSmartPtr <IMvAVContent> pJAvContent;

      hr = m_pJPool->WaitForAVContent(&pJAvContent ); 
      ASSERT(SUCCEEDED(hr));

      // release the one we got its ready (unused), so it goes back to the pool now
      pJAvContent = NULL;
      // request to unlink, we know we can unlink at least one avcontent
      // always unlink the avcontent, don't wait for it to come back it may be held by the caller
      if (SUCCEEDED(hr))
      {
         hr = m_pJPool->RequestToUnlinkAVContent( NULL );
      }
   }

   // reset member count
   m_ulTotalNumberOfContents = 0;

   ASSERT(SUCCEEDED(hr) || hr == MV_E_OUT_OF_MEMORY);
   return hr;
}


// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::_GetAvContent
// Description     : 
// ----------------------------------------------------------------------------
HRESULT CAvContentPool::_GetAvContent(TAPIMvAVContent & out_rJAvContent)
{
   HRESULT hr;

   // An AddRef is done by the pool manager before returning us the AvContent
   // do not wait, this will fail if no content is available
   hr = m_pJPool->GetAVContent(&out_rJAvContent);
   if (FAILED(hr))
   {
      if (m_ulTotalNumberOfContents == m_ulMaxAvContent)
      {
         hr = MV_E_MAX_VALUE_REACHED;

         MV_LOG_TEXT_FORMAT(keLogPkgFlexChannelTester,
            keLogPkgFlexChannelTesterFuncErrors,
            keLogLevelInTeamSpecific,
            ("CAvContentPool::_GetAvContent. MV_E_MAX_VALUE_REACHED[%I32u]", m_ulMaxAvContent) );

      }
      else
      {
         hr = ovl_CreateAvContent(out_rJAvContent);
		   if (SUCCEEDED(hr))
		   {
            // increment count
            m_ulTotalNumberOfContents++;
         }
      }
   }
   
   return hr;
}


// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::_WaitForAvContent
// Description     : 
// ----------------------------------------------------------------------------
HRESULT CAvContentPool::_WaitForAvContent(TAPIMvAVContent & out_rJAvContent)
{
   HRESULT hr;

   // An AddRef is done by the pool manager before returning us the AvContent
   // do not wait, this will fail if no content is available
   hr = m_pJPool->GetAVContent(&out_rJAvContent);
   if (FAILED(hr))
   {
      if (m_ulTotalNumberOfContents == m_ulMaxAvContent)
      {
         hr = m_pJPool->WaitForAVContent(&out_rJAvContent);
      }
      else
      {
         hr = ovl_CreateAvContent(out_rJAvContent);
         if (SUCCEEDED(hr))
         {
            // increment count
            m_ulTotalNumberOfContents++;
         }
      }
   }

   return hr;
}

// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::OnAVContentCompletion
// Description     : 
// ----------------------------------------------------------------------------
HRESULT CAvContentPool::OnAVContentCompletion(IMvAVContent * in_pIAVContent)
{
   HRESULT hr;


   if (m_pJLogManager)
   {
      m_pJLogManager->LogAVContentTiming(m_wszName, in_pIAVContent);
   }

#ifdef _DEBUG
   in_pIAVContent->GetLastError(&hr);
   if (hr != MV_NOERROR)
   {
      CString strError;
      CString strMsg;

      FctUtils::ErrorToString(hr, strError);

      strMsg.Format(_T("CAvContentPool::OnAVContentCompletion: %ls: "), m_wszName);
      strMsg += strError;

      MV_LOG_TEXT_FORMAT(keLogPkgFlexChannelTester,
         keLogPkgFlexChannelTesterFuncErrors,
         keLogLevelInTeamSpecific,
         ("%S", strMsg) );

      //strMsg += _T("\n");
      //OutputDebugString(strMsg);
   }
#endif

   // Read event is signaled!  Surface is READY!!!  Flush it and return it.
   hr = in_pIAVContent->Flush();
   ASSERT(SUCCEEDED(hr));

   return MV_NOERROR;
}


// ----------------------------------------------------------------------------
// Function name   : CAvContentPool::_CreateInitialPool
// Description     : 
// ----------------------------------------------------------------------------
HRESULT CAvContentPool::_CreateInitialPool(int in_iInitialAvContent)
{
   HRESULT hr;
   int     iIndex;

   // Create the initial count of surface
   for (iIndex = 0 ; iIndex < in_iInitialAvContent ; iIndex++)
   {
      TAPIMvAVContent pQAvcontent;

      hr = ovl_CreateAvContent(pQAvcontent);

      if (SUCCEEDED(hr))
      {
         // increment count
         m_ulTotalNumberOfContents++;
      }
      else
      {
         return hr;
      }
   }

   return MV_NOERROR;
}

/*virtual*/
HRESULT __stdcall CAvContentPool::LogInformation(void)
{
   return m_pJPool->LogInformation();
}
