/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_PLATFORM_LOCALIO_H__
#define __INCLUDED_PLATFORM_LOCALIO_H__

#include <string>

#include "bbs/keycodes.h"
#include "core/file.h"
// This C++ class should encompass all Local Input/Output from The BBS.
// You should use a routine in here instead of using printf, puts, etc.

class WStatus;
class WSession;

class LocalIO {
 public:
  // Constructor/Destructor
  LocalIO();
  LocalIO(const LocalIO& copy) = delete;
  virtual ~LocalIO();

  static constexpr int cursorNone = 0;
  static constexpr int cursorNormal = 1;
  static constexpr int cursorSolid = 2;

  static constexpr int topdataNone = 0;
  static constexpr int topdataSystem = 1;
  static constexpr int topdataUser = 2;

  void SetChatReason(const std::string& chat_reason) { m_chatReason = chat_reason; }
  void ClearChatReason() { m_chatReason.clear(); }

  const int GetTopLine() const { return m_nTopLine; }
  void SetTopLine(int nTopLine) { m_nTopLine = nTopLine; }

  const int GetScreenBottom() const { return m_nScreenBottom; }
  void SetScreenBottom(int nScreenBottom) { m_nScreenBottom = nScreenBottom; }

  void SetSysopAlert(bool b) { m_bSysopAlert = b; }
  const bool GetSysopAlert() const { return m_bSysopAlert; }
  
  virtual void LocalGotoXY(int x, int y) = 0;
  virtual int  WhereX() = 0;
  virtual int  WhereY() = 0;
  virtual void LocalLf() = 0;
  virtual void LocalCr() = 0;
  virtual void LocalCls() = 0;
  virtual void LocalClrEol() = 0;
  virtual void LocalBackspace() = 0;
  virtual void LocalPutchRaw(unsigned char ch) = 0;
  // Overridden by TestLocalIO in tests.
  virtual void LocalPutch(unsigned char ch) = 0;
  virtual void LocalPuts(const std::string& text) = 0;
  virtual void LocalXYPuts(int x, int y, const std::string& text) = 0;
  virtual int  LocalPrintf(const char *formatted_text, ...) = 0;
  virtual int  LocalXYPrintf(int x, int y, const char *formatted_text, ...) = 0;
  virtual int  LocalXYAPrintf(int x, int y, int nAttribute, const char *formatted_text, ...) = 0;
  virtual void set_protect(int l) = 0;
  virtual void savescreen() = 0;
  virtual void restorescreen() = 0;
  virtual void skey(char ch) = 0;
  virtual void tleft(bool bCheckForTimeOut) = 0;
  virtual void UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) = 0;
  virtual bool LocalKeyPressed() = 0;
  virtual unsigned char LocalGetChar() = 0;
  virtual void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) = 0;
  /*
   * MakeLocalWindow makes a "shadowized" window with the upper-left hand corner at
   * (x,y), and the lower-right corner at (x+xlen,y+ylen).
   */
  virtual void MakeLocalWindow(int x, int y, int xlen, int ylen) = 0;
  virtual void SetCursor(int cursorStyle) = 0;
  virtual void LocalWriteScreenBuffer(const char *buffer) = 0;
  virtual int  GetDefaultScreenBottom() = 0;
  virtual void LocalEditLine(char *s, int len, int status, int *returncode, char *ss) = 0;
  virtual void UpdateNativeTitleBar() = 0;

private:
  virtual void LocalFastPuts(const std::string &text) = 0;

private:
  std::string m_chatReason;
  bool m_bSysopAlert;
  int m_nTopLine;
  int m_nScreenBottom;
};


#endif // __INCLUDED_PLATFORM_LOCALIO_H__
