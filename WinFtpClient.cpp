/*
 * WinFtpClient.cpp
 *
 *  Created on: 01/06/2015
 */

#include "WinFtpClient.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <vector>

using namespace std;

/**
 * Default constructor: replaces WinFtpClient + subsequent call to SetParameters
 */
WinFtpClient::WinFtpClient(const string& strServerIpAddress,
  const string& strLogin, const string& strPassword)
: BoTransfer    (     )
, m_strIpAddress(strServerIpAddress)
, m_strLogin    (strLogin)
, m_strPassword (strPassword)
, m_bIsConnected(false)
, m_ulOffset    (0    )
{
   m_csCommand = INVALID_SOCKET;
   m_csData = INVALID_SOCKET;
}

/**
 * Default destructor. Closes all open connections, if possible.
 */
WinFtpClient::~WinFtpClient()
{
   Disconnect();
}

/*
 * Public functions
 */
bool WinFtpClient::Connect()
{
   printf("%s() ftp://%s@%s\n", __FUNCTION__, m_strLogin.c_str(), m_strIpAddress.c_str());

   /* Test configuration */
   if( m_bIsConnected )
   {
      printf("%s() : Already connected\n", __FUNCTION__);
      return m_bIsConnected;
   }

   if( m_strIpAddress.empty() )
   {
      printf("%s() : ERROR - Bad configuration or not ready\n", __FUNCTION__);
      return false;
   }

   m_bIsConnected = true;

   // Startup WinSock
   WORD wVersionRequested;
   WSADATA wsaData;
   wVersionRequested = MAKEWORD(2, 2);
   if(WSAStartup(wVersionRequested, &wsaData) != 0)
   {
      printf("WSAStartup error\n");
      return false;
   }

   // Create the commnad socket
   m_csCommand = GetSocket(m_strIpAddress, FTP_COMMAND_PORT);

   if (m_csCommand == INVALID_SOCKET)
   {
      printf("%s() Socket error: %ld\n", __FUNCTION__, WSAGetLastError());
      if (m_csCommand == WSAEISCONN)
      {
         printf("Already connected\n");
      }
      m_bIsConnected = false;
   }
   else
   {
      // Read response
      char strConn[256] = {0};
      ReceiveAnswer(strConn, 256);
      strConn[255] = 0;
      if(!CheckExpectedResponse(strConn, "220"))
      {
         closesocket(m_csCommand);
         printf("%s() Closing command socket. Unexpected server response: %s\n", __FUNCTION__, strConn);
         m_bIsConnected = false;
      }
      else
      {
         // Force login if user and password are not empty
         if(!m_strLogin.empty() && !m_strPassword.empty()
               && !Login())
         {
            m_bIsConnected = false;
            closesocket(m_csCommand);
            printf("%s() Closing command socket. Login failed: %ld\n", __FUNCTION__, WSAGetLastError());
         }
      }
   }


   return m_bIsConnected;
}


void WinFtpClient::Disconnect()
{
   printf("%s()\n", __FUNCTION__);

   if( shutdown(m_csCommand, SD_BOTH) == SOCKET_ERROR )
   {
      printf("m_csCommand shutdown failed: %d\n", WSAGetLastError());
   }

   if ( shutdown(m_csData, SD_BOTH) == SOCKET_ERROR )
   {
      printf("m_csData shutdown failed: %d\n", WSAGetLastError());
   }

   WSACleanup();

   // Set flag to false
   m_bIsConnected = false;
}

bool WinFtpClient::Login()
{
   printf("Login\n");

   SendCommand("USER " + m_strLogin + "\r\n");
   Sleep(250);
   char strUser[256] = {0};
   ReceiveAnswer(strUser, 256);
   strUser[255] = 0;
   // TODO poner reintentos
   if (!CheckExpectedResponse(strUser, "331")) {
      printf("USER failed! -- \"%s\"\n", strUser);
      return false;
   }

   SendCommand("PASS " + m_strPassword + "\r\n");
   Sleep(250);
   char strPass[256] = {0};
   ReceiveAnswer(strPass, 256);
   strPass[255] = 0;

   int retry = 0;
   if (!CheckExpectedResponse(strPass, "230")) {
      printf("PASS failed! -- \"%s\"\n", strPass);
      return false;
   }

   printf("\"%s\"\n", strPass);

   return true;
}

bool WinFtpClient::IsConnected() const
{
   return m_bIsConnected;
}

void WinFtpClient::SetParameters  (const std::string& strIpAddress, const std::string& strLogin, const std::string& strPassword)
{
   m_strIpAddress = strIpAddress;
   m_strLogin = strLogin;
   m_strPassword = strPassword;
}

bool WinFtpClient::FileExists (const std::string&)
{
   return true;
}

