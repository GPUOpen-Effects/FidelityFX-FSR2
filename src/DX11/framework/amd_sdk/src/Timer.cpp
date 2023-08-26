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

#include "DXUT.h"
#include "Timer.h"

//using namespace AMD;

#if USE_RDTSC
__declspec(naked) LONGLONG __cdecl rdtsc_time(void)
{
    __asm rdtsc
    __asm ret
}
#endif

//-----------------------------------------------------------------------------
Timer::Timer() :
m_LastTime( 0.0 ),
m_SumTime( 0.0 ),
m_NumFrames( 0 )
{
}

Timer::~Timer()
{
}

double Timer::GetTime()
{
    FinishCollection();

    return m_LastTime;
}
double Timer::GetSumTime()
{
    return m_SumTime;
}

double Timer::GetTimeNumFrames()
{
    return m_NumFrames;
}

//-----------------------------------------------------------------------------

CpuTimer::CpuTimer() :
Timer()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency( &freq );
    m_freq = static_cast<double>(freq.QuadPart);

#if USE_RDTSC
    const double calibrationTime = 0.1;

    LONGLONG start, stop;
    start = rdtsc_time();
    Delay(calibrationTime);
    stop = rdtsc_time();

    m_freqRdtsc = static_cast<double>(stop - start) / calibrationTime;
#endif
}

CpuTimer::~CpuTimer()
{
}

void CpuTimer::Reset( bool bResetSum )
{
    m_LastTime = 0.0;
    if (bResetSum)
    {
        m_SumTime = 0.0;
        m_NumFrames = 0;
    }
    else
    {
        ++m_NumFrames;
    }
}

void CpuTimer::Start()
{
#if USE_RDTSC
    m_startTime.QuadPart = rdtsc_time();
#else
    QueryPerformanceCounter( &m_startTime );
#endif
}

void CpuTimer::Stop()
{
    LARGE_INTEGER t;
    double freq;

#if USE_RDTSC
    t.QuadPart = rdtsc_time();
    freq = m_freqRdtsc;
#else
    QueryPerformanceCounter( &t );
    freq = m_freq;
#endif

    m_LastTime += static_cast<double>(t.QuadPart - m_startTime.QuadPart) / freq;
    m_SumTime += static_cast<double>(t.QuadPart - m_startTime.QuadPart) / freq;
}

void CpuTimer::Delay( double sec )
{
    LARGE_INTEGER start, stop;
    double t;

    QueryPerformanceCounter( &start );

    do
    {
        QueryPerformanceCounter( &stop );

        t = static_cast<double>(stop.QuadPart - start.QuadPart) / m_freq;
    } while (t < sec);
}

//-----------------------------------------------------------------------------

GpuTimer::GpuTimer( ID3D11Device* pDev, UINT64 freq, UINT numTimeStamps ) :
Timer(),
m_pDevCtx( NULL ),
m_numTimeStamps( numTimeStamps ),
m_curIssueTs( m_numTimeStamps - 1 ),
m_nextRetrTs( 0 ),
m_FrameID( 0 ),

m_CurTime( 0.0 )
{
    HRESULT hr;

    _ASSERT( pDev != NULL );
    _ASSERT( numTimeStamps>0 );

    pDev->GetImmediateContext( &m_pDevCtx );
    _ASSERT( m_pDevCtx != NULL );

    D3D11_QUERY_DESC qd;
    qd.MiscFlags = 0;

    m_ts = new TsRecord[m_numTimeStamps];
    for (UINT i = 0; i < m_numTimeStamps; i++)
    {
        qd.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

        hr = pDev->CreateQuery( &qd, &m_ts[i].pDisjointTS );
        _ASSERT( (hr == S_OK) && (m_ts[i].pDisjointTS != NULL) );

        qd.Query = D3D11_QUERY_TIMESTAMP;

        hr = pDev->CreateQuery( &qd, &m_ts[i].pStart );
        _ASSERT( (hr == S_OK) && (m_ts[i].pStart != NULL) );

        hr = pDev->CreateQuery( &qd, &m_ts[i].pStop );
        _ASSERT( (hr == S_OK) && (m_ts[i].pStop != NULL) );

        m_ts[i].state.stateWord = 0;
    }
    m_CurTimeFrame.id = 0;
    m_CurTimeFrame.invalid = 1;

    freq = 0;//prevent warning
}

GpuTimer::~GpuTimer()
{
    for (UINT i = 0; i < m_numTimeStamps; i++)
    {
        SAFE_RELEASE( m_ts[i].pDisjointTS );
        SAFE_RELEASE( m_ts[i].pStart );
        SAFE_RELEASE( m_ts[i].pStop );
    }
    SAFE_DELETE_ARRAY( m_ts );

    SAFE_RELEASE( m_pDevCtx );
}

