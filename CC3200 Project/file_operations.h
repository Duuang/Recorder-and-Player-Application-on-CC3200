//*****************************************************************************
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************


//*****************************************************************************
//
// Application Name     -   File operations 
// Application Overview -   This example demonstate the file operations that 
//                          can be performed by the applications. The 
//                          application use the serial-flash as the storage 
//                          medium.
// Application Details  -
// http://processors.wiki.ti.com/index.php/CC32xx_File_Operations
// or
// docs\examples\CC32xx_File_Operations.pdf
//
//*****************************************************************************


//*****************************************************************************
//
//! \addtogroup filesystem_demo
//! @{
//
//*****************************************************************************


#define SL_MAX_FILE_SIZE        200L*1024L       /* 200KB file */
#define BUF_SIZE                2048
#define USER_FILE_NAME          "aaaa.txt"

/* Application specific status/error codes */
//typedef enum{  in main.c
typedef enum{
    // Choosing this number to avoid overlap w/ host-driver's error codes
    FILE_ALREADY_EXIST = -0x7D0,
    FILE_CLOSE_ERROR = FILE_ALREADY_EXIST - 1,
    FILE_NOT_MATCHED = FILE_CLOSE_ERROR - 1,
    FILE_OPEN_READ_FAILED = FILE_NOT_MATCHED - 1,
    FILE_OPEN_WRITE_FAILED = FILE_OPEN_READ_FAILED -1,
    FILE_READ_FAILED = FILE_OPEN_WRITE_FAILED - 1,
    FILE_WRITE_FAILED = FILE_READ_FAILED - 1,

    //STATUS_CODE_MAX = -0xBB8
}e_AppStatusCodes_file;


//*****************************************************************************
//                 GLOBAL VARIABLES -- Start
//*****************************************************************************
//unsigned char gaucCmpBuf[BUF_SIZE];

extern int Report(const char *pcFormat, ...);

//*****************************************************************************
//
//!  This funtion includes the following steps:
//!  -open a user file for writing
//!  -write "Old MacDonalds" child song 37 times to get just below a 64KB file
//!  -close the user file
//!
//!  /param[out] ulToken : file token
//!  /param[out] lFileHandle : file handle
//!
//!  /return  0:Success, -ve: failure
//
//*****************************************************************************
long ReadFileFromDevice(unsigned long ulToken, long lFileHandle)
{
    long lRetVal = -1;
    int iLoopCnt = 0;

    //
    // open a user file for reading
    //
    lRetVal = sl_FsOpen((unsigned char *)"aaaa.txt",
                        FS_MODE_OPEN_READ,
                        &ulToken,
                        &lFileHandle);
    if(lRetVal < 0)
    {
        lRetVal = sl_FsClose(lFileHandle, 0, 0, 0);
        ASSERT_ON_ERROR(FILE_OPEN_READ_FAILED);
    }
    //
    // read the data , and send the data via uart to PC.  use program on pc to receive data
    //
    //TODO: use tcp to send data via wlan (problems...)
    unsigned char gaucCmpBuf[10];  // buffer
    int blocknum;
    for (blocknum = 0; blocknum < 1; blocknum++) {
		for (iLoopCnt = 0; iLoopCnt < 2000; iLoopCnt++) {  // send by block not used
			lRetVal = sl_FsRead(lFileHandle,
						(unsigned int)((blocknum * 100 + iLoopCnt) * sizeof(gaucCmpBuf)),
						 gaucCmpBuf, sizeof(gaucCmpBuf));
			if ((lRetVal < 0) || (lRetVal != sizeof(gaucCmpBuf)))
			{
				lRetVal = sl_FsClose(lFileHandle, 0, 0, 0);
				ASSERT_ON_ERROR(FILE_READ_FAILED);
			}

			Report("%s", gaucCmpBuf);
			//osi_Sleep(50);
		}
		//ClearTerm();
    }
    //
    // close the user file
    //
    lRetVal = sl_FsClose(lFileHandle, 0, 0, 0);
    if (SL_RET_CODE_OK != lRetVal)
    {
        ASSERT_ON_ERROR(FILE_CLOSE_ERROR);
    }

    return SUCCESS;
}
