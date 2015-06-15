/*
 * WinFtpClient.cpp
 *
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
: m_strIpAddress(strServerIpAddress)
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
      ReceiveAnswer(m_csCommand, strConn, 256);
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
   ReceiveAnswer(m_csCommand, strUser, 256);
   strUser[255] = 0;
   // TODO poner reintentos
   if (!CheckExpectedResponse(strUser, "331")) {
      printf("USER failed! -- \"%s\"\n", strUser);
      return false;
   }

   SendCommand("PASS " + m_strPassword + "\r\n");
   Sleep(250);
   char strPass[256] = {0};
   ReceiveAnswer(m_csCommand, strPass, 256);
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

bool WinFtpClient::GetFile  (const std::string& strSourcePath, const std::string& strTargetPath, bool )
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
   ReceiveAnswer(m_csCommand, strBuffer, 256);
   return (CheckExpectedResponse(strBuffer, "250"));
}

bool WinFtpClient::SendFile(const string& strSourceFile, const string& strTargetPath, unsigned long ulOffset)
{
   printf("%s() \n", __FUNCTION__);
   string strPath = strTargetPath;
   if(strPath.substr(0, 1) .compare("/") != 0)
   {
      strPath = "/" + strPath; // slash symbol
   }

   printf("%s()\n\tsource file = %s\n\tttarget path = %s\n", __FUNCTION__, strSourceFile.c_str(), strPath.c_str());

   string strDir = strPath.substr(0, strPath.find_last_of('/'));
   string strTargetFilename = strPath.substr(strPath.find_last_of('/') + 1,
         strPath.size() - 1);

   /*
   *  Get text from strSourceFile starting at ulOffset.
   */
   string buffer = "";
   ifstream myfile (strSourceFile.c_str(), std::ifstream::in);

   if (!myfile || !myfile.is_open())
   {
      printf("%s(): Error opening source file = %s\n", __FUNCTION__, strSourceFile.c_str());
      return false;
   }

   myfile.seekg (0, ios::end); // Move position to ulOffset
   ulong_t fsize = myfile.tellg();

   if(ulOffset >= fsize)
   {
      printf("%s() Offset value is greater than the file size itself (%lu > %lu)\n", __FUNCTION__, ulOffset, fsize);
      return false;
   }

   myfile.seekg (ulOffset, ios::beg); // Move position to offset
   buffer.resize(fsize - ulOffset); // Allocate memory
   myfile.read(&buffer[0], buffer.size()); // Read the contents from offset to end
   myfile.close();

   /* Set destination directory */
   if(!SetDirectory(strDir))
   {
       printf("%s(): Destination directory not found = %\n", __FUNCTION__, strSourceFile.c_str());
       return false;
   }

   /* Start passive mode */
   char strBuffer[256];
   int port = PassiveMode();
   if(port <= 0)
   {
      printf("%s() Couldn't determine connection port number for data interchange\n", __FUNCTION__);
      return false;
   }

   printf("%s() Using port %d\n", __FUNCTION__, port);

   /* Resume upload if proceed */
   if(!ResumeUpload(strTargetFilename, ulOffset))
   {
      printf("%s() Error while sending REST + STOR sequence: %d\n", __FUNCTION__, WSAGetLastError());
      return false;
   }
   printf("%s() Resuming uploading at %d\n", __FUNCTION__, ulOffset);

   m_csData = GetSocket(m_strIpAddress, port);
   if (m_csData == INVALID_SOCKET)
   {
     printf("%s() Error connecting: %d\n", __FUNCTION__, WSAGetLastError());
     return false;
   }

   // If the data connection has successfully been created then, the server will answer
   // with 150 code.

   Sleep(250);
   ReceiveAnswer(m_csCommand, strBuffer, 256);

   printf("%s() buffer = %s\n", __FUNCTION__, strBuffer);
   if (! CheckExpectedResponse(strBuffer, "150"))
   {
      printf("%s() Unexpected server response: %s\n", __FUNCTION__, strBuffer);
      return false;
   }

   printf("%s() Sending: %s\n", __FUNCTION__, buffer.c_str());
   int result = SendBuffer(m_csData, buffer, buffer.size());

   // Close data socket
   closesocket(m_csData);
   printf("%s() Closed socket: %d\n", __FUNCTION__, WSAGetLastError());

   // And check it's what we expected
   if(result != 0)
   {
      printf("%s() Sending data failed: %d\n", __FUNCTION__, result);
      return false;
   }

   // Check that the server updated correctly the target file
   result = ReceiveAnswer(m_csCommand, strBuffer, 256);

   if(! CheckExpectedResponse(strBuffer, "226"))
   {
      printf("%s() Unexpected server response: %s\n", __FUNCTION__, strBuffer);
      return false;
   }

   return true;
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

   return true;
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

bool WinFtpClient::CheckExpectedResponse(const string& response, const string& expected)
{
   printf("%s()\n", __FUNCTION__);
   std::istringstream f(response);
   std::string line;
   while (std::getline(f, line) && !f.eof())
   {
      if(line.find(expected.c_str(), 0) != std::string::npos)
      {
         return true;
      }
   }
   return false;
}

bool WinFtpClient::SendCommand(const string& strCommand)
{
   Debug("%s() Command: %s\n", __FUNCTION__, strCommand.c_str());
   int iResult;
   // Send an initial buffer
   iResult = SendBuffer(m_csCommand, strCommand, strCommand.size());
   return iResult != SOCKET_ERROR;
}

int WinFtpClient::ReceiveAnswer(SOCKET socket, char* const strBuffer, int iLength)
{
   int iRC = 0;
   int iSendStatus = 0;
   timeval RecvTimeout;

   fd_set fds;

   FD_ZERO(&fds);
   FD_SET(socket, &fds);

   // Set timeout
   RecvTimeout.tv_sec  = 0;
   RecvTimeout.tv_usec = 500000;              // 500 ms

   iRC = select(0, &fds, NULL, NULL, &RecvTimeout);

   // Timeout
   if(!iRC)
   {
      printf("%s() Timeout reading\n", __FUNCTION__);
      return 0;
   }

   // Error
   if(iRC < 0)
   {
      printf("%s() Error reading: %ld\n", __FUNCTION__, WSAGetLastError());
      return -1;
   }

   int iResult = -1;

   // Clean the array before use
   memset(&strBuffer[0], 0, iLength);

   if (recv(socket, strBuffer, iLength, 0) == SOCKET_ERROR)
   {
      printf("Answer failed: %d\n", WSAGetLastError());
      return -2;
   }

   return iLength;
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
  char *pCopyPointer = pBuffer;
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

  if(pCopyPointer) delete[] pCopyPointer;
  return 0;
}