void GpuTimer::Reset( bool bResetSum )
{
    FinishCollection();
    m_FrameID = (m_FrameID + 1) & 0x3FFFFFFF;

    if (bResetSum)
    {
        WaitIdle();
        m_CurTimeFrame.invalid = 1;
        m_CurTime = 0.0;
        m_LastTime = 0.0;
        m_SumTime = 0.0;
        m_NumFrames = 0;
    }
}

void GpuTimer::Start()
{
    if (++m_curIssueTs == m_numTimeStamps)
    {
        m_curIssueTs = 0;
    }

    if (0 != m_ts[m_curIssueTs].state.data.startIssued)
    {
        _ASSERT( false && "CPU stall required! This should never happen. Please increase GpuTimer::NumTimeStamps in Timer.h" );
        CollectData( m_curIssueTs, TRUE );
    }

    m_ts[m_curIssueTs].state.data.frameID = m_FrameID;
    m_ts[m_curIssueTs].state.data.startIssued = 1;
    m_ts[m_curIssueTs].state.data.stopIssued = 0;
    m_pDevCtx->Begin( m_ts[m_curIssueTs].pDisjointTS );
    m_pDevCtx->End( m_ts[m_curIssueTs].pStart );
}

void GpuTimer::Stop()
{
    // check if timestamp start has been issued but no stop yet
    _ASSERT( (m_ts[m_curIssueTs].state.data.startIssued == 1) && (m_ts[m_curIssueTs].state.data.stopIssued == 0) );

    m_ts[m_curIssueTs].state.data.stopIssued = 1;
    m_pDevCtx->End( m_ts[m_curIssueTs].pStop );
    m_pDevCtx->End( m_ts[m_curIssueTs].pDisjointTS );
}

void GpuTimer::WaitIdle()
{
    while (m_nextRetrTs != m_curIssueTs)
    {
        CollectData( m_nextRetrTs, true );

        if (++m_nextRetrTs == m_numTimeStamps)
        {
            m_nextRetrTs = 0;
        }
    }

    // retrieve the current Ts
    CollectData( m_nextRetrTs, true );
    if (++m_nextRetrTs == m_numTimeStamps)
    {
        m_nextRetrTs = 0;
    }

    if (0 == m_CurTimeFrame.invalid)
    {
        m_LastTime = m_CurTime;
        m_SumTime += m_CurTime;
        ++m_NumFrames;
    }
}

void GpuTimer::FinishCollection()
{
    // retrieve all available timestamps
    while (CollectData( m_nextRetrTs ))
    {
        if (++m_nextRetrTs == m_numTimeStamps)
        {
            m_nextRetrTs = 0;
        }
    }
}

bool GpuTimer::CollectData( UINT idx, BOOL stall )
{
    if (!m_ts[idx].state.data.stopIssued)
    {
        return false;
    }

    // start collecting data from a new frame?
    if (m_ts[idx].state.data.frameID != m_CurTimeFrame.id)
    {
        // if frametimes collected are valid: write them into m_time
        // so m_time always contains the most recent valid timing data
        if (0 == m_CurTimeFrame.invalid)
        {
            m_LastTime = m_CurTime;
            m_SumTime += m_CurTime;
            ++m_NumFrames;
        }

        // start collecting time data of the next frame
        m_CurTime = 0.0;
        m_CurTimeFrame.id = m_ts[idx].state.data.frameID;
        m_CurTimeFrame.invalid = 0;
    }

    // if we want to retrieve the next timing data NOW the CPU will stall
    // increase NumTimeStamps in Timer.h to prevent this from happening
    if (stall)
    {
        HRESULT hr;
        UINT64 start, stop;

        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT tsd;
        do
        {
            hr = m_pDevCtx->GetData( m_ts[idx].pDisjointTS, &tsd, sizeof( D3D11_QUERY_DATA_TIMESTAMP_DISJOINT ), 0 );
        } while (hr == S_FALSE);

        _ASSERT( hr == S_OK );

        do
        {
            hr = m_pDevCtx->GetData( m_ts[idx].pStart, &start, sizeof( UINT64 ), 0 );
        } while (hr == S_FALSE);

        _ASSERT( hr == S_OK );

        do
        {
            hr = m_pDevCtx->GetData( m_ts[idx].pStop, &stop, sizeof( UINT64 ), 0 );
        } while (hr == S_FALSE);

        _ASSERT( hr == S_OK );

        if (tsd.Disjoint || ((start & 0xFFFFFFFF) == 0xFFFFFFFF) || ((stop & 0xFFFFFFFF) == 0xFFFFFFFF))
        {
            // mark current frametime as invalid
            m_CurTimeFrame.invalid = 1;
        }
        else
        {
            m_CurTime += static_cast<double>(stop - start) / static_cast<double>(tsd.Frequency);
        }

        m_ts[idx].state.stateWord = 0;
        return true;
    }

    // finally try collecting the available data, return false if it was not yet available
    UINT64 start, stop;

    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT tsd;
    if (S_FALSE == m_pDevCtx->GetData( m_ts[idx].pDisjointTS, &tsd, sizeof( D3D11_QUERY_DATA_TIMESTAMP_DISJOINT ), 0 ))
    {
        return false;
    }
    if (S_FALSE == m_pDevCtx->GetData( m_ts[idx].pStart, &start, sizeof( UINT64 ), 0 ))
    {
        return false;
    }

    if (S_FALSE == m_pDevCtx->GetData( m_ts[idx].pStop, &stop, sizeof( UINT64 ), 0 ))
    {
        return false;
    }

    // all data was available, so evaluate times
    if (tsd.Disjoint || ((start & 0xFFFFFFFF) == 0xFFFFFFFF) || ((stop & 0xFFFFFFFF) == 0xFFFFFFFF))
    {
        // mark current frametime as invalid
        m_CurTimeFrame.invalid = 1;
    }
    else
    {
        UINT64 dt = (stop - start);
        m_CurTime += static_cast<double>(dt) / static_cast<double>(tsd.Frequency);
    }

    m_ts[idx].state.stateWord = 0;
    return true;
}

