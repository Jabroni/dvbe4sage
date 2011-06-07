#include "StdAfx.h"
#include "Decompress.h"

CDecompress::CDecompress(void)
{
}

CDecompress::~CDecompress(void)
{
}

void CDecompress::GetString468A(BYTE *b, int maxLen,char *text)
{
	int i = 0;
	int num=0;
	unsigned char c;
#pragma warning(disable:4310)
	char em_ON = (char)0x86;
	char em_OFF = (char)0x87;
#pragma warning(default:4310)
	bool threeByteEncoding = false; 

  if (maxLen< 1) return;
  if (text==NULL) return;
  if (b==NULL) return;

  int len=maxLen;
	do
	{
		c = (char)b[i];

		// If first byte is 0x10 use three byte encoding (0x10 0x00 0xXX)
    if ( i == 0 && (BYTE)c == 0x10 )
    {
			threeByteEncoding = true;
      text[num] = 0x10;
      text[num+1]=0;
      num++;
      goto cont;
    }
    if ( i == 1 && threeByteEncoding == true )
    {
			//NOTE: using 0x00 will mess up the string, so i use 0x10  as a dummy value instead
      //      this value will be ignored by the DvbTextConverter.Convert() method anyway
      text[num] = 0x10;
      text[num+1]=0;
      num++;
      goto cont;
    }
    if ( i == 2 && threeByteEncoding == true )
    {
			// if this value is invalid, the DvbTextConverter.Convert() method will fall back to default charset
      text[num] = c;
            
      text[num+1]=0;
      num++;
      goto cont;
    } 

		if ( (((BYTE)c) >= 0x80) && (((BYTE)c) <= 0x9F))
		{
			goto cont;
		}
		// GEMX: 11.04.08: This strips off a probably encoding pointer byte !!!
		/*if (i==0 && ((BYTE)c) < 0x20)
		{
			goto cont;
		}*/
				
		if (c == em_ON)
		{
			//					
			goto cont;
		}
		if (c == em_OFF)
		{
			//					
			goto cont;
		}
				
		if ( ((BYTE)c) == 0x84)
		{
      if (num >=maxLen) return;
			text[num] = '\r';
			text[num+1]=0;
			num++;
			goto cont;
		}
				
		if (((BYTE)c) < 0x20)
		{
      //0x1-0x5 = choose character set...
      if ((BYTE)c < 0x1 || (BYTE)c>0x5)
      {
			  goto cont;
      }
		}
				
      
    if (num >=maxLen) return;
		text[num] = c;
		text[num+1]=0;
		num++;
cont:
		len -= 1;
		i += 1;
	}while (!(len <= 0));

}