// TODO
bool WinFtpClient::MoveFile (const std::string& strSourcePath, const std::string& strTargetPath       )
{
   return true;
}

bool WinFtpClient::GetFile  (const std::string& strSourcePath, const std::string& strTargetPath, bool )
{
   return true;
}

bool WinFtpClient::RetrFile (const std::string& strSource, const std::string& strTarget, bool bRemoveSource)
{
   return true;
}

list<std::string> WinFtpClient::GetFileList (const std::string& strSourcePath)
{
   std::list<std::string> l;
   return l;
}


bool WinFtpClient::SetDirectory(string const & destination)
{
   const string& cwd = BuildCommand<string>("CWD", destination);
   if(! SendCommand(cwd))
   {
      printf("Error while sending command sequence: %d\n", WSAGetLastError());
      return false;
   }

   char strBuffer[256];
   ReceiveAnswer(strBuffer, 256);
   return (CheckExpectedResponse(strBuffer, "250"));
}

bool WinFtpClient::SendFile(const string& strSourceFile, const string& strTargetPath, ulong_t ulOffset)
{
   printf("%s() --> Begin\n", __FUNCTION__);

   string strDir = strTargetPath.substr(0, strTargetPath.find_last_of('/'));
   string strTargetFilename = strTargetPath.substr(strTargetPath.find_last_of('/') + 1,
         strTargetPath.size() - 1);

   /*
   *  Get text from strSourceFile starting at ulOffset.
   */
   string buffer = "";
   ifstream myfile (strSourceFile.c_str(), std::ifstream::in);

   if (myfile && myfile.is_open())
   {
      myfile.seekg (0, ios::end); // Move position to ulOffset
      int fsize = myfile.tellg();

      if(ulOffset < fsize)
      {
         myfile.seekg (ulOffset, ios::beg); // Move position to offset
         buffer.resize(fsize - ulOffset); // Allocate memory
         myfile.read(&buffer[0], buffer.size()); // Read the contents from offset to end
         myfile.close();
      }
      else
      {
         printf("%s() Offset value is greater than the file size itself (%lu > %lu)\n", __FUNCTION__, ulOffset, fsize);
         return false;
      }

   }
   else
   {
      printf("%s(): Error opening source file = %\n", __FUNCTION__, strSourceFile.c_str());
      return false;
   }

   /* Set destination directory */
   if(!SetDirectory(strDir))
   {
       printf("%s(): Destination directory not found = %\n", __FUNCTION__, strSourceFile.c_str());
       return false;
   }

   /* Start passive mode */
   char strBuffer[256];
   int port = PassiveMode();
   if(port == 0)
   {
      printf("%s() Couldn't determine connection port number for data interchange\n", __FUNCTION__);
      printf("%s\n", strBuffer);
      return false;
   }

   printf("%s() Using port %d\n", __FUNCTION__, port);

   /* Resume upload if proceed */
   if(!ResumeUpload(strTargetFilename, ulOffset))
   {
      printf("Error while sending REST + STOR sequence: %d\n", WSAGetLastError());
   }

   printf("%s() Resuming uploading at %d\n", __FUNCTION__, ulOffset);

   m_csData = GetSocket(m_strIpAddress, port);
   if (m_csData == INVALID_SOCKET)
   {
     printf("%s() Error connecting: %d\n", __FUNCTION__, WSAGetLastError());
     return false;
   }

   printf("\nSending...\n");
   // int result = send(m_csData, buffer.c_str(), buffer.size(), 0) != SOCKET_ERROR ;
   bool bResult = SendBuffer(m_csData, buffer, buffer.size()) == 0;
   closesocket(m_csData);
   printf("%s() closed socket: %d\n", __FUNCTION__, WSAGetLastError());


   // Check that the server updated correctly the target file
   ReceiveAnswer(strBuffer, 256);

   if(! (bResult &= CheckExpectedResponse(strBuffer, "226")) )
   {
      printf("Unexpected server response: %s\n", strBuffer);
   }

   return bResult;
}


/*
 * Protected functions
 */
bool WinFtpClient::ResumeUpload(const string& targetFile, int offset)
{
   // Build rest command
   const string& rest = BuildCommand<int>("REST", offset);
   if(!SendCommand(rest))
   {
      printf("REST command failed!\n");
      return false;
   }
   char strBuffer[256];
   ReceiveAnswer(strBuffer, 256);

   if(!CheckExpectedResponse(strBuffer, "350"))
   {
      printf("Unexpected server response: %s\n", strBuffer);
      return false;
   }

   // Stor command
   const string& stor = BuildCommand<string>("STOR", targetFile);
   if(!SendCommand(stor))
   {
      printf("STOR command failed!\n");
      return false;
   }

   ReceiveAnswer(strBuffer, 256);

   return (CheckExpectedResponse(strBuffer, "150"));
}

