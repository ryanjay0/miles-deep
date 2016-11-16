/*
 * Created by Ryan Jay 15.11.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#ifndef UTIL_HPP
#define UTIL_HPP

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace std;


float scoreMax(vector<float> x);
int scoreArgMax(vector<float> x);
string getFileName(const string& s);
string getFileExtension(const string& s);
string getBaseName(const string& s);
bool queryYesNo();
string PrettyTime(int seconds);
string getDirectory(const string& path);

#endif
