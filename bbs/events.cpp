/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/events.h"

#include <algorithm>

#include "bbs/bbsutl1.h"
#include "bbs/bbsovl3.h"
#include "bbs/datetime.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/execexternal.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/netsup.h"
#include "bbs/printfile.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/wfc.h"
#include "core/file.h"
#include "core/datafile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

using wwiv::bbs::InputMode;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Local Functions

void sort_events();
void show_events();
void select_event_days(int evnt);
void modify_event(int evnt);
void insert_event();
void delete_event(int n);

static int16_t t_now() {
  time_t t = time(nullptr);
  struct tm* pTm = localtime(&t);
  return static_cast<int16_t>((pTm->tm_hour * 60) + pTm->tm_min);
}

static char *ttc(int d) {
  static char ch[7];

  sprintf(ch, "%02d:%02d", d / 60, d % 60);
  return ch;
}

static void write_events() {
  if (session()->events.empty()) {
    File eventsFile(session()->config()->datadir(), EVENTS_DAT);
    eventsFile.Delete();
    return;
  }
  DataFile<eventsrec> file(session()->config()->datadir(), EVENTS_DAT,
    File::modeBinary | File::modeReadWrite | File::modeCreateFile);
  file.WriteVector(session()->events);
}

static void write_event(int n) {
  DataFile<eventsrec> file(session()->config()->datadir(), EVENTS_DAT,
    File::modeBinary | File::modeReadWrite | File::modeCreateFile);
  file.Write(n, &session()->events[n]);
}

static void read_events() {
  session()->events.clear();

  DataFile<eventsrec> file(session()->config()->datadir(), EVENTS_DAT);
  if (!file) {
    return;
  }
  file.ReadVector(session()->events);
  if (!session()->events.empty()) {
    get_next_forced_event();
  }
}

static void read_event(int n) {
  DataFile<eventsrec> file(session()->config()->datadir(), EVENTS_DAT);
  if (!file) {
    return;
  }
  file.Read(n, &session()->events[n]);
}

void sort_events() {
  // keeping events sorted in time order makes things easier.
  for (size_t i = 0; i < (session()->events.size() - 1); i++) {
    size_t z = i;
    for (size_t j = (i + 1); j < session()->events.size(); j++) {
      if (session()->events[j].time < session()->events[z].time) {
        z = j;
      }
    }
    if (z != i) {
      std::swap(session()->events[i], session()->events[z]);
    }
  }
}

void init_events() {
  read_events();
}

void get_next_forced_event() {
  syscfg.executetime = 0;
  time_event = 0;
  int first = -1;
  int16_t tl = t_now();
  int day = dow() + 1;
  if (day == 7) {
    day = 0;
  }
  for (const auto& e : session()->events) {
    if ((e.instance == session()->instance_number() || e.instance == 0) && e.status & EVENT_FORCED) {
      if (first < 0 && e.time < tl && ((e.days & (1 << day)) > 0)) {
        first = e.time;
      }
      if ((e.status & EVENT_RUNTODAY) == 0 && (e.days & (1 << dow())) > 0 && !syscfg.executetime) {
        time_event = static_cast<long>(e.time) * SECONDS_PER_MINUTE;
        syscfg.executetime = e.time;
        if (!syscfg.executetime) {
          ++syscfg.executetime;
        }
      }
    }
  }
  if (first >= 0 && !syscfg.executetime) {
    // all of todays events are
    time_event = static_cast<long>(first) * SECONDS_PER_MINUTE;     // complete, set next forced
    syscfg.executetime = static_cast<uint16_t>(first);                // event to first one
    if (!syscfg.executetime) {                                              // scheduled for tomorrow
      ++syscfg.executetime;
    }
  }
}

void cleanup_events() {
  if (session()->events.empty()) {
    return;
  }

  // since the date has changed, make sure all events for yesterday have been
  // run, then clear all status to "not run" note in this case all events end up
  // running on the same node, but this is preferable to not running at all
  int day = dow() - 1;
  if (day < 0) {
    day = 6;
  }

  for (size_t i = 0; i < session()->events.size(); i++) {
    auto& e = session()->events[i];

    if (((e.status & EVENT_RUNTODAY) == 0) &&
        ((e.days & (1 << day)) > 0)) {
      run_event(i);
    }
  }
  for (auto& e : session()->events) {
    e.status &= ~EVENT_RUNTODAY;
    // zero out last run in case this is a periodic event
    e.lastrun = 0;
  }

  write_events();
}

