/* $Id: petsctoolfe.cpp,v 1.1 2001/03/06 23:58:18 buschelm Exp $ */
#include "petscfe.h"
#include <iostream>
#include <string>

using namespace PETScFE;

tool::tool(void) {
  tool::OptionTags= "uv";
  tool::Options['u'] = &tool::FoundUse;
  tool::Options['v'] = &tool::FoundVerbose;

  verbose = 0;
}
  
void tool::GetArgs(int argc,char *argv[]) {
  if (argc>2) { 
    for (int i=1;i<argc;i++) arg.push_back(argv[i]);
    tool::Parse();
    ReplaceSlashWithBackslash(*(arg.begin()));
  } else {
    cout << "Not enough arguments." << endl;
    cout << "Error: 2" << endl;
  }
}

void tool::Parse() {
  LI i = arg.begin();
  while (i!=arg.end()) {
    string temp = *i;
    if (temp.substr(0,2)=="--") {
      char flag = temp[2];
      if (tool::OptionTags.find(flag)!=string::npos) {
        (this->*tool::Options[flag])(i);
      }
    } else {
      i++;
    }
  }
}

void tool::Execute(void) {
 if (verbose) cout << "PETSc Front End" << endl;
}

void tool::FoundUse(LI &i) {
  if (*i=="--use") {
    i = arg.erase(i);
    ReplaceSlashWithBackslash(*i);
    *(arg.begin()) = *i;
    i = arg.erase(i);
  }
}

void tool::FoundVerbose(LI &i) {
  if (*i == "--verbose") {
    verbose = -1;
    i = arg.erase(i);
  }
}

void tool::ReplaceSlashWithBackslash(string &name) {
  for (int i=0;i<name.length();i++)
    if (name[i]=='/') name[i]='\\';
}

void tool::ProtectQuotes(string &name) {
  string::size_type a,b;
  a = name.find("\"");
  if (a!=string::npos) {
    string temp = name.substr(0,a+1);
    temp += "\\\"";
    temp += name.substr(a+1,string::npos);
    name = temp;
    b = name.rfind("\"");
    if (b!=a+2) {
      temp = name.substr(0,b);
      temp += "\\\"";
      temp += name.substr(b,string::npos);
      name = temp;
    }
  }
}

void tool::PrintListString(list<string> &liststr) {
  cout << "Printing..." << endl;
  LI i = liststr.begin();
  while (i!=liststr.end()) cout << *i << " ";
  cout << endl;
}

void tool::Merge(string &str,list<string> &liststr,LI i) {
  while (i!=liststr.end()) {
    str += " " + *i++;
  }
}
