/**
 *  @copyright Copyright 2016 The J-PET Framework Authors. All rights reserved.
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may find a copy of the License in the LICENCE file.
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  @file JPetOptionsGenerator.cpp
 */

#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
#include <cassert>

#include "JPetOptionsGenerator.h"
#include "./JPetCommonTools/JPetCommonTools.h"
#include "./JPetLoggerInclude.h"
#include "./JPetScopeConfigParser/JPetScopeConfigParser.h"
#include "./JPetOptionValidator/JPetOptionValidator.h"
#include "./JPetOptionsGenerator/JPetOptionsGeneratorTools.h"

using boost::any_cast;
using namespace std;
using namespace jpet_options_tools;
using namespace jpet_options_generator_tools;


JPetOptionsGenerator::OptsForFiles JPetOptionsGenerator::generateOptionsForTasks(const OptsStrAny& inOptions,  int nbOfRegisteredTasks)
{
  using namespace jpet_options_tools;
  OptsForFiles  optsForAllFiles;

  if (nbOfRegisteredTasks <= 0) {
    ERROR("nbOfRegisteredTasks <= 0");
    return optsForAllFiles;
  }

  auto options(inOptions);
  auto files = getInputFiles(options);
  if (files.empty()) {
    ERROR("no input files defined");
    return optsForAllFiles;
  }
  ///Now generate set of options for every file
  std::map<std::string, OptsStrAny> optionsPerFile;

  /// In case of scope there is one special input file
  /// which is a json config file which must be parsed.
  /// Based on its content the set of input directories are generated.
  /// The input directories contain data files.
  /// The config input file name also should be stored in a special option field.
  if (any_cast<std::string>(getOptionValue(options, "type_std::string")) == "scope") {
    assert(files.size() == 1); /// there should be only file which is config.
    auto configFileName = files.front();
    options["scopeConfigFile_std::string"] =  configFileName;
    JPetScopeConfigParser scopeConfigParser;
    /// The scope module must use a fake input file name which will be used to
    /// produce the correct output file names by the following modules.
    /// At the same time, the input directory with true input files must be
    /// also added. The container of pairs <directory, fileName> is generated
    /// based on the content of the configuration file.
    JPetScopeConfigParser::DirFileContainer dirsAndFiles = scopeConfigParser.getInputDirectoriesAndFakeInputFiles(configFileName);
    for (const auto& dirAndFile : dirsAndFiles) {
      options["scopeInputDirectory_std::string"] = dirAndFile.first;
      options["inputFile_std::string"] = dirAndFile.second;
      optionsPerFile[dirAndFile.second] = options;
    }
  } else {
    for (const auto& file : files) {
      options["inputFile_std::string"] = file;
      optionsPerFile[file] = options;
    }
  }
  assert(nbOfRegisteredTasks > 0);

  return optionsPerFile;
}

JPetOptionsGenerator::OptsStrAny JPetOptionsGenerator::generateAndValidateOptions(const po::variables_map& cmdLineArgs)
{
  OptsStrAny options;

  if (cmdLineArgs.empty()) {
    ERROR("cmdLineArgs are empty");
    return options;
  }

  options = addTypeSuffixes(transformToStrAnyMap(cmdLineArgs));
  auto cfgFileName = getConfigFileName(options);
  if (!cfgFileName.empty()) {
    addNewOptionsFromCfgFile(cfgFileName, options);
  }
  auto optionNames = getOptionsToBeValidated(options);

  options = addMissingDefaultOptions(options);

  auto transformationMap = generateTransformationMap(options);
  options = transformOptions(transformationMap, options);

  JPetOptionValidator validator;
  if (!validator.areCorrectOptions(options, optionNames)) {
    throw std::invalid_argument("Wrong user options provided! Check the log!");
  }
  return options;
}

std::vector<std::string> JPetOptionsGenerator::getOptionsToBeValidated(const std::map<std::string, boost::any>& optionsMap) const
{
  std::vector<std::string> optionNames;
  /// add standard validation options
  for (auto& opt : optionsMap) {
    optionNames.push_back(opt.first);
  }
  /// add non-standard validation options
  optionNames.push_back("type_std::string, file_std::vector<std::string>");
  return optionNames;
}
