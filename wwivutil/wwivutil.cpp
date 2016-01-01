/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
#include "wwivutil/wwivutil.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/stl.h"
#include "sdk/config.h"
#include "wwivutil/messages/messages.h"
#include "wwivutil/net/net.h"

using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::wwivutil;

int main(int argc, char *argv[]) {
  try {
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.AddStandardArgs();

    UtilCommand* messages = new MessagesCommand();
    cmdline.add(messages);
    AddCommandsAndArgs(messages);

    UtilCommand* net= new NetCommand();
    cmdline.add(net);
    AddCommandsAndArgs(net);

    if (!cmdline.Parse()) { return 1; }
    const std::string bbsdir(cmdline.arg("bbsdir").as_string());
    Config config(bbsdir);
    if (!config.IsInitialized()) {
      clog << "Unable to load CONFIG.DAT.";
      return 1;
    }
    std::unique_ptr<Configuration> command_config(
        new Configuration(bbsdir, &config));
    messages->set_config(command_config.get());
    net->set_config(command_config.get());
    return cmdline.Execute();
  } catch (std::exception& e) {
    clog << e.what() << endl;
  }
  return 1;
}
