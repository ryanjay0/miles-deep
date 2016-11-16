/*
 * Created by Ryan Jay 15.11.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <string>

#include "util.hpp"

using namespace std;

float scoreMax(vector<float> x)
{
     return *max_element(x.begin(), x.end());
}

int scoreArgMax(vector<float> x)
{
    return distance(x.begin(), max_element(x.begin(), x.end()));
}

string getFileName(const string& s)
{
    char sep = '/';

    #ifdef _WIN32
        sep = '\\';
    #endif

    size_t i = s.rfind(sep, s.length());
    if( i != string::npos)
        return(s.substr(i+1, s.length() -i));
    return(s);
}

string getFileExtension(const string& s)
{
    char sep = '.';

    size_t i = s.rfind(sep, s.length());
    if( i != string::npos)
        return(s.substr(i, s.length() -i));
    return(s);
}

string getBaseName(const string& s)
{
    char sep = '.';

    size_t i = s.rfind(sep, s.length());
    if( i != string::npos)
        return(s.substr(0, i));
    return(s);
}

bool queryYesNo()
{
    cout << "Replace movie with cut? This will delete the movie. [y/N]?";
    string input;
    getline(cin, input);
    if( input == "YES" || input == "Yes" || input == "yes" 
            || input == "Y" || input == "y")
        return true;
    else
        return false;
}

string PrettyTime(int seconds)
{
    int s, h, m;
    string pTime = "";

    m = (seconds / 60);
    h = int(m / 60)%60;
    m = int(m % 60);
    s = int(seconds%60);

    if(h > 0)
        pTime += to_string(h) + "h";
    if(seconds >= 60)
        pTime += to_string(m) + "m";
    pTime += to_string(s) + "s";
    return(pTime);

}

std::string getDirectory (const std::string& path)
{
    int found = path.find_last_of("/\\");
    if(found < 0)
        return(".");
    else
        return(path.substr(0, found));
}