//-----------------------------------------------------------------------------

GpuCpuTimer::GpuCpuTimer( ID3D11Device* pDev ) :
CpuTimer()
{
    HRESULT hr;

    _ASSERT( pDev != NULL );

    pDev->GetImmediateContext( &m_pDevCtx );
    _ASSERT( m_pDevCtx != NULL );

    D3D11_QUERY_DESC qd;
    qd.Query = D3D11_QUERY_EVENT;
    qd.MiscFlags = 0;

    hr = pDev->CreateQuery( &qd, &m_pEvent );
    _ASSERT( (hr == S_OK) && (m_pEvent != NULL) );
}

GpuCpuTimer::~GpuCpuTimer()
{
    SAFE_RELEASE( m_pEvent );
    SAFE_RELEASE( m_pDevCtx );
}

void GpuCpuTimer::Start()
{
    WaitIdle();
    CpuTimer::Start();
}

void GpuCpuTimer::Stop()
{
    WaitIdle();
    CpuTimer::Stop();
}

void GpuCpuTimer::WaitIdle()
{
    m_pDevCtx->End( m_pEvent );

    HRESULT hr;
    BOOL data;

    do
    {
        hr = m_pDevCtx->GetData( m_pEvent, &data, sizeof( BOOL ), 0 );
    } while (hr == S_FALSE);

    _ASSERT( hr == S_OK );
}

//-----------------------------------------------------------------------------
// convenience timer functions
//-----------------------------------------------------------------------------

TimingEvent::TimingEvent() :
m_name( NULL ),
m_nameLen( 0 ),
m_used( false ),
m_parent( NULL ),
m_firstChild( NULL ),
m_next( NULL )
{
    m_gpu = (NULL != TimerEx::Instance().GetDevice()) ? new GpuTimer( TimerEx::Instance().GetDevice(), 0, 16 ) : NULL;
}

TimingEvent::~TimingEvent()
{
    SAFE_DELETE( m_gpu );
    SAFE_DELETE_ARRAY( m_name );
}

void TimingEvent::SetName( LPCWSTR name )
{
    size_t len_req = wcslen( name ) + 1;
    if (len_req > m_nameLen)
    {
        SAFE_DELETE_ARRAY( m_name );
        int len_alloc = 32 + (len_req & 0xFFFFFFE0);  // round up to next multiple of 32 so when reused we don't have to realloc too often
        m_name = new WCHAR[len_alloc];
        m_nameLen = len_alloc;
    }
    wcscpy_s( m_name, m_nameLen, name );
}

LPCWSTR TimingEvent::GetName()
{
    return m_name;
}

void TimingEvent::Start()
{
    m_used = true;
    if (NULL != m_gpu) { m_gpu->Start(); }
    m_cpu.Start();
}

void TimingEvent::Stop()
{
    m_cpu.Stop();
    if (NULL != m_gpu) { m_gpu->Stop(); }
    m_used = true;
}

