
# New App Wizard for OpenXR Provider v2
# Copyright 2021, 2022, 2023 Rune Berg (GitHub: https://github.com/1runeberg, Twitter: https://twitter.com/1runeberg, YouTube: https://www.youtube.com/@1RuneBerg)
#
# SPDX-License-Identifier: MIT
#
# Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this
#   software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.

import os, sys, shutil

print ("\n=====================================")
print ("New App Wizard for OpenXR Provider v2")
print ("=====================================\n")

# Get app details
sAppName = input("(1) Input name of app - no special characters other than underscore: ")
if not sAppName:
    sys.exit("Error - you must provide a name.")

sLocalizedName = input(f"(2) Input localized app name. Press Enter to use app name [{sAppName}]: ")
if not sLocalizedName:
    sLocalizedName = sAppName

sAppNamespace = input("(3) Input your app's C++ namespace. Press enter to use [oxa]: ")
if not sAppNamespace:
    sAppNamespace = "oxa"

sAppMainClass = input("(4) Input your app's C++ main class name. Press enter to use [CXrApp]: ")
if not sAppMainClass:
    sAppMainClass = "CXrApp"

sTLD = input("(5) Enter top level domain. Press enter to use [net]: ")
if not sTLD:
    sTLD = "net"

sDomainName = input("(6) Enter domain name. Press enter to use [beyondreality]: ")
if not sDomainName:
    sDomainName = "beyondreality"

sTargetDir = input(f"(7) Input target directory name (will also be the executable name) [{sAppName}]: ")
if not sTargetDir:
    sTargetDir = sAppName

# Copy files
shutil.copytree(".", "../" + sTargetDir)

# Rename java package directory
sOutputDir = "../" + sTargetDir + "/"
os.rename(sOutputDir + "java/net", sOutputDir + "java/" + sTLD)
os.rename(sOutputDir + "java/" + sTLD + "/beyondreality", sOutputDir + "java/" + sTLD + "/" + sDomainName)
os.rename(sOutputDir + "java/" + sTLD + "/" + sDomainName + "/YOUR_APP_NAME_HERE", sOutputDir + "java/" + sTLD + "/" + sDomainName + "/" + sAppName)

# Replace app, class and namespace names in pertinent files
def replace_text( sFilename, sFrom, sTo ):
    fin = open(sFilename, "rt")
    data = fin.read()
    data = data.replace(sFrom, sTo)
    fin.close()

    fin = open(sFilename, "wt")
    fin.write(data)
    fin.close()

replace_text(sOutputDir + "CMakeLists.txt", "YOUR_LOCALIZED_APP_NAME_HERE", sLocalizedName)
replace_text(sOutputDir + "build.gradle", "YOUR_LOCALIZED_APP_NAME_HERE", sLocalizedName)
replace_text(sOutputDir + "README.md", "YOUR_LOCALIZED_APP_NAME_HERE", sLocalizedName)

replace_text(sOutputDir + "CMakeLists.txt", "YOUR_APP_NAME_HERE", sAppName)
replace_text(sOutputDir + "project_config.h.in", "YOUR_APP_NAME_HERE", sAppName)
replace_text(sOutputDir + "AndroidManifest.xml", "YOUR_APP_NAME_HERE", sAppName)
replace_text(sOutputDir + "build.gradle", "YOUR_APP_NAME_HERE", sAppName)

replace_text(sOutputDir + "src/main.hpp", "YOUR_APP_NAME_HERE", sAppName)
replace_text(sOutputDir + "src/main.cpp", "YOUR_APP_NAME_HERE", sAppName)
replace_text(sOutputDir + "java/" + sTLD + "/" + sDomainName + "/" + sAppName + "/MainActivity.java", "YOUR_APP_NAME_HERE", sAppName)

replace_text(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.hpp", "YOUR_APP_NAME_HERE", sAppName)
replace_text(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.cpp", "YOUR_APP_NAME_HERE", sAppName)

replace_text(sOutputDir + "src/main.cpp", "YOUR_MAIN_CLASS_HERE", sAppMainClass)
replace_text(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.hpp", "YOUR_MAIN_CLASS_HERE", sAppMainClass)
replace_text(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.cpp", "YOUR_MAIN_CLASS_HERE", sAppMainClass)

replace_text(sOutputDir + "src/main.cpp", "YOUR_NAMESPACE_HERE", sAppNamespace)
replace_text(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.hpp", "YOUR_NAMESPACE_HERE", sAppNamespace)
replace_text(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.cpp", "YOUR_NAMESPACE_HERE", sAppNamespace)

# Rename C++ main class files
os.rename(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.hpp", sOutputDir + "src/" + sAppMainClass + ".hpp")
os.rename(sOutputDir + "src/YOUR_MAIN_CLASS_HERE.cpp", sOutputDir + "src/" + sAppMainClass + ".cpp")

# Replace domain identifiers in pertinent files
replace_text(sOutputDir + "AndroidManifest.xml", "YOUR_TLD_HERE", sTLD)
replace_text(sOutputDir + "build.gradle", "YOUR_TLD_HERE", sTLD)
replace_text(sOutputDir + "multidex-config.pro", "YOUR_TLD_HERE", sTLD)
replace_text(sOutputDir + "java/" + sTLD + "/" + sDomainName + "/" + sAppName + "/MainActivity.java", "YOUR_TLD_HERE", sTLD)

replace_text(sOutputDir + "AndroidManifest.xml", "YOUR_DOMAIN_HERE", sDomainName)
replace_text(sOutputDir + "build.gradle", "YOUR_DOMAIN_HERE", sDomainName)
replace_text(sOutputDir + "multidex-config.pro", "YOUR_DOMAIN_HERE", sDomainName)
replace_text(sOutputDir + "java/" + sTLD + "/" + sDomainName + "/" + sAppName + "/MainActivity.java", "YOUR_DOMAIN_HERE", sDomainName)

# Replace output directory in pertinent files
replace_text(sOutputDir + "CMakeLists.txt", "YOUR_OUTPUT_DIRECTORY_HERE", sTargetDir)
replace_text(sOutputDir + "project_config.h.in", "YOUR_OUTPUT_DIRECTORY_HERE", sTargetDir)
replace_text(sOutputDir + "AndroidManifest.xml", "YOUR_OUTPUT_DIRECTORY_HERE", sTargetDir)
replace_text(sOutputDir + "build.gradle", "YOUR_OUTPUT_DIRECTORY_HERE", sTargetDir)
replace_text(sOutputDir + "java/" + sTLD + "/" + sDomainName + "/" + sAppName + "/MainActivity.java", "YOUR_OUTPUT_DIRECTORY_HERE", sTargetDir)

# Delete last four lines in main CMakeLists file
with open("../CMakeLists.txt", "r+") as fp:
    lines = fp.readlines()
    fp.seek(0)
    fp.truncate()
    fp.writelines(lines[:-4]) # delete last four lines, we'll append it later along with the new project subdir

# Append new project to main CMakeLists file
fin = open("../CMakeLists.txt", "a")

fin.write(f"\n# {sLocalizedName}\n")
fin.write(f"add_subdirectory(\"{sTargetDir}\")\n\n")
fin.write("# Set a sample app as the startup project in VS\n")
fin.write(f"set_property (DIRECTORY PROPERTY VS_STARTUP_PROJECT \"{sTargetDir}\")\n")
fin.write("message(\"[${MAIN_PROJECT}] Startup project for VS set to: sample_10_openxr_workshop\")\n")

fin.close()

# Delete this script from output directory
os.remove(sOutputDir + "new_app_wizard.py")

print(f"App {sAppName} created in {sOutputDir}")
