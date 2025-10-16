#pragma once

#include "CNamedPipeIPC.h"

typedef VOID (*PPIPE_SERVER_ON_CONNECT) (DWORD pipeIndex);
typedef VOID (*PPIPE_SERVER_ON_DISCONNECT) (DWORD pipeIndex);
typedef VOID (*PPIPE_SERVER_ON_MESSAGE) (DWORD pipeIndex, VOID* rqstBuf, SIZE_T bufSize);

class CNamedPipeServer
{
public:
	CNamedPipeServer(LPTSTR lpszPipeName, PPIPE_SERVER_ON_MESSAGE pOnMessage,
		PPIPE_SERVER_ON_CONNECT pOnConnected = NULL,
		PPIPE_SERVER_ON_DISCONNECT pOnDisconnected = NULL);
	~CNamedPipeServer();

	//delete copy constructor and assignment operator
	CNamedPipeServer(const CNamedPipeServer&) = delete;
	CNamedPipeServer& operator=(const CNamedPipeServer&) = delete;

	DWORD Run();
	VOID Stop();

	DWORD SendData(DWORD pipeIndex, LPVOID data, size_t data_size);
	VOID BroadcastData(LPVOID data, size_t data_size);

private:
	DWORD CreatePipeAndEvents();
	DWORD ConnectToNewClient(HANDLE pipe, LPOVERLAPPED overlapped,	BOOL& pendingIO);
	void DisconnectAndReconnect(DWORD pipeIndex);

	void GetPendingOperationResult(DWORD pipeIndex);
	//void ExecuteCurrentState(DWORD pipeIndex);

	DWORD ReadPipe(DWORD pipeIndex);
	DWORD WritePipe(DWORD pipeIndex, LPVOID msg, size_t msg_size);

	void OnMessage(DWORD pipeIndex);
	void OnConnected(DWORD pipeIndex);
	void OnDisconnected(DWORD pipeIndex);

private:
	static const int nMaxPipes = MAX_PIPE_INSTANCES;
	static const int nMaxEvents = nMaxPipes + 1;

	static const int nMaxBufferSize = MAX_PIPE_BUFFER_SIZE;
	static const int nPipeTimeout = 5000;

	enum class PipeStates { CONNECTING, READING };

	struct PipeInstanceDeleter 
	{
		void operator()(HANDLE pipeInstance) const 
		{
			if (pipeInstance != INVALID_HANDLE_VALUE) 
			{
				FlushFileBuffers(pipeInstance);
				DisconnectNamedPipe(pipeInstance);
				CloseHandle(pipeInstance);
			}
		}
	};

	class Pipe
	{
	public:
		Pipe() {}

		//delete copy constructor and assignment operator
		Pipe(const Pipe&) = delete;
		Pipe& operator=(const Pipe&) = delete;

		DWORD CreatePipeInstance(LPCTSTR lpszPipeName)
		{
            //initialize the scurity descriptor
            SECURITY_DESCRIPTOR sd = {};

            // Initialize the security descriptor
			if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
			{
				return GetLastError();
            }

            // Set the security descriptor to allow all users to access the pipe
            if (!SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
			{
				return GetLastError();
            }

            //set security attributes to allow access from all users
            SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), &sd, TRUE };

			HANDLE hPipe = CreateNamedPipe(
				lpszPipeName,
				PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
				PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
				nMaxPipes,
				nMaxBufferSize,
				nMaxBufferSize,
				nPipeTimeout,
				&sa);
			if (hPipe == INVALID_HANDLE_VALUE)
			{
				return GetLastError();
			}

			mPipeInstance.reset(hPipe);

			return ERROR_SUCCESS;
		}

		std::unique_ptr<std::remove_pointer_t<HANDLE>, PipeInstanceDeleter>	mPipeInstance;

		OVERLAPPED	mOverlap = {};
		BOOL		mPendingIO = FALSE;
		PipeStates	mCurrentState = PipeStates::CONNECTING;
	
		BYTE		mPipeReadBuffer[nMaxBufferSize] = { 0 };
		DWORD		mBytesRead = 0;
	};

	TCHAR	m_szPipeName[MAX_PATH] = { 0 };

	Pipe	m_instPipes[nMaxPipes];
	HANDLE  m_hExitEvent = NULL;
	HANDLE	m_hEvents[nMaxEvents];

	std::mutex					m_mutexWrite;

	PPIPE_SERVER_ON_CONNECT		m_pOnConnected = NULL;
	PPIPE_SERVER_ON_DISCONNECT	m_pOnDisconnected = NULL;
	PPIPE_SERVER_ON_MESSAGE		m_pOnMessage = NULL;
};

