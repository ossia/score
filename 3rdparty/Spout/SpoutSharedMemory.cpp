/**

http://www.codeproject.com/Articles/835818/Ultimate-Shared-Memory-A-flexible-class-for-interp

	SpoutSharedMemory.cpp

	Thanks and credit to Malcolm Bechard the author of this class

	https://github.com/mbechard

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		Copyright (c) 2014-2017, Lynn Jarvis. All rights reserved.

		Redistribution and use in source and binary forms, with or without modification, 
		are permitted provided that the following conditions are met:

		1. Redistributions of source code must retain the above copyright notice, 
		   this list of conditions and the following disclaimer.

		2. Redistributions in binary form must reproduce the above copyright notice, 
		   this list of conditions and the following disclaimer in the documentation 
		   and/or other materials provided with the distribution.

		THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"	AND ANY 
		EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
		OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE	ARE DISCLAIMED. 
		IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
		INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
		PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
		INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
		LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
		OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
		- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

*/

#include "SpoutSharedMemory.h"
#include <assert.h>
#include <string>

SpoutSharedMemory::SpoutSharedMemory()
{
	m_pBuffer = NULL;
	m_hMutex = NULL;
	m_hMap = NULL;
	m_pName = NULL;
	m_size = 0;
	m_lockCount = 0;
}

SpoutSharedMemory::~SpoutSharedMemory()
{
	Close();
}

// Create a new memory segment, or attach to an existing one
SpoutCreateResult SpoutSharedMemory::Create(const char* name, int size)
{
	DWORD err;

	// Don't call open twice on the same object without a Close()
	assert(name);
	assert(size);

	if (m_hMap != NULL)	{
		assert(strcmp(name, m_pName) == 0);
		assert(m_pBuffer && m_hMutex);
		return SPOUT_ALREADY_CREATED;
	}

	// https://msdn.microsoft.com/en-us/library/windows/desktop/aa366537%28v=vs.85%29.aspx
	// Creates or opens a named or unnamed file mapping object for a specified file.
	// If hFile is INVALID_HANDLE_VALUE, the calling process must also specify a size
	// for the file mapping object in the dwMaximumSizeHigh and dwMaximumSizeLow parameters.
	// In this scenario, CreateFileMapping creates a file mapping object of a specified size
	// that is backed by the system paging file instead of by a file in the file system.

	m_hMap = CreateFileMappingA ( INVALID_HANDLE_VALUE,
									NULL,
									PAGE_READWRITE,
									0,
									size,
									(LPCSTR)name);

	if (m_hMap == NULL)	{
		return SPOUT_CREATE_FAILED;
	}

	// If the object exists before the function call, the function returns a handle
	// to the existing object (with its current size, not the specified size),
	// and GetLastError returns ERROR_ALREADY_EXISTS.
	err = GetLastError();
	bool alreadyExists = false;
	if (err == ERROR_ALREADY_EXISTS) {
		alreadyExists = true;
		// The size of the map will be the same as when it was created.
		// 2.004 apps will have created a 10 sender map which will not be increased in size thereafter.
	}
	else {
		if(err != 0) printf("SpoutSharedMemory::Create - Error = %ld (0x%x)\n", err, err);
	}

	m_pBuffer = (char*)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);

	if (!m_pBuffer)	{
		Close();
		return SPOUT_CREATE_FAILED;
	}

	std::string	mutexName;
	mutexName = name;
	mutexName += "_mutex";

	m_hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());

	if (!m_hMutex) {
		Close();
		return SPOUT_CREATE_FAILED;
	}

	// Set the name and size
	m_pName = _strdup(name);
	m_size = size;

	return alreadyExists ? SPOUT_ALREADY_EXISTS : SPOUT_CREATE_SUCCESS;

}


bool SpoutSharedMemory::Open(const char* name)
{
	// Don't call open twice on the same object without a Close()
	assert(name);

	if (m_hMap)	{
		assert(strcmp(name, m_pName) == 0);
		assert(m_pBuffer && m_hMutex);
		return true;
	}

	m_hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, (LPCSTR)name);
	if (m_hMap == NULL)	{
		return false;
	}

	m_pBuffer = (char*)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
	if (!m_pBuffer)	{
		Close();
		return false;
	}

	std::string	mutexName;
	mutexName = name;
	mutexName += "_mutex";

	m_hMutex = CreateMutexA(NULL, FALSE, mutexName.c_str());
	if (!m_hMutex) {
		Close();
		return false;
	}

	m_pName = _strdup(name);
	m_size = 0;

	return true;

}

void SpoutSharedMemory::Close()
{
	if (m_pBuffer) {
		UnmapViewOfFile((LPCVOID)m_pBuffer);
		m_pBuffer = NULL;
	}

	if (m_hMap) {
		CloseHandle(m_hMap);
		m_hMap = NULL;
	}

	if (m_hMutex) {
		CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}

	if (m_pName) {
		free((void*)m_pName);
		m_pName = NULL;
	}

}


char* SpoutSharedMemory::Lock()
{
	assert(m_lockCount >= 0);
	assert(m_hMutex);

	if(m_lockCount < 0) {
		return NULL;
	}

	if(!m_hMutex) {
		return NULL;
	}

	if(!m_pBuffer) {
		return NULL;
	}

	if (m_lockCount > 0) {
		assert(m_pBuffer);
		m_lockCount++;
		return m_pBuffer;
	}

	DWORD waitResult = WaitForSingleObject(m_hMutex, 67);
	if (waitResult != WAIT_OBJECT_0) {
		return NULL;
	}

	m_lockCount++;

	assert(m_pBuffer);
	return m_pBuffer;
}

void SpoutSharedMemory::Unlock()
{

	assert(m_hMutex);

	m_lockCount--;
	assert(m_lockCount >= 0);

	if (m_lockCount == 0) {
		ReleaseMutex(m_hMutex);
	}
}


void SpoutSharedMemory::Debug()
{
	/*
	if (m_pName) {
		printf("(%s) m_hMap = [%x], m_pBuffer = [%x]\n", m_pName, m_hMap, m_pBuffer);
	}
	else {
		printf("Shared Memory Map is not open\n");
	}
	*/
}