void check_event() {
  int16_t tl = t_now();
  for (size_t i = 0; i < session()->events.size() && !do_event; i++) {
    auto& e = session()->events[i];
    if (((e.status & EVENT_RUNTODAY) == 0) && (e.time <= tl) &&
        ((e.days & (1 << dow())) > 0) &&
        ((e.instance == session()->instance_number()) ||
         (e.instance == 0))) {
      // make sure the event hasn't already been executed on another node,then mark it as run
      read_event(i);

      if ((e.status & EVENT_RUNTODAY) == 0) {
        e.status |= EVENT_RUNTODAY;
        write_event(i);
        do_event = i + 1;
      }
    } else if ((e.status & EVENT_PERIODIC) &&
               ((e.days & (1 << dow())) > 0) &&
               ((e.instance == session()->instance_number()) ||
                (e.instance == 0))) {
      // periodic events run after N minutes from last execution.
      int16_t nextrun = ((e.lastrun == 0) ? e.time : e.lastrun) + e.period;
      // The next run time should be the later of "now", or the next scheduled time.
      // This should keep events from executing numerous times to "catch up".
      nextrun = std::max(tl, nextrun);
      // if the next runtime is before now trigger it to run
      if (nextrun <= tl) {
        // flag the event to run
        read_event(i);

        // make sure other nodes didn't run it already
        if ((((e.lastrun == 0) ? e.time : e.lastrun) + e.period) <= tl) {
          e.status |= EVENT_RUNTODAY;
          // record that we ran it now.
          write_event(i);
          do_event = i + 1;
        }
      }
    }
  }
}

void run_event(int evnt) {
  auto& e = session()->events[evnt];

  write_inst(INST_LOC_EVENT, 0, INST_FLAGS_NONE);
  session()->localIO()->SetCursor(LocalIO::cursorNormal);
  bout.cls();
  bout << "\r\nNow running external event.\r\n\n";
  if (session()->events[evnt].status & EVENT_EXIT) {
    int exitlevel = static_cast<int>(e.cmd[0]);
    if (ok_modem_stuff && session()->remoteIO() != nullptr) {
      session()->remoteIO()->close(false);
    }
    session()->ExitBBSImpl(exitlevel, true);
  }
  ExecuteExternalProgram(session()->events[evnt].cmd, EFLAG_NONE);
  do_event = 0;
  get_next_forced_event();
  cleanup_net();
  wfc_cls();

  e.lastrun = t_now();
  write_event(evnt);
}

void show_events() {
  char s[121] = {}, s1[81] = {}, daystr[8] = {};

  bout.cls();
  bool abort = false;
  char y = "Yes"[0];
  char n = "No"[0];
  pla("|#1                                         Hold   Force   Run            Run", &abort);
  pla("|#1Evnt Time  Command                 Node  Phone  Event  Today   Freq    Days", &abort);
  pla("|#7=============================================================================", &abort);
  for (size_t i = 0; (i < session()->events.size()) && !abort; i++) {
    auto& e = session()->events[i];
    if (e.status & EVENT_EXIT) {
      sprintf(s1, "Exit Level = %d", e.cmd[0]);
    } else {
      strncpy(s1, e.cmd, sizeof(s1));
    }
    strcpy(daystr, "SMTWTFS");
    for (int j = 0; j <= 6; j++) {
      if ((e.days & (1 << j)) == 0) {
        daystr[j] = ' ';
      }
    }
    if (e.status & EVENT_PERIODIC) {
      if (e.status & EVENT_RUNTODAY) {
        sprintf(s, " %2d  %-5.5s %-23.23s  %2d     %1c      %1c    %-5.5s    %2dm   %s",
                i, ttc(e.time), s1, e.instance,
                ' ',
                e.status & EVENT_FORCED ? y : n,
                ttc(e.lastrun),
                e.period,
                daystr);
      } else {
        sprintf(s, " %2d  %-5.5s %-23.23s  %2d     %1c      %1c      %1c      %2dm   %s",
                i, ttc(e.time), s1, e.instance,
                ' ',
                e.status & EVENT_FORCED ? y : n,
                n,
                e.period,
                daystr);
      }
    } else {
      sprintf(s, " %2d  %-5.5s %-23.23s  %2d     %1c      %1c      %1c            %s",
              i, ttc(e.time), s1, e.instance,
              ' ',
              e.status & EVENT_FORCED ? y : n,
              e.status & EVENT_RUNTODAY ? y : n,
              daystr);
    }
    pla(s, &abort);
  }
}

