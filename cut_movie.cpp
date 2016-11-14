/*
 * Created by Ryan Jay 30.10.16
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

#include "cut_movie.hpp"

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

std::string GetDirectory (const std::string& path)
{
    int found = path.find_last_of("/\\");
    if(found < 0)
        return(".");
    else
        return(path.substr(0, found));
}


void cut( ScoreList score_list, string movie_file, vector<int> target_list, string output_dir, string temp_dir,
            int total_targets, int min_cut, int max_gap, float threshold, float min_coverage)
{

    //path stuff with movie file
    char  sep = '/';
    #ifdef _WIN32
    char  sep = '\\';
    #endif
    string movie_base = getBaseName(getFileName(movie_file));
    string movie_type = getFileExtension(movie_file);
    string cut_movie = movie_base + ".cut";
    string temp_base = temp_dir + sep + "cuts";
    string temp_path = temp_base + sep + cut_movie;
    string movie_directory = GetDirectory(movie_file);

    //will come from input

    vector<int> target_on(total_targets,0);
    for(int i=0; i<target_list.size(); i++)
       target_on[target_list[i]] = 1; 



    string mkdir_command = "mkdir -p " + temp_base;
    if(system(mkdir_command.c_str()))
    {
        cerr << "Error making directory: " << temp_base << endl;
        exit(EXIT_FAILURE);
    }
    
    //init
    CutList cut_list;
    int cut_start = -1;
    int gap = 0;
    int win_sum = 0;
    float val_sum = 0.0;
    bool did_concat = true;


    //find winners and their scores
    vector<int> winners(score_list.size());
    vector<float> vals(score_list.size());

    for( int i=0; i < score_list.size(); i++ ){
        winners[i] = scoreArgMax(score_list[i]);
        vals[i] = scoreMax(score_list[i]);
    }

    //find the cuts
    for( int i=0; i<score_list.size(); i++)
    {
        if( cut_start >= 0 )
        {
            if( target_on[winners[i]] )
            {
                win_sum++;
                val_sum += vals[i];
            }
            
            if( !target_on[winners[i]] || vals[i] < threshold || i == score_list.size()-1 )
            {
                if(i < score_list.size()-1) gap++;

                if( gap > max_gap || i == score_list.size()-1 )
                {
                    if( cut_start < i - gap - min_cut )
                    {
                        //output cut
                        int win_size = (i - gap) - cut_start + 1;
                        float score_avg = val_sum / (float)win_sum;
                        float coverage = (float)win_sum / (float)win_size;

                        if(coverage >= min_coverage)
                        {
                            cout << cut_start << " - " << i - gap << ": size= " 
                                << win_size << " coverage= " << coverage 
                                << " score= " << score_avg << '\n';
                            Cut cut;
                            cut.s = cut_start;
                            cut.e = i-gap;
                            cut_list.push_back(cut);
                        }
                    }
                    cut_start = -1;
                    gap = 0;
                    val_sum = 0.0;
                    win_sum = 0;
                }
            }
            else
            {
                gap = 0;
            }

        }
        else if( target_on[winners[i]] && vals[i] >= threshold )
        {
            cut_start = i;
            gap = 0;
            val_sum = 0.0;
            win_sum = 0.0;
        }
    }


    //make the cuts
    if( cut_list.size() > 0 )
    {
        cout << "Making the cuts" << endl;
        string part_file_path = temp_dir + sep + "cuts.txt";
        ofstream part_file;
        part_file.open(part_file_path.c_str());
        if(!part_file.is_open())
        {
            cout << "Cannot open file for writing: " << part_file_path << endl;
            exit(EXIT_FAILURE);
        }

        
        //use output_seek for wmv. fixed bug where cuts would freeze
        bool output_seek = false;
        if(movie_type == ".wmv" || movie_type == ".WMV" || movie_type == ".Wmv")
        {
            movie_type = ".mkv";
            output_seek = true;
        }


        //output a file for each cut in the list
        for( int i=0; i<cut_list.size(); i++)
        {
    
            Cut this_cut = cut_list[i];

            string part_name = temp_path + '.' + to_string(i) + movie_type;
            cout << "   Creating piece: " << part_name << endl;

            string cut_command;
            if(output_seek)
                cut_command = "ffmpeg -loglevel 8 -y -i \"" + movie_file + "\" -ss " + 
                    to_string(this_cut.s) + " -t " + to_string(this_cut.e - this_cut.s)  +
                    " -c copy \"" + part_name + "\"";
            else       
                cut_command = "ffmpeg -loglevel 8 -y -ss " + to_string(this_cut.s) +
                    " -i \"" + movie_file + "\" -t " + to_string(this_cut.e - this_cut.s)  +
                    " -c copy -avoid_negative_ts 1 \"" + part_name + "\"";

            if(system(cut_command.c_str()))
            {
                cerr << "Error cutting piece : " << part_name << endl;
                exit(EXIT_FAILURE);
            }

            //write piece to cuts.txt as instructions for concatenation
            part_file << "file \'" << part_name << "\'" << endl;
        }
        part_file.close();

        //default behavior (output cut where input movie is located)
        if(output_dir == "")
            output_dir = movie_directory;


        cout << "Concatenating parts in " << part_file_path << endl;
        cout << "Final output: " << output_dir << sep << cut_movie << movie_type << endl;
        
        string concat_command = "ffmpeg -loglevel 16 -f concat -safe 0 -i " + part_file_path + 
            " -c copy \"" + output_dir + sep + cut_movie + movie_type + "\"";
        if(system(concat_command.c_str()))
        {
            cerr << "Didn't concatenate pieces from: " << part_file_path << endl;
            did_concat = false;
        }

    }
    else
    {
        cout << "No cuts found." << endl;
        return;
    }

    
    //ask about removing original and only keeping cut
    if(did_concat && queryYesNo())
    {
        string rm_cmd = "rm -rf \"" + movie_file + "\"";
        if(system(rm_cmd.c_str()))
        {
            cerr << "Error removing input movie: " << rm_cmd << endl;
            exit(EXIT_FAILURE);
        }
    } 


    //clean up cuts directory and cuts.txt file
    string clean_cmd = "rm -rf " + temp_dir + sep + "cuts.txt " + temp_base;
    if(system(clean_cmd.c_str()))
    {
        cerr << "Error cleaning up temporary cut piece files in: " 
                << clean_cmd << endl; 
        exit(EXIT_FAILURE);
    }
}

