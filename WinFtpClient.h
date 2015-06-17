
/*
 * WinFtpClient.h
 *
 */

#ifndef WINFTPCLIENT_H_
#define WINFTPCLIENT_H_


#include <string>
#include <list>

#define FTP_COMMAND_PORT 21
#define FTP_DATA_PORT 1090

#define FILE_DEL_RDONLY 0x20
#define SD_BOTH 0x02

/*
 * Custom FTP client using raw commands.
 *
 */
class WinFtpClient
{
public:
   WinFtpClient (const std::string& strServerIpAddress,
                   const std::string& strLogin,
                   const std::string& strPassword);

   virtual ~WinFtpClient();

   virtual bool Connect ();
   virtual void Disconnect ();
   virtual bool IsConnected () const;
   virtual bool Login ();
   virtual bool SendFile (const std::string& strSourceFile, const std::string& strTargetPath, unsigned long ulOffset);
   virtual bool SetDirectory (std::string const &);

   virtual void SetParameters  (const std::string& ip, const std::string& login, const std::string& pwd);
   virtual bool FileExists (const std::string&);
   virtual bool GetFile  (const std::string& strSourcePath, const std::string& strTargetPath, bool );
   virtual std::list<std::string> GetFileList (const std::string& strSourcePath);

protected:
   bool SendCommand(const std::string& strCommand);
   bool CheckExpectedResponse(const std::string& response, const std::string& expected);
   int PassiveMode();
   bool ResumeUpload(const std::string& targetFile, int offset);
   int ParsePortPassive(const std::string& pasvAnswer);
   SOCKET GetSocket(const std::string& strIpAddress, ushort_t usPort);
   int ReceiveAnswer(SOCKET socket, char* const strBuffer, int iLength);
   int SendBuffer(SOCKET socket, const std::string& strBuffer, int iStillToSend);

private:
   std::string m_strIpAddress;
   std::string m_strLogin;
   std::string m_strPassword;
   unsigned long m_ulOffset;
   bool m_bIsConnected;
   SOCKET m_csCommand, m_csData;

   // Private functions
   template <typename T>
   std::string BuildCommand(const std::string& strCommand, const T& strParams);

};


#endif /* WINFTPCLIENT_H_ */