int WinFtpClient::PassiveMode()
{
   // Set type as binary/image
   if(!SendCommand("TYPE I\r\n"))
   {
      printf("TYPE command failed!\n");
      return 0;
   }
   char strBuffer[256];
   ReceiveAnswer(strBuffer, 256);

   if(!CheckExpectedResponse(strBuffer, "200"))
   {
      printf("Unexpected server response: %s\n", strBuffer);
      return 0;
   }

   // Proceed with passive mode
   if(!SendCommand("PASV\r\n"))
   {
      printf("PASV command failed!\n");
      return 0;
   }

   // Get port for passive mode
   ReceiveAnswer(strBuffer, 256);
   if(!CheckExpectedResponse(strBuffer, "227"))
   {
      printf("Unexpected server response: %s\n", strBuffer);
      return 0;
   }

   printf("Passive answer: %s\n", strBuffer);

   return ParsePortPassive(strBuffer);
}

bool WinFtpClient::SendCommand(const string& strCommand)
{
   int iResult;
   // Send an initial buffer
   iResult = send(m_csCommand, strCommand.c_str(), (int) strCommand.size(), 0);
   return iResult != SOCKET_ERROR;
}

bool WinFtpClient::ReceiveAnswer(char* const strBuffer, int iLength)
{
   // Clean the array before use
   memset(&strBuffer[0], 0, iLength);

   int iResult = -1;
   // Send an initial buffer
   iResult = recv(m_csCommand, strBuffer, iLength, 0);

   if (iResult == SOCKET_ERROR) {
      printf("Answer failed: %d\n", WSAGetLastError());
      return false;
   }

   printf("Bytes received: %d\n", iResult);

   return true;
}

bool WinFtpClient::CheckExpectedResponse(const string& response, const string& expected)
{
   std::istringstream f(response);
   std::string line;
   std::string before;
   while (std::getline(f, line) && !f.eof())
   {
      before = line;
   }
   return (before.find(expected.c_str(), 0) != std::string::npos);
}
int WinFtpClient::ParsePortPassive(const string& pasvAnswer)
{
   std::istringstream f(pasvAnswer);
   std::string line;
   while (std::getline(f, line)
      && !(line.find("227", 0) != std::string::npos));

   if(line.empty())
   {
      return 0;
   }
   vector<std::string> elems;
   std::stringstream ss(line.substr(0, line.find(")")));
   std::string item;
   int count = 0;
   while (std::getline(ss, item, ',')) {
      elems.push_back(item);
   }

   int p1, p2 = 0;
   p1 = atoi(elems.at(4).c_str());
   p2 = atoi(elems.at(5).c_str());

   int port = p1 * 256 + p2;

   return port;
}

SOCKET WinFtpClient::GetSocket(const string& strIpAddress, ushort_t usPort)
{
   //Fill out the information needed to initialize a socket…
    SOCKADDR_IN target; //Socket address information

    target.sin_family = AF_INET; // address family Internet
    target.sin_port = htons (usPort); //Port to connect on
    target.sin_addr.s_addr = inet_addr (strIpAddress.c_str()); //Target IP

    SOCKET mySocket = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP); //Create socket
    if (mySocket != INVALID_SOCKET)
    {
       //Try connecting...
       connect(mySocket, (SOCKADDR *)&target, sizeof(target));
    }

   return mySocket;
}

/*
 * Private functions
 */
template <typename T>
string WinFtpClient::BuildCommand(const string& strCommand, const T& strParams)
{
   std::stringstream ss;
   ss << strCommand << " " << strParams << "\r\n";
   return (ss.str());
}

int WinFtpClient::SendBuffer(SOCKET socket, const string& strBuffer, int iStillToSend)
{
  int iRC = 0;
  int iSendStatus = 0;
  timeval SendTimeout;

  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(socket, &fds);

  // Set timeout
  SendTimeout.tv_sec  = 0;
  SendTimeout.tv_usec = 250000;              // 250 ms

  // As long as we need to send bytes...
  char *pBuffer = new char[strBuffer.size()];
  memcpy(pBuffer, strBuffer.c_str(), strBuffer.size());
  while(iStillToSend > 0)
  {
    iRC = select(0, NULL, &fds, NULL, &SendTimeout);

    // Timeout
    if(!iRC)
      return -1;

    // Error
    if(iRC < 0)
      return WSAGetLastError();

    // Send some bytes
    iSendStatus = send(socket, pBuffer, iStillToSend, 0);

    // Error
    if(iSendStatus < 0)
      return WSAGetLastError();
    else
    {
      // Update buffer and counter
      iStillToSend -= iSendStatus;
      pBuffer += iSendStatus;
    }
  }

  if(pBuffer) delete[] pBuffer;
  return 0;
}




