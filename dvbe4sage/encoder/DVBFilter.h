// DVBFilter.h : Declaration of the DVBFilter and stuff

#pragma once

#include "DVBParser.h"

class DVBFilter;
class DVBFilterInputPin;

class DVBPullPin : public CPullPin
{
private:
	DVBFilterInputPin* const		m_pParentPin;

	// Disallow default and copy constructors
	DVBPullPin();
	DVBPullPin(const DVBPullPin&);

public:
	// The only valid constructor
	DVBPullPin(DVBFilterInputPin* pParentPin) : m_pParentPin(pParentPin) {}

	 // override this to handle data arrival
    // return value other than S_OK will stop data
    virtual HRESULT Receive(IMediaSample* sample);

    // override this to handle end-of-stream
    virtual HRESULT EndOfStream();

    // called on runtime errors that will have caused pulling
    // to stop
    // these errors are all returned from the upstream filter, who
    // will have already reported any errors to the filtergraph.
    virtual void OnError(HRESULT hr);

    // flush this pin and all downstream
    virtual HRESULT BeginFlush();
    virtual HRESULT EndFlush();

	virtual HRESULT DecideAllocator(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* pProps);
};

class DVBFilterInputPin : public CRenderedInputPin
{
private:
	// Disallow default and copy constructors
	DVBFilterInputPin();
	DVBFilterInputPin(const DVBFilterInputPin&);
public:
	// Constructor
    DVBFilterInputPin(DVBFilter* pDVBFilter, CCritSec* pLock, HRESULT* phr);

    // Do something with this media sample
    STDMETHODIMP Receive(IMediaSample* pSample);

	// We need to indicate the sender that we can block the processing
    STDMETHODIMP ReceiveCanBlock();

    // Check if the pin can support this specific proposed type and format (we support everything)
	HRESULT CheckMediaType(const CMediaType*);

	// We override this method since our buffer must be mod(188) == 0!
	STDMETHODIMP GetAllocatorRequirements(ALLOCATOR_PROPERTIES* pProps);

	// We need to override this method in order to handle synchronization issues
	STDMETHODIMP EndOfStream(void);
};

class DVBFilterOutputPin : public CBaseOutputPin
{
private:
	// Disallow default and copy constructors
	DVBFilterOutputPin();
	DVBFilterOutputPin(const DVBFilterOutputPin&);
protected:
	HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);
	HRESULT CheckMediaType(const CMediaType* pMediaType);
	HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* ppropInputRequest);
public:
	DVBFilterOutputPin(CBaseFilter* pFilter, CCritSec* pLock, HRESULT* phr);
};

class DVBFilter : public CBaseFilter
{
private:
	// Data members
    DVBFilterInputPin*			m_pPin1;						// This is out only input pin
	DVBFilterOutputPin*			m_pPin2;						// This is out only output pin
	CCritSec					m_Lock;							// Main renderer critical section
	DVBParser					m_Parser;						// The parser itself

	// Disallow default and copy constructors
	DVBFilter();
	DVBFilter(const DVBFilter&);
public:
	// Constructor
    DVBFilter(UINT ordinal);
	// Destructor
	virtual ~DVBFilter();

    // Pin enumeration
	CBasePin* GetPin(int n);
	int GetPinCount() { return 2; }

	// Provide access to the parser object
	DVBParser& getParser() { return m_Parser; }
};

class FileReaderOutputPin : public CSourceStream
{
private:
	FILE* const					m_InFile;						// The file handle

	// Disallow default and copy constructors
	FileReaderOutputPin();
	FileReaderOutputPin(const FileReaderOutputPin&);
protected:
	HRESULT GetMediaType(int iPosition, CMediaType* pMediaType);
	HRESULT CheckMediaType(const CMediaType* pMediaType);
	HRESULT DecideBufferSize(IMemAllocator* pAlloc, ALLOCATOR_PROPERTIES* ppropInputRequest);
	HRESULT FillBuffer(IMediaSample* pSample);
public:
	FileReaderOutputPin(CSource* pFilter, HRESULT* phr, TCHAR* debugName, LPCWSTR fileName, bool& isOK);
	~FileReaderOutputPin();
};

class FileReaderFilter : public CSource
{
private:
	FileReaderOutputPin*		m_pPin1;						// This is out only output pin

	// Disallow default and copy constructors
	FileReaderFilter();
	FileReaderFilter(const FileReaderFilter&);

public:
	// Constructor
    FileReaderFilter(LPCWSTR fileName, bool& isOK);
	// Destructor
	virtual ~FileReaderFilter();
};