double TimingEvent::GetTime( TimerType type, bool stall )
{
    switch (type)
    {
    case ttCpu:
        return m_cpu.GetTime();
    case ttGpu:
        if (NULL != m_gpu)
        {
            if (stall)
            {
                m_gpu->WaitIdle();
            }
            return m_gpu->GetTime();
        }
        // else fallthrough
    default:
        return 0.0f;
    }
}

double TimingEvent::GetAvgTime( TimerType type, bool stall )
{
    switch (type)
    {
    case ttCpu:
        return m_cpu.GetSumTime() / m_cpu.GetTimeNumFrames();
    case ttGpu:
        if (NULL != m_gpu)
        {
            if (stall)
            {
                m_gpu->WaitIdle();
            }
            return m_gpu->GetSumTime() / m_gpu->GetTimeNumFrames();
        }
        // else fallthrough
    default:
        return 0.0f;
    }
}

TimingEvent* TimingEvent::GetTimer( LPCWSTR timerId )
{
    size_t len = wcslen( timerId );
    size_t seperator = wcscspn( timerId, L"/|\\" );

    if (seperator < len)
    {
        LPWSTR idCopy = new WCHAR[len + 1];
        wcscpy_s( idCopy, len + 1, timerId );
        idCopy[seperator] = 0;

        TimingEvent* te = m_firstChild;
        while (te)
        {
            if (!wcscmp( idCopy, te->m_name ))
            {
                te = te->GetTimerRec( &idCopy[seperator + 1] );
                delete [] idCopy;
                return te;
            }
            te = te->m_next;
        }

        delete [] idCopy;
    }
    else
    {
        TimingEvent* te = m_firstChild;
        while (te)
        {
            if (!wcscmp( timerId, te->m_name ))
            {
                return te;
            }
            te = te->m_next;
        }
    }
    return NULL;
}

TimingEvent* TimingEvent::GetParent()
{
    return m_parent;
}

TimingEvent* TimingEvent::GetFirstChild()
{
    return m_firstChild;
}

TimingEvent* TimingEvent::GetNextTimer()
{
    return m_next;
}

// when this function is called we know we're working on a copy of the name, so we can "destruct" it
TimingEvent* TimingEvent::GetTimerRec( LPWSTR timerId )
{
    size_t len = wcslen( timerId );
    size_t seperator = wcscspn( timerId, L"/|\\" );
    if (seperator < len)
    {
        timerId[seperator] = 0;
    }

    TimingEvent* te = m_firstChild;
    while (te)
    {
        if (!wcscmp( timerId, te->m_name ))
        {
            if (seperator < len)
            {
                te = te->GetTimerRec( &timerId[seperator + 1] );
            }

            return te;
        }
        te = te->m_next;
    }

    return NULL;
}

TimingEvent* TimingEvent::FindLastChildUsed()
{
    TimingEvent* ret = NULL;
    TimingEvent* te = m_firstChild;
    while (te)
    {
        if (te->m_used)
        {
            ret = te;
        }
        te = te->m_next;
    }
    return ret;
}
//-----------------------------------------------------------------------------

TimerEx::TimerEx() :
m_pDev( NULL ),
m_Current( NULL ),
m_Unused( NULL )
{
};

TimerEx::~TimerEx()
{
    _ASSERT( "Stop() not called for every Start(...)" && (m_Current == NULL) );

    Destroy();
}

void TimerEx::DeleteTimerTree( TimingEvent* te )
{
    // first delete all children
    while (NULL != te)
    {
        DeleteTimerTree( te->m_firstChild );

        TimingEvent* tmp = te;
        te = te->m_next;
        delete tmp;
    }
}

void TimerEx::Init( ID3D11Device* pDev )
{
    m_pDev = pDev;
}

void TimerEx::Destroy()
{
    // delete all unused
    TimingEvent* te = m_Unused;
    while (NULL != te)
    {
        TimingEvent* tmp = te;
        te = te->m_next;
        delete tmp;
    }
    m_Unused = NULL;

    // delete all used
    DeleteTimerTree( m_Root );
    m_Root = NULL;

    m_pDev = NULL;
}