void select_event_days(int evnt) {
  char ch, daystr[9], days[8];

  bout.nl();
  strcpy(days, "SMTWTFS");
  for (int i = 0; i <= 6; i++) {
    if (session()->events[evnt].days & (1 << i)) {
      daystr[i] = days[i];
    } else {
      daystr[i] = ' ';
    }
  }
  daystr[8] = '\0';
  bout << "Enter number to toggle day of the week, 'Q' to quit.\r\n\n";
  bout << "                   1234567\r\n";
  bout << "Days to run event: ";
  do {
    bout << daystr;
    ch = onek_ncr("1234567Q");
    if ((ch >= '1') && (ch <= '7')) {
      int i = ch - '1';
      session()->events[evnt].days ^= (1 << i);
      if (session()->events[evnt].days & (1 << i)) {
        daystr[i] = days[i];
      } else {
        daystr[i] = ' ';
      }
      bout << "\b\b\b\b\b\b\b";
    }
  } while (ch != 'Q' && !hangup);
}

void modify_event(int evnt) {
  char s[81], s1[81], ch;
  int j;

  bool ok     = true;
  bool done   = false;
  int i       = evnt;
  do {
    bout.cls();
    auto& e = session()->events[i];

    bout << "A) Event Time......: " << ttc(e.time) << wwiv::endl;
    if (e.status & EVENT_EXIT) {
      sprintf(s1, "Exit BBS with DOS Errorlevel %d", e.cmd[0]);
    } else {
      strcpy(s1, e.cmd);
    }
    bout << "B) Event Command...: " << s1 << wwiv::endl;
    bout << "D) Already Run?....: " << ((e.status & EVENT_RUNTODAY) ? "Yes" : "No") << wwiv::endl;
    bout << "E) Shrink?.........: " << ((e.status & EVENT_SHRINK) ? "Yes" : "No") << wwiv::endl;
    bout << "F) Force User Off?.: " << ((e.status & EVENT_FORCED) ? "Yes" : "No") << wwiv::endl;
    strcpy(s1, "SMTWTFS");
    for (j = 0; j <= 6; j++) {
      if ((e.days & (1 << j)) == 0) {
        s1[j] = ' ';
      }
    }
    bout << "G) Days to Execute.: " << s1 << wwiv::endl;
    bout << "H) Node (0=Any)....: " << e.instance << wwiv::endl;
    bout << "I) Periodic........: " << ((e.status & EVENT_PERIODIC) ? "Yes" : "No");
    if (e.status & EVENT_PERIODIC) {
      bout << " (every " << std::to_string(e.period) << " minutes)";
    }
    bout << wwiv::endl;
    bout.nl();
    bout << "|#5Which? |#7[|#1A-I,[,],Q=Quit|#7] |#0: ";
    ch = onek("QABCDEFGHI[]");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case ']':
      i++;
      if (i >= size_int(session()->events)) {
        i = 0;
      }
      break;
    case '[':
      i--;
      if (i < 0) {
        i = session()->events.size() - 1;
      }
      break;
    case 'A':
      bout.nl();
      bout << "|#2Enter event times in 24 hour format. i.e. 00:01 or 15:20\r\n";
      bout << "|#2Event time? ";
      ok = true;
      j = 0;
      do {
        if (j == 2) {
          s[j++] = ':';
          bout.bputch(':');
        } else {
          switch (j) {
          case 0:
            ch = onek_ncr("012\r");
            break;
          case 3:
            ch = onek_ncr("012345\b");
            break;
          case 5:
            ch = onek_ncr("\b\r");
            break;
          case 1:
            if (s[0] == '2') {
              ch = onek_ncr("0123\b");
              break;
            }
          default:
            ch = onek_ncr("0123456789\b");
            break;
          }
          if (hangup) {
            ok = false;
            s[0] = '\0';
            break;
          }
          switch (ch) {
          case '\r':
            switch (j) {
            case 0:
              ok = false;
              break;
            case 5:
              s[5] = '\0';
              break;
            default:
              ch = 0;
              break;
            }
            break;
          case '\b':
            bout << " \b";
            --j;
            if (j == 2) {
              bout.bs();
              --j;
            }
            break;
          default:
            s[j++] = ch;
            break;
          }
        }
      } while (ch != '\r' && !hangup);
      if (ok) {
        e.time = static_cast<int16_t>((60 * atoi(s)) + atoi(&(s[3])));
      }
      break;
    case 'B':
      bout.nl();
      bout << "|#2Exit BBS for event? ";
      if (yesno()) {
        e.status |= EVENT_EXIT;
        bout << "|#2DOS ERRORLEVEL on exit? ";
        input(s, 3);
        j = atoi(s);
        if (s[0] != 0 && j >= 0 && j < 256) {
          e.cmd[0] = static_cast<char>(j);
        }
      } else {
        e.status &= ~EVENT_EXIT;
        bout << "|#2Commandline to run? ";
        input(s, 80);
        if (s[0] != '\0') {
          strcpy(e.cmd, s);
        }
      }
      break;
    case 'D':
      e.status ^= EVENT_RUNTODAY;
      // reset it in case it is periodic
      e.lastrun = 0;
      break;
    case 'E':
      e.status ^= EVENT_SHRINK;
      break;
    case 'F':
      e.status ^= EVENT_FORCED;
      break;
    case 'G':
      bout.nl();
      bout << "|#2Run event every day? ";
      if (noyes()) {
        e.days = 127;
      } else {
        select_event_days(i);
      }
      break;
    case 'H':
      bout.nl();
      bout << "|#2Run event on which node (0=any)? ";
      input(s, 3);
      j = atoi(s);
      if (s[0] != '\0' && j >= 0 && j < 1000) {
        e.instance = static_cast<int16_t>(j);
      }
      break;
    case 'I':
      e.status ^= EVENT_PERIODIC;
      if (e.status & EVENT_PERIODIC) {
        bout.nl();
        bout << "|#2Run again after how many minutes (0=never, max=240)? ";
        input(s, 4);
        j = atoi(s);
        if (s[0] != '\0' && j >= 1 && j <= 240) {
          e.period = static_cast<int16_t>(j);
        } else {
          // user entered invalid time period, disable periodic
          e.status &= ~EVENT_PERIODIC;
          e.period = 0;
        }
      }
      break;
    }
  } while (!done && !hangup);
}

