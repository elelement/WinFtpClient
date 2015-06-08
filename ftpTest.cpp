#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include "wininet.h"
#include "windows.h"

#include "WinFtpClient.h"

int main()
{
   printf("Comienza el programa\n");

   BoMyFtpTransfer transfer("127.0.0.1", "ftpuser", "ftpuser");
   if(transfer.Connect())
   {
      if(transfer.SendFile("c:/ftp_local/local/origen.txt", "./local/destino.txt", 32))
      {
          printf("Sent ok!\n");
      }
      transfer.Disconnect();
   }

   printf("End of program\n");
}