void TimerEx::Reset( TimingEvent* te, bool bResetSum )
{
    TimingEvent* prev = NULL;
    while (NULL != te)
    {
        // recursion
        Reset( te->m_firstChild, bResetSum );

        // reset the timer event
        te->m_cpu.Reset( bResetSum );
        if (NULL != te->m_gpu) { te->m_gpu->Reset( bResetSum ); }

        if (te->m_used || !bResetSum)
        {
            //if it was used this frame: just reset
            te->m_used = false;
            prev = te;
            te = te->m_next;
        }
        else
        {
            if (bResetSum && !te->m_used)
            {
                te->m_used = false;
                // if it was not used this frame
                //move to unused timer list
                TimingEvent* tmp = te;
                te = te->m_next;

                if (NULL == prev)
                {
                    if (NULL == tmp->m_parent)
                    {
                        m_Root = tmp->m_next;
                    }
                    else
                    {
                        tmp->m_parent->m_firstChild = tmp->m_next;
                    }
                }
                else
                {
                    prev->m_next = tmp->m_next;
                }

                tmp->m_parent = NULL;
                tmp->m_next = m_Unused;
                m_Unused = tmp;
            }
        }
    }
}

void TimerEx::Reset( bool bResetSum )
{
    _ASSERT( "init not called or called with NULL" && (m_pDev != NULL) );
    _ASSERT( "Stop() not called for every Start(...)" && (m_Current == NULL) );

    if (NULL != m_Root)
    {
        Reset( m_Root, bResetSum );
    }
}

void TimerEx::Start( LPCWSTR timerId )
{
    _ASSERT( "init not called or called with NULL" && (m_pDev != NULL) );

    TimingEvent* te = (NULL == m_Current) ? GetTimer( timerId ) : m_Current->GetTimer( timerId );
    if (NULL == te)
    {
        // create new timer event
        if (NULL == m_Unused)
        {
            te = new TimingEvent();
        }
        else
        {
            te = m_Unused;
            m_Unused = te->m_next;
            te->m_next = NULL;
        }

        te->SetName( timerId );
        te->m_parent = m_Current;

        // now look where to insert it
        TimingEvent* lu = NULL;
        if (NULL == m_Current)
        {
            TimingEvent* tmp = m_Root;
            while (tmp)
            {
                if (tmp->m_used)
                {
                    lu = tmp;
                }
                tmp = tmp->m_next;
            }
        }
        else
        {
            lu = m_Current->FindLastChildUsed();
        }

        if (NULL != lu)
        {
            te->m_next = lu->m_next;
            lu->m_next = te;
        }
        else
        {
            if (NULL == m_Current)
            {
                te->m_next = m_Root;
                m_Root = te;
            }
            else
            {
                te->m_next = m_Current->m_firstChild;
                m_Current->m_firstChild = te;
            }

        }
    }

    m_Current = te;
    m_Current->Start();
}

void TimerEx::Stop()
{
    _ASSERT( "init not called or called with NULL" && (m_pDev != NULL) );
    _ASSERT( "Start(...) not called before Stop()" && (m_Current != NULL) );

    m_Current->Stop();
    m_Current = m_Current->m_parent;
}

double TimerEx::GetTime( TimerType type, LPCWSTR timerId, bool stall )
{
    _ASSERT( "init not called or called with NULL" && (m_pDev != NULL) );

    TimingEvent* te = NULL;

    if (NULL != m_Current)
    {
        te = m_Current->GetTimer( timerId );
    }

    if (NULL == te)
    {
        te = GetTimer( timerId );
    }

    return (NULL != te) ? te->GetTime( type, stall ) : 0.0;
}

double TimerEx::GetAvgTime( TimerType type, LPCWSTR timerId, bool stall )
{
    _ASSERT( "init not called or called with NULL" && (m_pDev != NULL) );

    TimingEvent* te = NULL;

    if (NULL != m_Current)
    {
        te = m_Current->GetTimer( timerId );
    }

    if (NULL == te)
    {
        te = GetTimer( timerId );
    }

    return (NULL != te) ? te->GetAvgTime( type, stall ) : 0.0;
}


TimingEvent* TimerEx::GetTimer( LPCWSTR timerId )
{
    if (NULL == timerId)
    {
        return m_Root;
    }

    _ASSERT( "init not called" && (m_pDev != NULL) );

    size_t len = wcslen( timerId );
    size_t seperator = wcscspn( timerId, L"/|\\" );
    if (seperator < len)
    {
        LPWSTR idCopy = new WCHAR[len + 1];
        wcscpy_s( idCopy, len + 1, timerId );
        idCopy[seperator] = 0;
        TimingEvent* te = m_Root;
        while (te)
        {
            if (!wcscmp( idCopy, te->m_name ))
            {
                te = te->GetTimerRec( &idCopy[seperator + 1] );
                delete [] idCopy;
                return te;
            }
            te = te->m_next;
        }
        delete [] idCopy;
    }
    else
    {
        TimingEvent* te = m_Root;
        while (te)
        {
            if (!wcscmp( timerId, te->m_name ))
            {
                return te;
            }
            te = te->m_next;
        }
    }
    return NULL;
}