void insert_event() {
  eventsrec e = {};
  strcpy(e.cmd, "**New Event**");
  e.time = 0;
  e.status = 0;
  e.instance = 0;
  e.days = 127;                // Default to all 7 days
  session()->events.emplace_back(e);
  modify_event(session()->events.size() - 1);
}

void delete_event(int n) {
  erase_at(session()->events, n);
}

void eventedit() {
  char s[81];

  if (!ValidateSysopPassword()) {
    return;
  }
  bool done = false;
  do {
    char ch = 0;
    show_events();
    bout.nl();
    bout <<
                       "|#9Events: |#1I|#9nsert, |#1D|#9elete, |#1M|#9odify, e|#1X|#9ecute, |#1Q|#9uit :";
    if (so()) {
      ch = onek("QDIM?X");
    } else {
      ch = onek("QDIM?");
    }
    switch (ch) {
    case '?':
      show_events();
      break;
    case 'Q':
      done = true;
      break;
    case 'X': {
      bout.nl();
      bout << "|#2Run which Event? ";
      input(s, 2);
      unsigned int nEventNum = StringToUnsignedInt(s);
      if (s[0] != '\0' && nEventNum >= 0 && nEventNum < session()->events.size()) {
        run_event(nEventNum);
      }
    }
    break;
    case 'M': {
      bout.nl();
      bout << "|#2Modify which Event? ";
      input(s, 2);
      unsigned int nEventNum = StringToUnsignedInt(s);
      if (s[0] != '\0' && nEventNum >= 0 && nEventNum < session()->events.size()) {
        modify_event(nEventNum);
      }
    }
    break;
    case 'I':
      if (session()->events.size() < MAX_EVENT) {
        insert_event();
      } else {
        bout << "\r\n|#6Can't add any more events!\r\n\n";
        pausescr();
      }
      break;
    case 'D':
      if (!session()->events.empty()) {
        bout.nl();
        bout << "|#2Delete which Event? ";
        input(s, 2);
        unsigned int nEventNum = StringToUnsignedInt(s);
        if (s[0] && nEventNum >= 0 && nEventNum < session()->events.size()) {
          bout.nl();
          if (session()->events[nEventNum].status & EVENT_EXIT) {
            sprintf(s, "Exit Level = %d", session()->events[nEventNum].cmd[0]);
          } else {
            strcpy(s, session()->events[nEventNum].cmd);
          }
          bout << "|#5Delete " << s << "?";
          if (yesno()) {
            delete_event(nEventNum);
          }
        }
      } else {
        bout << "\r\n|#6No events to delete!\r\n\n";
        pausescr();
      }
      break;
    }
  } while (!done && !hangup);
  sort_events();
  write_events();
}
